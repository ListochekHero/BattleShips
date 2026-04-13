#include "network_routine.h"

#include "utility/config.h"
#include "utility/error.h"
#include "utility/logger.h"

#include <coroutine>
#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <unistd.h>
#include <utility>

namespace bsm {

void NetworkEngine::attach_to_scheduler(Scheduler& scheduler) {
  scheduler.add_executor("network_task",
                         [this](std::unique_ptr<TaskContext> context) {
                           NetworkTaskContext* network_context =
                               static_cast<NetworkTaskContext*>(context.get());
                           process_client(network_context->slot);
                         });
  size_t slot = scheduler.add_co_task([this]() { return run_co(); });
  scheduler.schedule_co_task(slot, [&scheduler](
                                       std::coroutine_handle<> te_co_handle) {
    network_co_handle co_handle =
        network_co_handle::from_address(te_co_handle.address());
    while (true) {
      co_handle.resume();
      scheduler.push_task("network_task", std::make_unique<NetworkTaskContext>(
                                              co_handle.promise().latest_slot));
    }
  });
}

std::expected<ConnectionView, Error>
NetworkEngine::init_engine(end_point_e socket_type) {
  auto socket_slot = add_to_socket_pool();
  if (!socket_slot) {
    return std::unexpected(
        socket_slot.error().add_context("Unable to add init socket to pool"));
  };
  switch (socket_type) {
  case end_point_e::NONE:
  case end_point_e::LOBBY:
  case end_point_e::PARENT:
    break;
  case end_point_e::FROM_SERVER:
    if (auto result = socket_pool_[0].handler->setup_client(); !result) {
      return std::unexpected(
          std::move(result).error().add_context("Cant setup client"));
    }
    socket_pool_[0].type = end_point_e::FROM_SERVER;
    break;
  case end_point_e::SERVER:
    if (auto result = socket_pool_[0].handler->setup_listener(
            Config::instance().get_line("port"));
        !result) {
      return std::unexpected(
          std::move(result).error().add_context("Cant setup listener"));
    }
    break;
  case end_point_e::CLIENT:
    break;
  }
  if (auto result = init_epoll(); !result) {
    return std::unexpected(
        std::move(result).error().add_context("Unable to init epoll"));
  }
  return ConnectionView{*socket_slot};
}

std::expected<ConnectionView, Error>
NetworkEngine::init_engine(int parrent_socket, end_point_e socket_type) {
  auto result = add_to_socket_pool(parrent_socket, socket_type);
  if (!result) {
    return std::unexpected(
        result.error().add_context("Unable to add parent socket to pool"));
  }
  socket_pool_[0].type = end_point_e::PARENT;
  if (auto result = init_epoll_wrapper(); !result) {
    return std::unexpected(
        std::move(result).error().add_context("Unable to init epoll"));
  }
  return ConnectionView{*result};
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
                    LOG("Cant get events");
                    LOG(error.full_report());
                    return {};
                  });
  }
  deferred_actions_.flush(*this);
}

network_co_handle NetworkEngine::run_co() {
  while (true) {
    auto ready_slots = epoll_handler_.wait_for_events(MAX_EVENTS);
    if (!ready_slots) {
      LOG("Cant get events");
      LOG(ready_slots.error().full_report());
      continue;
    }
    for (size_t slot : *ready_slots) {
      if (socket_pool_[slot].type == end_point_e::SERVER) {
        process_server_socket(slot);
      } else if (socket_pool_[slot].type == end_point_e::PARENT) {
        process_client_socket(slot);
      } else if (socket_pool_[slot].type == end_point_e::FROM_SERVER) {
        process_client_socket(slot);
      } else {
        co_yield slot;
      }
    }
  }
  deferred_actions_.flush(*this);
}

bool NetworkEngine::send_message_to(const ConnectionView& conn_view,
                                    const OutgoingMessage& message) {

  return send_message_impl(socket_pool_[conn_view.get_slot()], message);
}

std::expected<ConnectionView, Error>
NetworkEngine::attach_socket(int socket, end_point_e socket_type) {
  auto slot_index = register_client(socket, socket_type);
  if (!slot_index) {
    return std::unexpected(slot_index.error().add_context(
        "Unable to attach new socket to net_engine"));
  }
  return ConnectionView{*slot_index};
}

