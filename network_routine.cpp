#include "network_routine.h"
#include "application.h"
#include "config.h"
#include "logger.h"
#include "socket_routine.h"
#include "utility.h"
#include <cstddef>

namespace bsm {

Ev Server::init() {
  if (auto result = add_to_socket_pool(); !result) {
    LOG(result.error().message);
    return std::unexpected(Error{"Unable to add init socket to pool"});
  };
  return this->sockets[0]
      ->setup_listener(Config::instance().get_line("port"))
      .or_else([](auto&& error) -> Ev {
        LOG(error.message);
        return std::unexpected(Error{"Cant setup listener"});
      })
      .and_then([this]() -> Ev { return init_epoll_wrapper(); });
}

Ev Server::init(int parrent_socket) {
  if (auto result = add_to_socket_pool(parrent_socket); !result) {
    LOG(result.error().message);
    return std::unexpected(Error{"Unable to add parent socket to pool"});
  }
  this->sockets[0]->set_socket_type(socket_type_e::IPC);
  return init_epoll_wrapper();
}

void NetworkEngine::set_message_handler(MessageHandler h) {
  on_message_callback = h;
}

void NetworkEngine::run() {
  while (true) {
    [[maybe_unused]]
    auto __ = epoll_handler.wait_for_events(MAX_EVENTS)
                  .and_then([this](auto&& event_slots) -> Ev {
                    process_events(event_slots);
                    return {};
                  })
                  .or_else([](auto&& error) -> Ev {
                    LOG(error.message);
                    LOG("Cant get events");
                    return {};
                  });
  }
  run_deferred_actions();
}

void NetworkEngine::send_message_to(const ConnectionView& conn_view,
                                    const OutgoingMessage& message) {
  socket_pool[conn_view.get_slot()].handler->write_to_user(message);
}

void NetworkEngine::set_status_to(ConnectionView& conn_view,
                                  socket_status_e status) {
  socket_pool[conn_view.get_slot()].handler->set_socket_status(status);
}

// ConnectionView NetworkEngine::attach(int socket) { register_client(socket); }

ConnectionView NetworkEngine::attach(int socket, end_point_type_e socket_type) {
  register_client(socket, socket_type);
}

Ev NetworkEngine::init_epoll() {
  return this->epoll_handler.init()
      .or_else([](auto&& error) -> Ev {
        LOG(error.message);
        return std::unexpected(Error{"Cant init epoll_handler"});
      })
      .and_then([this]() -> Ev {
        return this->epoll_handler.add_socket(*socket_pool[0].handler)
            .or_else([](auto&& error) -> Ev {
              LOG(error.message);
              return std::unexpected(Error{"Cant add socket to epoll"});
            });
      });
}

Ev NetworkEngine::init_epoll_wrapper() {
  return init_epoll().or_else([](auto&& error) -> Ev {
    LOG(error.message);
    return std::unexpected(Error{"Unable to init epoll"});
  });
}

std::expected<size_t, Error>
NetworkEngine::emplace_new_entry_to_pool(SlotEntry slot_entry) {
  try {
    socket_pool.emplace_back(std::move(slot_entry));
    size_t occupied_slot{socket_pool.size() - 1};
    socket_pool.back().handler->set_occupied_slot(occupied_slot);
    return occupied_slot;
  } catch (const std::exception& e) {
    LOG(e.what());
    return std::unexpected(Error{"Unable to emplace new socket handler"});
  }
}

std::expected<size_t, Error> NetworkEngine::add_to_socket_pool() {
  return emplace_new_entry_to_pool(SlotEntry{std::make_unique<SocketHandler>()})
      .transform_error([](auto&& error) {
        LOG(error.message);
        error.message = "Unable to increase socket pool size";
        return error;
      });
}
std::expected<size_t, Error>
NetworkEngine::add_to_socket_pool(int socket_fd, end_point_type_e socket_type) {
  return emplace_new_entry_to_pool(
             SlotEntry{std::make_unique<SocketHandler>(socket_fd), socket_type})
      .transform_error([](auto&& error) {
        LOG(error.message);
        error.message = "Unable to add given socket to socket pool";
        return error;
      });
}

CommandStatus NetworkEngine::free_socket_handler(SlotEntry& slot_entry) {
  if (auto result = epoll_handler.remove_socket(*slot_entry.handler); !result) {
    LOG(result.error().message);
    LOG("Unable to remove socket from epoll before closing");
  }
  slot_entry.handler->reset_to_empty();
  avaiable_slots.emplace_back(slot_entry.handler->get_occupied_slot());
  return {};
}

void Server::run_deferred_actions() {
  for (auto& action : deferred_actions) {
    action->run(*this);
  }
  deferred_actions.clear();
}

void NetworkEngine::process_events(std::vector<size_t>& event_slots) {
  for (size_t slot : event_slots) {
    SlotEntry& slot_entry{socket_pool[slot]};
    if (slot_entry.handler->get_socket_type() == socket_type_e::SERVER) {
      process_server_socket(slot_entry);
    } else {
      process_client_socket(slot_entry);
    }
  }
}

void NetworkEngine::process_server_socket(SlotEntry& slot_entry) {
  [[maybe_unused]]
  auto __ = slot_entry.handler->accept_connections()
                .and_then([this](auto&& new_clients) -> Ev {
                  register_clients(new_clients);
                  return {};
                })
                .or_else([](auto&& error) -> Ev {
                  LOG(error.message);
                  LOG("Cant accept new connections");
                  return {};
                });
}

void NetworkEngine::process_client_socket(SlotEntry& slot_entry) {
  CommandStatus cmd_status{cmd_se::CONTINUE};
  while (cmd_status.command_status_v == cmd_se::CONTINUE &&
         slot_entry.handler->get_socket_status() == socket_status_e::ALIVE) {
    auto read_result = slot_entry.handler->read_user_input();
    if (!read_result) {
      LOG(read_result.error().message);
      break;
    }
    cmd_status = process_message(slot_entry, *read_result);
  }
}

Ev NetworkEngine::register_client(int client_socket,
                                  end_point_type_e socket_type) {
  SlotEntry* client_entry = [&]() -> SlotEntry* {
    if (!avaiable_slots.empty()) {
      size_t slot = avaiable_slots.back();
      avaiable_slots.pop_back();
      SlotEntry& entry{socket_pool[slot]};
      entry.handler->reset_with_new(client_socket);
      entry.type = socket_type;
      return &entry;
    } else {
      auto new_entry = add_to_socket_pool(client_socket, socket_type);
      if (!new_entry) {
        LOG(new_entry.error().message);
        return nullptr;
      }
      return &socket_pool.back();
    }
  }();
  if (client_entry == nullptr)
    return std::unexpected(Error{"Unable to register new client"});
  if (auto result = epoll_handler.add_socket(*client_entry->handler); !result) {
    LOG(result.error().message);
    LOG("Unable to add new client to epoll");
    if (auto r = send_error_reply(*client_entry, result.error()); !r) {
      LOG(r.error().message);
    }
    avaiable_slots.emplace_back(client_entry->handler->get_occupied_slot());
    client_entry->handler->reset_to_empty();
  }
  return {};
}
void NetworkEngine::register_clients(std::vector<int>& new_clients) {
  for (int client : new_clients) {
    if (auto result = register_client(client, end_point_type_e::CLIENT);
        !result) {
      LOG(result.error().message);
      LOG("Continuing with the rest of clients...");
    }
  }
}

CommandStatus NetworkEngine::process_message(SlotEntry& slot_entry,
                                             ReadResult& message) {
  switch (message.status) {
  case bsm::message_status_e::EMPTY:
  case bsm::message_status_e::WOULDBLOCK:
    return CommandStatus{cmd_se::TERMINATE};
  case message_status_e::DISCONNECTED:
    return free_socket_handler(slot_entry);
  case bsm::message_status_e::DATA:
    break;
  }
  ConnectionView connection{slot_entry.slot};
  CommandContext context{connection, message};
  CommandStatus cmd_status =
      on_message_callback(context); //<-CallBack to Server, process user message
  if (cmd_status.error) {
    LOG(cmd_status.error->message);
    LOG("Unable to handle client command: " + context.message.payload);
    if (auto result = send_error_reply(slot_entry, *cmd_status.error); !result)
      LOG(result.error().message);
  }
  return cmd_status;
}

Ev NetworkEngine::send_error_reply(const SlotEntry& slot_entry, Error& error) {
  return slot_entry.handler->write_to_user({{user_message(error.user_code)}})
      .or_else([](auto&& error) -> Ev {
        LOG(error.message);
        return std::unexpected(Error{"Unable to send an answer to user"});
      });
}

ConnectionView::ConnectionView(size_t slot) : slot{slot} {}

size_t ConnectionView::get_slot() const { return slot; }

} // namespace bsm
