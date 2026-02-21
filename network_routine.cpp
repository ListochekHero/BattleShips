#include "network_routine.h"
#include "application.h"
#include "config.h"
#include "logger.h"
#include "scope_guard.h"
#include "socket_routine.h"
#include "utility.h"
#include <cstddef>
#include <exception>
#include <memory>
#include <optional>
#include <unistd.h>
#include <utility>

namespace bsm {

Ev NetworkEngine::init() {
  if (auto result = add_to_socket_pool(); !result) {
    LOG(result.error().message);
    return std::unexpected(Error{"Unable to add init socket to pool"});
  };
  return this->socket_pool_[0]
      .handler->setup_listener(Config::instance().get_line("port"))
      .or_else([](auto&& error) -> Ev {
        LOG(error.message);
        return std::unexpected(Error{"Cant setup listener"});
      })
      .and_then([this]() -> Ev { return init_epoll_wrapper(); });
}

Ev NetworkEngine::init(int parrent_socket, end_point_e socket_type) {
  if (auto result = add_to_socket_pool(parrent_socket, socket_type); !result) {
    LOG(result.error().message);
    return std::unexpected(Error{"Unable to add parent socket to pool"});
  }

  socket_pool_[0].type = end_point_e::PARENT;
  return init_epoll_wrapper();
}

void NetworkEngine::set_message_handler(MessageHandler h) {
  on_message_callback_ = h;
}

void NetworkEngine::run() {
  while (true) {
    [[maybe_unused]]
    auto __ = epoll_handler_.wait_for_events(MAX_EVENTS)
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

Ev NetworkEngine::send_message_to(const ConnectionView& conn_view,
                                  const OutgoingMessage& message) {
  return socket_pool_[conn_view.get_slot()]
      .handler->write_to_user(message)
      .transform_error([](auto&& error) {
        LOG(error.message);
        return Error{"Unable to send message to user"};
      });
}

Ev NetworkEngine::send_error_message_to(const ConnectionView& conn_view,
                                        user_error_e user_code) {
  return send_error_reply(socket_pool_[conn_view.get_slot()], user_code);
}
void NetworkEngine::set_status_to(ConnectionView& conn_view,
                                  socket_status_e status) {
  socket_pool_[conn_view.get_slot()].handler->set_socket_status(status);
}

// ConnectionView NetworkEngine::attach(int socket) { register_client(socket); }

std::expected<ConnectionView, Error>
NetworkEngine::attach(int socket, end_point_e socket_type) {
  auto slot_index = register_client(socket, socket_type);
  if (slot_index) {
    return ConnectionView{*slot_index};
  } else {
    LOG(slot_index.error().message);
    return std::unexpected(Error{"Unable to attach new client to engine"});
  }
}
Ev NetworkEngine::transfer(const ConnectionView& dest_view,
                           const ConnectionView& src_view) {
  SlotEntry& dest_entry{socket_pool_[dest_view.get_slot()]};
  SlotEntry& src_entry{socket_pool_[src_view.get_slot()]};
  unsubscribe_from_events(src_entry);
  auto rollback{make_scope_guard([&]() {
    if (auto result = subscribe_to_events(src_entry); !result) {
      LOG(result.error().message);
      if (auto result = free_slot_entry(src_entry); !result) {
        LOG(result.error().message);
      }
    }
  })};
  if (auto result =
          dest_entry.handler->write_to_user({{"\\socket"},
                                             message_type_e::SOCKET,
                                             src_entry.handler->get_socket()});
      !result) {
    return std::unexpected(Error{"Unable to transfer socket over"});
  }
  rollback.dismiss();
  return free_slot_entry(src_entry);
}

Ev NetworkEngine::init_epoll() {
  return this->epoll_handler_.init()
      .or_else([](auto&& error) -> Ev {
        LOG(error.message);
        return std::unexpected(Error{"Cant init epoll_handler"});
      })
      .and_then([this]() -> Ev {
        return this->epoll_handler_.add_socket(*socket_pool_[0].handler, 0)
            .or_else([](auto&& error) -> Ev {
              LOG(error.message);
              return std::unexpected(Error{"Cant add server socket to epoll"});
            });
      });
}

Ev NetworkEngine::init_epoll_wrapper() {
  return init_epoll().or_else([](auto&& error) -> Ev {
    LOG(error.message);
    return std::unexpected(Error{"Unable to init epoll"});
  });
}

template <typename... Args>
std::expected<size_t, Error>
NetworkEngine::emplace_new_entry_to_pool(Args&&... args) {
  try {
    size_t occupied_slot{socket_pool_.size()};
    socket_pool_.emplace_back(std::forward<Args>(args)..., occupied_slot);
    return occupied_slot;
  } catch (const std::exception& e) {
    LOG(e.what());
    return std::unexpected(Error{"Unable to emplace new socket handler"});
  }
}

std::expected<size_t, Error> NetworkEngine::add_to_socket_pool() {
  return emplace_new_entry_to_pool(std::make_unique<SocketHandler>(),
                                   end_point_e::SERVER)
      .transform_error([](auto error) {
        LOG(error.message);
        error.message = "Unable to increase socket pool size";
        return error;
      });
}

std::expected<size_t, Error>
NetworkEngine::add_to_socket_pool(int socket_fd, end_point_e socket_type) {
  return emplace_new_entry_to_pool(std::make_unique<SocketHandler>(socket_fd),
                                   socket_type)
      .transform_error([](auto error) {
        LOG(error.message);
        error.message = "Unable to add given socket to socket pool";
        return error;
      });
}

Ev NetworkEngine::free_slot_entry(SlotEntry& slot_entry) {
  try {
    avaiable_slots_.emplace_back(slot_entry.slot);
    slot_entry.handler->reset_to_empty();
  } catch (const std::exception e) {
    LOG(e.what());
    return std::unexpected(Error{"Exception caught during freeing slot entry"});
  }
  return {};
}

std::expected<size_t, Error>
NetworkEngine::find_spot_for_new_client(int client_socket,
                                        end_point_e socket_type) {
  if (!avaiable_slots_.empty()) {
    size_t slot = avaiable_slots_.back();
    avaiable_slots_.pop_back();
    SlotEntry* entry{&socket_pool_[slot]};
    entry->handler->reset_with_new(client_socket);
    entry->type = socket_type;
    return slot;
  } else {
    auto new_entry_slot = add_to_socket_pool(client_socket, socket_type);
    if (!new_entry_slot) {
      if (!new_entry_slot) {
        LOG(new_entry_slot.error().message);
        return std::unexpected(Error{"Unable to find spot for new client"});
      }
    }
    return new_entry_slot.value();
  }
}

Ev NetworkEngine::subscribe_to_events(SlotEntry& slot_entry) {
  return epoll_handler_.add_socket(*slot_entry.handler, slot_entry.slot)
      .or_else([&](auto&& error) -> Ev {
        LOG(error.message);
        return std::unexpected(
            Error{"Unable to subscribe new client to epoll"});
      });
}

void NetworkEngine::unsubscribe_from_events(SlotEntry& slot_entry) {
  drop_result(epoll_handler_.remove_socket(*slot_entry.handler)
                  .or_else([](auto&& error) -> Ev {
                    LOG(error.message);
                    return {};
                  }));
}

void NetworkEngine::release_client(SlotEntry& slot_entry) {
  unsubscribe_from_events(slot_entry);
  free_slot_entry(slot_entry);
  return;
}

std::expected<size_t, Error>
NetworkEngine::register_client(int client_socket, end_point_e socket_type) {
  auto new_client_slot = find_spot_for_new_client(client_socket, socket_type);
  if (!new_client_slot) {
    LOG(new_client_slot.error().message);
    return std::unexpected(Error{"Unable to register new client"});
  }
  SlotEntry& new_client_entry{socket_pool_[new_client_slot.value()]};
  return subscribe_to_events(new_client_entry)
      .transform([&]() { return new_client_entry.slot; })
      .or_else([&](auto&& error) -> std::expected<size_t, Error> {
        LOG(error.message);
        if (auto r = send_error_reply(new_client_entry, us_e::GENERIC); !r) {
          LOG(r.error().message);
        }
        free_slot_entry(new_client_entry);
        return std::unexpected(Error{"Unable to add new client to epoll"});
      });
}

void NetworkEngine::register_clients(std::vector<int>& new_clients) {
  for (int client : new_clients) {
    if (auto result = register_client(client, end_point_e::CLIENT); !result) {
      LOG(result.error().message);
    }
  }
}

void NetworkEngine::run_deferred_actions() {
  for (auto& action : deferred_actions_) {
    action->run(*this);
  }
  deferred_actions_.clear();
}

void NetworkEngine::process_events(std::vector<size_t>& event_slots) {
  for (size_t slot : event_slots) {
    SlotEntry& slot_entry{socket_pool_[slot]};
    if (slot_entry.type == end_point_e::SERVER) {
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

CommandStatus NetworkEngine::process_message(SlotEntry& slot_entry,
                                             ReadResult& message) {
  switch (message.status) {
  case bsm::message_status_e::EMPTY:
  case bsm::message_status_e::WOULDBLOCK:
    return CommandStatus{cmd_se::TERMINATE};
  case message_status_e::DISCONNECTED:
    release_client(slot_entry);
    return {command_status_e::TERMINATE};
  case bsm::message_status_e::DATA:
    break;
  }
  ConnectionView connection{slot_entry.slot};
  CommandContext context{connection, message, slot_entry.type};
  CommandStatus cmd_status = on_message_callback_(
      context); //<-CallBack to Server, process user message
  if (cmd_status.error) {
    LOG(cmd_status.error->message);
    LOG("Unable to handle client command: " + context.message.payload);
    if (auto result = send_error_reply(slot_entry, us_e::GENERIC); !result)
      LOG(result.error().message);
  }
  return cmd_status;
}

Ev NetworkEngine::send_error_reply(const SlotEntry& slot_entry,
                                   user_error_e user_code) {
  return slot_entry.handler->write_to_user({{user_message(user_code)}})
      .or_else([](auto&& error) -> Ev {
        LOG(error.message);
        return std::unexpected(Error{"Unable to send an answer to user"});
      });
}

ConnectionView::ConnectionView(size_t slot) : slot{slot} {}

size_t ConnectionView::get_slot() const { return slot; }

} // namespace bsm