CommandStatus NetworkEngine::transfer(const ConnectionView& dest_view,
                                      const ConnectionView& src_view) {
  SlotEntry& dest_entry{socket_pool_[dest_view.get_slot()]};
  SlotEntry& src_entry{socket_pool_[src_view.get_slot()]};
  unsubscribe_from_events(src_entry);
  if (auto result =
          dest_entry.handler->write_to_user({{"\\socket"},
                                             message_type_e::SOCKET,
                                             src_entry.handler->get_socket()});
      !result) {
    LOG(result.error().full_report());
    LOG("Unable to transfer socket, trying to recover...");
    if (auto result = subscribe_to_events(src_entry); !result) {
      LOG(result.error().full_report());
      LOG("Error while trying to recover socket, closing connection");
      deferred_actions_.schedule(
          std::make_unique<Cleanup_Connection>(src_entry.slot), *this);
      return CommandStatus{cmd_se::TERMINATE,
                           Error{{"Transfer failed, connection lost"}}};
    }
    return CommandStatus{cmd_se::CONTINUE,
                         Error{{"Transfer failed, connection preserved"}}};
  }
  deferred_actions_.schedule(
      std::make_unique<Cleanup_Connection>(src_entry.slot), *this);
  return CommandStatus{cmd_se::TERMINATE};
}

void NetworkEngine::process_client(size_t slot) {
  process_client_socket(slot);
  return;
}

Ev NetworkEngine::init_epoll() {
  return this->epoll_handler_.init()
      .transform_error([](auto&& error) {
        return error.add_context("Cant init epoll_handler");
      })
      .and_then([this]() -> Ev {
        return this->epoll_handler_.add_socket(*socket_pool_[0].handler, 0)
            .transform_error([](auto&& error) {
              return error.add_context("Cant add server socket to epoll");
            });
      });
}

Ev NetworkEngine::init_epoll_wrapper() {
  return init_epoll().transform_error(
      [](auto&& error) { return error.add_context("Unable to init epoll"); });
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
    return std::unexpected(Error{{"Unable to emplace new socket handler"}});
  }
}

std::expected<size_t, Error> NetworkEngine::add_to_socket_pool() {
  return emplace_new_entry_to_pool(std::make_unique<SocketHandler>(),
                                   end_point_e::SERVER)
      .transform_error([](auto&& error) {
        return error.add_context("Unable to increase socket pool size");
      });
}

std::expected<size_t, Error>
NetworkEngine::add_to_socket_pool(int socket_fd, end_point_e socket_type) {
  return emplace_new_entry_to_pool(std::make_unique<SocketHandler>(socket_fd),
                                   socket_type)
      .transform_error([](auto&& error) {
        error.add_context("Unable to add given socket to socket pool");
        return error;
      });
}

Ev NetworkEngine::free_slot_entry(SlotEntry& slot_entry) {
  try {
    slot_entry.handler->reset_to_empty();
    size_t* new_slot = new size_t(slot_entry.slot);
    avaiable_slots_.push(new_slot);
  } catch (const std::exception& e) {
    return std::unexpected(
        Error{{e.what(), "Exception caught during freeing slot entry"}});
  }
  return {};
}

std::expected<size_t, Error>
NetworkEngine::find_spot_for_new_client(int client_socket,
                                        end_point_e socket_type) {
  size_t* slot = avaiable_slots_.pop();
  if (slot) {
    SlotEntry* entry{&socket_pool_[*slot]};
    entry->handler->reset_with_new(client_socket);
    entry->type = socket_type;
    return *slot;
  } else {
    auto new_entry_slot = add_to_socket_pool(client_socket, socket_type);
    if (!new_entry_slot) {
      return std::unexpected(
          std::move(new_entry_slot)
              .error()
              .add_context("Unable to find spot for new client"));
    }
    return new_entry_slot;
  }
}

Ev NetworkEngine::subscribe_to_events(SlotEntry& slot_entry) {
  return epoll_handler_.add_socket(*slot_entry.handler, slot_entry.slot)
      .or_else([&](auto&& error) -> Ev {
        return std::unexpected(
            error.add_context("Unable to subscribe new client to epoll"));
      });
}

void NetworkEngine::unsubscribe_from_events(SlotEntry& slot_entry) {
  drop_result(epoll_handler_.remove_socket(*slot_entry.handler)
                  .or_else([](auto&& error) -> Ev {
                    LOG(error.full_report());
                    return {};
                  }));
}

void NetworkEngine::release_client(SlotEntry& slot_entry) {
  unsubscribe_from_events(slot_entry);
  success_or_terminate(free_slot_entry(slot_entry));
}

