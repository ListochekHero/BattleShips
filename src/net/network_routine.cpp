#include "network_routine.h"

#include "core/object_pool.h"
#include "core/scheduler.h"
#include "net/deferred_actions.h"
#include "net/socket_routine.h"
#include "protocol/message_defs.h"
#include "protocol/message_types.h"
#include "protocol/network_defs.h"
#include "protocol/network_types.h"
#include "utility/config.h"
#include "utility/error.h"
#include "utility/logger.h"
#include "utility/scope_guard.h"
#include "utility/utility.h"

#include <cstddef>
#include <exception>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <utility>

namespace bsm {

// public:
ConnectionEntry::ConnectionEntry(end_point_e type, int socket)
    : connection_type_(type), socket_handler_(SocketHandler(socket)) {}

ConnectionEntry::ConnectionEntry(ConnectionEntry&& other) noexcept {
  connection_type_ = other.connection_type_;
  socket_handler_ = std::move(other).socket_handler_;
}
auto ConnectionEntry::reset() -> void {
  connection_type_ = end_point_e::NONE;
  socket_handler_.reset_to_empty();
  generation_++;
}

auto ConnectionEntry::operator=(ConnectionEntry&& other) noexcept
    -> ConnectionEntry& {
  connection_type_ = other.connection_type_;
  socket_handler_ = std::move(other).socket_handler_;
  return *this;
}

auto NetworkEngine::init_engine(end_point_e socket_type, int root_socket)
    -> std::expected<ConnectionView, Error> {
  std::expected<size_t, Error> root_socket_slot{};
  if (root_socket != 0) {
    root_socket_slot = connection_pool_.push_to_pool(
        ConnectionEntry{socket_type, root_socket});
  } else {
    root_socket_slot = connection_pool_.push_to_pool(ConnectionEntry{});
  }
  if (!root_socket_slot) {
    return std::unexpected(
        std::move(root_socket_slot)
                               .error()
            .add_context("Unable to init NetworkEngine: failed to push socket "
                         "to connection pool"));
  }
  ConnectionEntry* root_connection{
      connection_pool_.get_object(*root_socket_slot),
  };
  switch (socket_type) {
  case end_point_e::NONE:
  case end_point_e::TO_CLIENT:
  case end_point_e::TO_LOBBY:
    break;
  case end_point_e::TO_PARENT:
    root_connection->connection_type_ = end_point_e::TO_PARENT;
    break;
  case end_point_e::LISTENER:
    if (auto setup_error = root_connection->socket_handler_.setup_listener(
            Config::instance().get_line("port"));
        setup_error) {
      return std::unexpected(
          std::move(setup_error)
              ->add_context("Unable to init NetworkEngine: failed to setup "
                            "listener socket for server"));
    }
    root_connection->connection_type_ = end_point_e::LISTENER;
    break;
  case end_point_e::TO_SERVER:
    if (auto error = root_connection->socket_handler_.setup_client(); error) {
      return std::unexpected(std::move(error)->add_context(
          "Unable to init NetworkEngine: failed to setup client socket"));
    }
    root_connection->connection_type_ = end_point_e::TO_SERVER;
    break;
  }
  if (auto error = init_epoll(); error) {
    return std::unexpected(std::move(error)->add_context(
        "Unable to init NetworkEngine: failed to init epoll"));
  }
  return ConnectionView{*root_socket_slot};
}

void NetworkEngine::set_message_handler(MessageHandler message_handler) {
  on_message_callback_ = std::move(message_handler);
}

void NetworkEngine::run_event_loop() {
  while (true) {
    auto wait_result =
        epoll_handler_.wait_for_events(MAX_EVENTS); // add this to Config
    if (!wait_result) {
      LOG("Error in run_event_loop");
      LOG(wait_result.error().full_report());
    } else {
      process_events(*wait_result);
    }
  }
}

auto NetworkEngine::send_message_to(const ConnectionView& view,
                                    const OutgoingMessage& message) -> bool {
  auto slot{view.get_slot()};
  auto& entry{*socket_pool_.get_object(slot)};
  bool delivered{send_message_impl(entry, message)};
  if (!delivered) {
    socket_pool_.release(slot);
  }
  return delivered;
}

auto NetworkEngine::attach_socket(int socket, end_point_e socket_type)
    -> std::expected<ConnectionView, Error> {
  auto slot_index = register_client(socket_type, socket);
  if (!slot_index) {
    return std::unexpected(slot_index.error().add_context(
        "Unable to attach new socket to net_engine"));
  }
  return ConnectionView{*slot_index};
}

auto NetworkEngine::transfer(const ConnectionView& destination_view,
                             const ConnectionView& source_view)
    -> CommandStatus {
  auto& destination_conn{*socket_pool_.get_object(destination_view.get_slot())};
  size_t source_slot{source_view.get_slot()};
  auto& source_conn{*socket_pool_.get_object(source_slot)};
  auto source_conn_guard{
      scope_guard([&source_conn, this, source_slot]() -> auto {
        source_conn.reset();
        socket_pool_.release(source_slot);
      }),
  };
  unsubscribe_from_events(source_conn);
  if (auto result = destination_conn.socket_handler_.write_to_user({
          .payloads = {"\\socket"},
          .msg_type = message_type_e::SOCKET,
          .socket = source_conn.socket_handler_.get_socket(),
      });
      !result) {
    LOG(result.error().full_report());
    LOG("Unable to transfer socket, trying to recover...");
    if (auto result = subscribe_to_events(source_conn, source_slot); !result) {
      LOG(result.error().full_report());
      LOG("Error while trying to recover socket, closing connection");
      return {
          .command_status_v = cmd_se::TERMINATE,
          .error = Error{.backtrace = {"Transfer failed, connection lost"}},
      };
    }
    source_conn_guard.dismiss();
    return {
        .command_status_v = cmd_se::CONTINUE,
        .error = Error{.backtrace = {"Transfer failed, connection preserved"}},
    };
  }
  return {.command_status_v = cmd_se::TERMINATE};
}

void NetworkEngine::process_client(const ConnectionView& view) {
  process_client_socket(view.get_slot());
}

// private:
auto NetworkEngine::init_epoll() -> Ev {
  return epoll_handler_.init()
      .transform_error([](auto&& error) -> auto {
        return error.add_context("Cant init epoll_handler");
      })
      .and_then([this]() -> Ev {
        return this->epoll_handler_
            .add_socket(socket_pool_.get_object(0)->socket_handler_, 0)
            .transform_error([](auto&& error) -> auto {
              return error.add_context("Cant add server socket to epoll");
            });
      });
}

auto NetworkEngine::subscribe_to_events(ConnectionEntry& slot_entry,
                                        size_t slot) -> Ev {
  return epoll_handler_.add_socket(slot_entry.socket_handler_, slot)
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

void NetworkEngine::release_client(ConnectionEntry& slot_entry, size_t slot) {
  unsubscribe_from_events(slot_entry);
  slot_entry.reset();
  socket_pool_.release(slot);
}

auto NetworkEngine::register_client(end_point_e socket_type, int client_socket)
    -> std::expected<size_t, Error> {
  auto new_client_spot{
      socket_pool_.push_to_pool(ConnectionEntry{socket_type, client_socket}),
  };
  if (!new_client_spot) {
    return std::unexpected(std::move(new_client_spot)
                               .error()
                               .add_context("Unable to register new client"));
  }
  auto& new_client_entry{*socket_pool_.get_object(*new_client_spot)};
  return subscribe_to_events(new_client_entry, *new_client_spot)
      .transform([&]() -> size_t { return *new_client_spot; })
      .or_else([&](auto&& error) -> std::expected<size_t, Error> {
        LOG(error.full_report());
        send_message_impl(new_client_entry,
                          {
                              .payloads = {user_message(us_e::GENERIC)},
                              .msg_type = message_type_e::PRINTABLE,
                          });
        new_client_entry.reset();
        socket_pool_.release(*new_client_spot);
        return std::unexpected(
            Error{.backtrace = {"Unable to add new client to epoll"}});
      });
}

void NetworkEngine::register_clients(std::vector<int>& new_clients) {
  for (int client : new_clients) {
    if (auto result = register_client(end_point_e::TO_CLIENT, client);
        !result) {
      LOG(result.error().full_report());
    }
  }
}

void NetworkEngine::process_events(const std::vector<size_t>& event_slots) {
  for (size_t slot : event_slots) {
    if (socket_pool_.get_object(slot)->connection_type_ ==
        end_point_e::LISTENER) {
      process_server_socket(slot);
    } else {
      network_raw_tasks_.push(new size_t(slot));
      available_task_tags_.push(new task_tag_e(task_tag_e::NETWORK));
    }
  }
}

void NetworkEngine::process_server_socket(size_t slot) {
  drop_result(socket_pool_.get_object(slot)
                  ->socket_handler_.accept_connections()
                  .and_then([this](auto&& new_clients) -> Ev {
                    register_clients(new_clients);
                    return {};
                  })
                  .or_else([](auto&& error) -> Ev {
                    LOG(error.full_report());
                    LOG("Cant accept new connections");
                    return {};
                  }));
  drop_result(epoll_handler_.rearm_socket(
      socket_pool_.get_object(slot)->socket_handler_, slot));
}

void NetworkEngine::process_client_socket(size_t slot) {
  CommandStatus cmd_status{.command_status_v = cmd_se::CONTINUE};
  while (cmd_status.command_status_v == cmd_se::CONTINUE &&
         socket_pool_.get_object(slot)->socket_handler_.get_socket_status() ==
             socket_status_e::ALIVE) {
    ConnectionEntry& entry = *socket_pool_.get_object(slot);
    auto read_result = entry.socket_handler_.read_user_input();
    if (!read_result) {
      LOG(read_result.error().full_report());
      break;
    }
    cmd_status = process_message(entry, slot, *read_result);
  }
}

auto NetworkEngine::process_message(ConnectionEntry& slot_entry, size_t slot,
                                    ReadResult& message) -> CommandStatus {
  switch (message.status) {
  case bsm::message_status_e::EMPTY:
  case bsm::message_status_e::WOULDBLOCK:
    drop_result(epoll_handler_.rearm_socket(slot_entry.socket_handler_, slot));
    return {.command_status_v = cmd_se::TERMINATE};
  case message_status_e::DISCONNECTED:
    release_client(slot_entry, slot);
    return {.command_status_v = command_status_e::TERMINATE};
  case bsm::message_status_e::DATA:
    break;
  }
  ConnectionView connection{slot};
  CommandContext context{.client_view = connection, .message = message};
  CommandStatus cmd_status = on_message_callback_(
      context); //<-CallBack to Server, process user message
  if (cmd_status.error) {
    LOG("Unable to handle client command: " + context.message.payload);
    LOG(cmd_status.error->full_report());
    if (send_message_impl(slot_entry,
                          {
                              .payloads = {user_message(*cmd_status.user_code)},
                              .msg_type = message_type_e::PRINTABLE,
                          })) {
      socket_pool_.release(slot);
    }
  }
  return cmd_status;
}

auto NetworkEngine::send_message_impl(ConnectionEntry& connection_entry,
                                      const OutgoingMessage& message) -> bool {
  if (auto write_result =
          connection_entry.socket_handler_.write_to_user(message);
      !write_result) {
    LOG("Unable to write message into socket");
    LOG(write_result.error().full_report());
    connection_entry.reset();
    return false;
  }
  return true;
}

void NetworkEngine::Cleanup_Connection::prepare(NetworkEngine& engine) {
  std::scoped_lock lock(engine.m_);
  ConnectionEntry& entry{*engine.socket_pool_.get_object(slot_)};
  entry.generation_++;
  entry.connection_type_ = end_point_e::NONE;
  entry.socket_handler_.set_socket_status(socket_status_e::CLOSED);
}

void NetworkEngine::Cleanup_Connection::execute(NetworkEngine& engine) {
  std::scoped_lock lock(engine.m_);
  engine.release_client(*engine.socket_pool_.get_object(slot_), slot_);
}

} // namespace bsm
