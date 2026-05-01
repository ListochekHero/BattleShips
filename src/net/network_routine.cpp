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

auto NetworkEngine::send_message_to(const ConnectionView& recipient_view,
                                    const OutgoingMessage& outgoing_message)
    -> std::optional<Error> {
  auto recipient_slot{recipient_view.get_slot()};
  auto& recipient_connection{*connection_pool_.get_object(recipient_slot)};
  auto error{send_message_impl(recipient_connection, outgoing_message)};
  if (error) {
    release_connection(
        recipient_connection, // for now, if we cannot send message via
        recipient_slot);      // connection, it's better to just drop it
    return error;
  }
  return std::nullopt;
}

auto NetworkEngine::attach_socket(int socket, end_point_e socket_type)
    -> std::expected<ConnectionView, Error> {
  auto registration_slot = register_connection(socket_type, socket);
  if (!registration_slot) {
    return std::unexpected(registration_slot.error().add_context(
        "Unable to attach socket: failed to register connection"));
  }
  return ConnectionView{*registration_slot};
}

auto NetworkEngine::transfer(const ConnectionView& destination_view,
                             const ConnectionView& source_view)
    -> TransferResult {
  TransferResult transfer_result{};
  auto& destination_conn{
      *connection_pool_.get_object(destination_view.get_slot()),
  };
  size_t source_slot{source_view.get_slot()};
  auto& source_conn{*connection_pool_.get_object(source_slot)};
  auto source_conn_guard{
      scope_guard([&source_conn, this, source_slot]() -> auto {
        source_conn.reset();
        connection_pool_.release(source_slot);
      }),
  };
  unsubscribe_from_events(source_conn);
  if (auto transfer_error = send_message_impl(
          destination_conn,
          {
          .payloads = {"\\socket"},
          .msg_type = message_type_e::SOCKET,
          .socket = source_conn.socket_handler_.get_socket(),
      });
      transfer_error) {
    transfer_error->add_context("Unable to transfer socket: failed to send "
                                "socket to recipient");
    transfer_error->add_context("Releasing recipient connection");
    release_connection(destination_conn, destination_view.get_slot());
    transfer_result.destination_conn_preserved = false;
    transfer_error->add_context("Trying to recover source connection...");
    if (auto recover_error = subscribe_to_events(source_conn, source_slot);
        recover_error) {
      transfer_error->add_context(recover_error->full_report());
      transfer_error->add_context("Unable to recover source connection: failed "
                                  "to subscribe back to events, "
                                  "releasing source connection");
      release_connection(source_conn, source_slot);
      transfer_result.source_conn_preserved = false;
    } else {
      transfer_error->add_context("Source connection preserved");
    }
    source_conn_guard.dismiss();
    LOG(transfer_error->full_report());
  }
  return transfer_result;
}

auto NetworkEngine::process_connection(const ConnectionView& pending_view)
    -> std::optional<Error> {
  if (auto process_error{process_connection_impl(pending_view.get_slot())};
      process_error) {
    return process_error;
  }
  return std::nullopt;
}

// private:
auto NetworkEngine::init_epoll() -> std::optional<Error> {
  if (auto init_error{epoll_handler_.init()}; init_error) {
    return init_error->add_context(
        "Unable to init epoll for Engine: failed to init");
  }
  size_t root_slot{0};
  auto& root_connection{*connection_pool_.get_object(root_slot)};
  if (auto adding_error{
          subscribe_to_events(root_connection, root_slot),
      };
      adding_error) {
    return adding_error->add_context("Unable to init epoll for Engine: failed "
                                     "to subscribe root connection to events");
  }
  return std::nullopt;
}

auto NetworkEngine::subscribe_to_events(ConnectionEntry& connection,
                                        size_t pool_slot)
    -> std::optional<Error> {
  if (auto adding_error{
          epoll_handler_.add_socket(connection.socket_handler_, pool_slot),
      };
      adding_error) {
    return adding_error->add_context(
        "Unable to subscribe connection to events: failed "
        "to add socket to epoll");
  }
  return std::nullopt;
}

auto NetworkEngine::unsubscribe_from_events(ConnectionEntry& connection)
    -> std::optional<Error> {
  if (auto error{epoll_handler_.remove_socket(connection.socket_handler_)};
      error) {
    error->add_context("Unable to unsubcribe connection from events: failed to "
                       "remove socket from epoll");
  }
                    return {};
}

void NetworkEngine::release_connection(ConnectionEntry& connection,
                                       size_t pool_slot) {
  if (auto unsubscribe_error{unsubscribe_from_events(connection)};
      unsubscribe_error) {
    LOG(unsubscribe_error
            ->full_report()); // For now just ignore errors from epoll
  }
  connection.reset();
  connection_pool_.release(pool_slot);
}

auto NetworkEngine::register_connection(end_point_e socket_type, int socket)
    -> std::expected<size_t, Error> {
  auto push_result{
      connection_pool_.push_to_pool(ConnectionEntry{socket_type, socket}),
  };
  if (!push_result) {
    return std::unexpected(
        std::move(push_result)
                               .error()
            .add_context("Unable to register new connection: failed to push to "
                         "connection pool"));
  }
  size_t connection_slot{*push_result};
  auto& connection{*connection_pool_.get_object(connection_slot)};
  if (auto subscribe_error{subscribe_to_events(connection, connection_slot)};
      subscribe_error) {
    subscribe_error->add_context(
        "Unable to register new connection: failed to subscribe to events");
    send_message_impl(connection,
                          {
                          .payloads = {user_message(user_error_e::GENERIC)},
                              .msg_type = message_type_e::PRINTABLE,
                          });
    connection.reset();
    connection_pool_.release(connection_slot);
    return std::unexpected(*subscribe_error);
  }
  return connection_slot;
}

auto NetworkEngine::register_connections(std::vector<int>& new_sockets)
    -> RegistrationReport {
  RegistrationReport registration_report;
  for (int socket : new_sockets) {
    if (auto registration_result{
            register_connection(end_point_e::TO_CLIENT, socket),
        };
        !registration_result) {
      LOG(registration_result.error().full_report());
      registration_report.failed++;
    } else {
      registration_report.registered++;
    }
  }
  return registration_report;
}

void NetworkEngine::process_events(const std::vector<size_t>& event_slots) {
  for (size_t slot : event_slots) {
    auto& pending_connection{*connection_pool_.get_object(slot)};
    if (pending_connection.connection_type_ == end_point_e::LISTENER) {
      process_server_socket(pending_connection, slot);
    } else {
      auto* task_slot{new (std::nothrow) size_t(slot)};
      auto* task_tag{new (std::nothrow) task_tag_e(task_tag_e::NETWORK)};
      if (task_slot == nullptr || task_tag == nullptr) {
        plain_terminate();
      }
      network_raw_tasks_.push(task_slot);
      available_task_tags_.push(task_tag);
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
                                      const OutgoingMessage& message)
    -> std::optional<Error> {
  if (auto error{connection_entry.socket_handler_.send_message(message)};
      error) {
    return error->add_context("Unable to send message via connection: failed "
                              "to send message to socket");
}
  return std::nullopt;
}

} // namespace bsm