std::expected<size_t, Error>
NetworkEngine::register_client(int client_socket, end_point_e socket_type) {
  auto new_client_slot = find_spot_for_new_client(client_socket, socket_type);
  if (!new_client_slot) {
    return std::unexpected(std::move(new_client_slot)
                               .error()
                               .add_context("Unable to register new client"));
  }
  SlotEntry& new_client_entry{socket_pool_[*new_client_slot]};
  return subscribe_to_events(new_client_entry)
      .transform([&]() { return new_client_entry.slot; })
      .or_else([&](auto&& error) -> std::expected<size_t, Error> {
        LOG(error.full_report());
        send_message_impl(new_client_entry, {{user_message(us_e::GENERIC)},
                                             message_type_e::PRINTABLE});
        success_or_terminate(free_slot_entry(new_client_entry));
        return std::unexpected(Error{{"Unable to add new client to epoll"}});
      });
}

void NetworkEngine::register_clients(std::vector<int>& new_clients) {
  for (int client : new_clients) {
    if (auto result = register_client(client, end_point_e::CLIENT); !result) {
      LOG(result.error().full_report());
    }
  }
}

void NetworkEngine::process_events(std::vector<size_t>& event_slots) {
  for (size_t slot : event_slots) {
    if (socket_pool_[slot].type == end_point_e::SERVER) {
      process_server_socket(slot);
    } else if (socket_pool_[slot].type == end_point_e::PARENT) {
      process_client_socket(slot);
    } else if (socket_pool_[slot].type == end_point_e::FROM_SERVER) {
      process_client_socket(slot);
    } else {
      // bool push_success = push_to_clients_queue(slot); // <- callback to add
      // pending client to queue for processing process_client_socket(slot);
    }
  }
}

void NetworkEngine::process_server_socket(size_t slot) {
  drop_result(socket_pool_[slot]
                  .handler->accept_connections()
                  .and_then([this](auto&& new_clients) -> Ev {
                    register_clients(new_clients);
                    return {};
                  })
                  .or_else([](auto&& error) -> Ev {
                    LOG(error.full_report());
                    LOG("Cant accept new connections");
                    return {};
                  }));
  epoll_handler_.rearm_socket(*(socket_pool_[slot].handler),
                              socket_pool_[slot].slot);
}

void NetworkEngine::process_client_socket(size_t slot) {
  CommandStatus cmd_status{cmd_se::CONTINUE};
  while (cmd_status.command_status_v == cmd_se::CONTINUE &&
         socket_pool_[slot].handler->get_socket_status() ==
             socket_status_e::ALIVE) {
    SlotEntry& entry = socket_pool_[slot];
    auto read_result = entry.handler->read_user_input();
    if (!read_result) {
      LOG(read_result.error().full_report());
      break;
    }
    cmd_status = process_message(entry, *read_result);
  }
}

CommandStatus NetworkEngine::process_message(SlotEntry& slot_entry,
                                             ReadResult& message) {
  switch (message.status) {
  case bsm::message_status_e::EMPTY:
  case bsm::message_status_e::WOULDBLOCK:
    epoll_handler_.rearm_socket(*(slot_entry.handler), slot_entry.slot);
    return CommandStatus{cmd_se::TERMINATE};
  case message_status_e::DISCONNECTED:
    release_client(slot_entry);
    return {command_status_e::TERMINATE};
  case bsm::message_status_e::DATA:
    break;
  }
  ConnectionView connection{slot_entry.slot};
  CommandContext context{connection, message};
  CommandStatus cmd_status = on_message_callback_(
      context); //<-CallBack to Server, process user message
  if (cmd_status.error) {
    LOG("Unable to handle client command: " + context.message.payload);
    LOG(cmd_status.error->full_report());
    send_message_impl(slot_entry, {{user_message(*cmd_status.user_code)},
                                   message_type_e::PRINTABLE});
  }
  return cmd_status;
}

bool NetworkEngine::send_message_impl(SlotEntry& slot_entry,
                                      const OutgoingMessage& message) {
  if (auto write_result = slot_entry.handler->write_to_user(message);
      !write_result) {
    LOG("Unable to write message into socket");
    LOG(write_result.error().full_report());
    deferred_actions_.schedule(
        std::make_unique<Cleanup_Connection>(slot_entry.slot), *this);
    return false;
  }
  return true;
}

void NetworkEngine::Cleanup_Connection::prepare(NetworkEngine& engine) {
  std::lock_guard lock(engine.m_);
  SlotEntry& entry{engine.socket_pool_[slot]};
  entry.generation++;
  entry.type = end_point_e::NONE;
  entry.handler->set_socket_status(socket_status_e::CLOSED);
}

void NetworkEngine::Cleanup_Connection::execute(NetworkEngine& engine) {
  std::lock_guard lock(engine.m_);
  engine.release_client(engine.socket_pool_[slot]);
}

ConnectionView::ConnectionView(size_t slot) : slot{slot} {}

size_t ConnectionView::get_slot() const { return slot; }

} // namespace bsm
