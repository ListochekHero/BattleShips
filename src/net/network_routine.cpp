#include "network_routine.h"

#include "core/scheduler.h"
#include "net/deferred_actions.h"
#include "net/socket_routine.h"
#include "protocol/message_defs.h"
#include "protocol/message_types.h"
#include "protocol/network_defs.h"
#include "utility/config.h"
#include "utility/error.h"
#include "utility/logger.h"

#include <cstddef>
#include <exception>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <utility>

namespace bsm {

ConnectionEntry::ConnectionEntry(end_point_e type, int socket)
    : connection_type_(type), socket_handler_(SocketHandler(socket)) {}

auto NetworkEngine::init_engine(end_point_e socket_type, int parrent_socket)
    -> std::expected<ConnectionView, Error> {
  std::expected<size_t, Error> socket_slot{};
  if (parrent_socket != 0) {
    socket_slot = socket_pool__.push_to_pool(
        ConnectionEntry{socket_type, parrent_socket});
    socket_slot = add_to_socket_pool(parrent_socket, socket_type);
  } else {
    socket_slot = add_to_socket_pool();
  }
  if (!socket_slot) {
    return std::unexpected(std::move(socket_slot)
                               .error()
                               .add_context("Unable to add socket to pool"));
  }
  switch (socket_type) {
  case end_point_e::NONE:
  case end_point_e::TO_CLIENT:
  case end_point_e::TO_LOBBY:
    break;
  case end_point_e::TO_PARENT:
    socket_pool_[0].connection_type_ = end_point_e::TO_PARENT;
    break;
  case end_point_e::LISTENER:
    if (auto result = socket_pool_[0].socket_handler_.setup_listener(
            Config::instance().get_line("port"));
        !result) {
      return std::unexpected(
          std::move(result).error().add_context("Cant setup listener"));
    }
    socket_pool_[0].connection_type_ = end_point_e::LISTENER;
    break;
  case end_point_e::TO_SERVER:
    if (auto result = socket_pool_[0].socket_handler_.setup_client(); !result) {
      return std::unexpected(
          std::move(result).error().add_context("Cant setup client"));
    }
    socket_pool_[0].connection_type_ = end_point_e::TO_SERVER;
    break;
  }
  if (auto result = init_epoll(); !result) {
    return std::unexpected(
        std::move(result).error().add_context("Unable to init epoll"));
  }
  return ConnectionView{*socket_slot};
}

void NetworkEngine::set_message_handler(MessageHandler msg_handler) {
  on_message_callback_ = std::move(msg_handler);
}

void NetworkEngine::run() {
  while (true) {
    auto wait_result = epoll_handler_.wait_for_events(MAX_EVENTS);
    if (!wait_result) {
      LOG("Cant get events");
      LOG(wait_result.error().full_report());
    } else {
      process_events(*wait_result);
    }
    deferred_actions_.flush(*this);
  }
}

auto NetworkEngine::send_message_to(const ConnectionView& conn_view,
                                    const OutgoingMessage& message) -> bool {

  return send_message_impl(socket_pool_[conn_view.get_slot()], message);
}

auto NetworkEngine::attach_socket(int socket, end_point_e socket_type)
    -> std::expected<ConnectionView, Error> {
  auto slot_index = register_client(socket, socket_type);
  if (!slot_index) {
    return std::unexpected(slot_index.error().add_context(
        "Unable to attach new socket to net_engine"));
  }
  return ConnectionView{*slot_index};
}

auto NetworkEngine::transfer(const ConnectionView& dest_view,
                             const ConnectionView& src_view) -> CommandStatus {
  ConnectionEntry& dest_entry{socket_pool_[dest_view.get_slot()]};
  ConnectionEntry& src_entry{socket_pool_[src_view.get_slot()]};
  unsubscribe_from_events(src_entry);
  if (auto result = dest_entry.socket_handler_.write_to_user({
          .payloads = {"\\socket"},
          .msg_type = message_type_e::SOCKET,
          .socket = src_entry.socket_handler_.get_socket(),
      });
      !result) {
    LOG(result.error().full_report());
    LOG("Unable to transfer socket, trying to recover...");
    if (auto result = subscribe_to_events(src_entry); !result) {
      LOG(result.error().full_report());
      LOG("Error while trying to recover socket, closing connection");
      deferred_actions_.schedule(
          std::make_unique<Cleanup_Connection>(src_entry.occupied_slot_),
          *this);
      return {
          .command_status_v = cmd_se::TERMINATE,
          .error = Error{.backtrace = {"Transfer failed, connection lost"}},
      };
    }
    return {
        .command_status_v = cmd_se::CONTINUE,
        .error = Error{.backtrace = {"Transfer failed, connection preserved"}},
    };
  }
  deferred_actions_.schedule(
      std::make_unique<Cleanup_Connection>(src_entry.occupied_slot_), *this);
  return {.command_status_v = cmd_se::TERMINATE};
}

void NetworkEngine::process_client(size_t slot) { process_client_socket(slot); }

auto NetworkEngine::init_epoll() -> Ev {
  return this->epoll_handler_.init()
      .transform_error([](auto&& error) -> auto {
        return error.add_context("Cant init epoll_handler");
      })
      .and_then([this]() -> Ev {
        return this->epoll_handler_
            .add_socket(socket_pool_[0].socket_handler_, 0)
            .transform_error([](auto&& error) -> auto {
              return error.add_context("Cant add server socket to epoll");
            });
      });
}

auto NetworkEngine::init_epoll_wrapper() -> Ev {
  return init_epoll().transform_error([](auto&& error) -> auto {
    return error.add_context("Unable to init epoll");
  });
}

template <typename... Args>
auto NetworkEngine::emplace_new_entry_to_pool(Args&&... args)
    -> std::expected<size_t, Error> {
  try {
    size_t occupied_slot{socket_pool_.size()};
    socket_pool_.emplace_back(std::forward<Args>(args)..., occupied_slot);
    return occupied_slot;
  } catch (const std::exception& e) {
    LOG(e.what());
    return std::unexpected(
        Error{.backtrace = {"Unable to emplace new socket handler"}});
  }
}

auto NetworkEngine::add_to_socket_pool() -> std::expected<size_t, Error> {
  return emplace_new_entry_to_pool(std::make_unique<SocketHandler>(),
                                   end_point_e::NONE)
      .transform_error([](auto&& error) {
        return error.add_context("Unable to increase socket pool size");
      });
}

auto NetworkEngine::add_to_socket_pool(int socket_fd, end_point_e socket_type)
    -> std::expected<size_t, Error> {
  return emplace_new_entry_to_pool(std::make_unique<SocketHandler>(socket_fd),
                                   socket_type)
      .transform_error([](auto&& error) {
        error.add_context("Unable to add given socket to socket pool");
        return error;
      });
}

auto NetworkEngine::free_slot_entry(ConnectionEntry& slot_entry) -> Ev {
  try {
    slot_entry.socket_handler_.reset_to_empty();
    auto* new_slot = new size_t(slot_entry.occupied_slot_);
    avaiable_slots_.push(new_slot);
  } catch (const std::exception& e) {
    return std::unexpected(Error{
        .backtrace = {e.what(), "Exception caught during freeing slot entry"},
    });
  }
  return {};
}

auto NetworkEngine::find_spot_for_new_client(int client_socket,
                                             end_point_e socket_type)
    -> std::expected<size_t, Error> {
  size_t* slot = avaiable_slots_.try_pop();
  if (slot != nullptr) {
    ConnectionEntry* entry{&socket_pool_[*slot]};
    entry->socket_handler_.reset_with_new(client_socket);
    entry->connection_type_ = socket_type;
    return *slot;
  }
  auto new_entry_slot = add_to_socket_pool(client_socket, socket_type);
  if (!new_entry_slot) {
    return std::unexpected(
        std::move(new_entry_slot)
            .error()
            .add_context("Unable to find spot for new client"));
  }
  return new_entry_slot;
}

auto NetworkEngine::subscribe_to_events(ConnectionEntry& slot_entry) -> Ev {
  return epoll_handler_
      .add_socket(slot_entry.socket_handler_, slot_entry.occupied_slot_)
      .or_else([&](auto&& error) -> Ev {
        return std::unexpected(
            error.add_context("Unable to subscribe new client to epoll"));
      });
}

void NetworkEngine::unsubscribe_from_events(ConnectionEntry& slot_entry) {
  drop_result(epoll_handler_.remove_socket(slot_entry.socket_handler_)
                  .or_else([](auto&& error) -> Ev {
                    LOG(error.full_report());
                    return {};
                  }));
}

void NetworkEngine::release_client(ConnectionEntry& slot_entry) {
  unsubscribe_from_events(slot_entry);
  success_or_terminate(free_slot_entry(slot_entry));
}

auto NetworkEngine::register_client(int client_socket, end_point_e socket_type)
    -> std::expected<size_t, Error> {
  auto new_client_slot = find_spot_for_new_client(client_socket, socket_type);
  if (!new_client_slot) {
    return std::unexpected(std::move(new_client_slot)
                               .error()
                               .add_context("Unable to register new client"));
  }
  ConnectionEntry& new_client_entry{socket_pool_[*new_client_slot]};
  return subscribe_to_events(new_client_entry)
      .transform([&]() { return new_client_entry.occupied_slot_; })
      .or_else([&](auto&& error) -> std::expected<size_t, Error> {
        LOG(error.full_report());
        send_message_impl(new_client_entry,
                          {
                              .payloads = {user_message(us_e::GENERIC)},
                              .msg_type = message_type_e::PRINTABLE,
                          });
        success_or_terminate(free_slot_entry(new_client_entry));
        return std::unexpected(
            Error{.backtrace = {"Unable to add new client to epoll"}});
      });
}

void NetworkEngine::register_clients(std::vector<int>& new_clients) {
  for (int client : new_clients) {
    if (auto result = register_client(client, end_point_e::TO_CLIENT);
        !result) {
      LOG(result.error().full_report());
    }
  }
}

void NetworkEngine::process_events(const std::vector<size_t>& event_slots) {
  for (size_t slot : event_slots) {
    if (socket_pool_[slot].connection_type_ == end_point_e::LISTENER) {
      process_server_socket(slot);
    } else {
      network_raw_tasks_.push(new size_t(slot));
      available_task_tags_.push(new task_tag_e(task_tag_e::NETWORK));
    }
  }
}

void NetworkEngine::process_server_socket(size_t slot) {
  drop_result(socket_pool_[slot]
                  .socket_handler_.accept_connections()
                  .and_then([this](auto&& new_clients) -> Ev {
                    register_clients(new_clients);
                    return {};
                  })
                  .or_else([](auto&& error) -> Ev {
                    LOG(error.full_report());
                    LOG("Cant accept new connections");
                    return {};
                  }));
  epoll_handler_.rearm_socket((socket_pool_[slot].socket_handler_),
                              socket_pool_[slot].occupied_slot_);
}

void NetworkEngine::process_client_socket(size_t slot) {
  CommandStatus cmd_status{.command_status_v = cmd_se::CONTINUE};
  while (cmd_status.command_status_v == cmd_se::CONTINUE &&
         socket_pool_[slot].socket_handler_.get_socket_status() ==
             socket_status_e::ALIVE) {
    ConnectionEntry& entry = socket_pool_[slot];
    auto read_result = entry.socket_handler_.read_user_input();
    if (!read_result) {
      LOG(read_result.error().full_report());
      break;
    }
    cmd_status = process_message(entry, *read_result);
  }
}

auto NetworkEngine::process_message(ConnectionEntry& slot_entry,
                                    ReadResult& message) -> CommandStatus {
  switch (message.status) {
  case bsm::message_status_e::EMPTY:
  case bsm::message_status_e::WOULDBLOCK:
    epoll_handler_.rearm_socket((slot_entry.socket_handler_),
                                slot_entry.occupied_slot_);
    return {.command_status_v = cmd_se::TERMINATE};
  case message_status_e::DISCONNECTED:
    release_client(slot_entry);
    return {.command_status_v = command_status_e::TERMINATE};
  case bsm::message_status_e::DATA:
    break;
  }
  ConnectionView connection{slot_entry.occupied_slot_};
  CommandContext context{.client_view = connection, .message = message};
  CommandStatus cmd_status = on_message_callback_(
      context); //<-CallBack to Server, process user message
  if (cmd_status.error) {
    LOG("Unable to handle client command: " + context.message.payload);
    LOG(cmd_status.error->full_report());
    send_message_impl(slot_entry,
                      {
                          .payloads = {user_message(*cmd_status.user_code)},
                          .msg_type = message_type_e::PRINTABLE,
                      });
  }
  return cmd_status;
}

auto NetworkEngine::send_message_impl(ConnectionEntry& slot_entry,
                                      const OutgoingMessage& message) -> bool {
  if (auto write_result = slot_entry.socket_handler_.write_to_user(message);
      !write_result) {
    LOG("Unable to write message into socket");
    LOG(write_result.error().full_report());
    deferred_actions_.schedule(
        std::make_unique<Cleanup_Connection>(slot_entry.occupied_slot_), *this);
    return false;
  }
  return true;
}

void NetworkEngine::Cleanup_Connection::prepare(NetworkEngine& engine) {
  std::scoped_lock lock(engine.m_);
  ConnectionEntry& entry{engine.socket_pool_[slot_]};
  entry.generation_++;
  entry.connection_type_ = end_point_e::NONE;
  entry.socket_handler_.set_socket_status(socket_status_e::CLOSED);
}

void NetworkEngine::Cleanup_Connection::execute(NetworkEngine& engine) {
  std::scoped_lock lock(engine.m_);
  engine.release_client(engine.socket_pool_[slot_]);
}

} // namespace bsm
