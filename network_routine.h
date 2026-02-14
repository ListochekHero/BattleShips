#ifndef NETWORK_ROUTINE_H
#define NETWORK_ROUTINE_H

#include "deferred.h"
#include "socket_routine.h"
#include "utility.h"
#include <cstddef>
#include <functional>
#include <memory>

namespace bsm {

enum class end_point_type_e { CLIENT, LOBBY, SERVER };

struct SlotEntry {
  std::unique_ptr<SocketHandler> handler;
  end_point_type_e type{end_point_type_e::CLIENT};
  size_t slot{std::numeric_limits<std::size_t>::max()};
  size_t generation{std::numeric_limits<std::size_t>::max()};
};

class NetworkEngine {
public:
  using MessageHandler = std::function<CommandStatus(CommandContext&)>;

  Ev init();
  Ev init(int parrent_socket); // init() for Lobby
  void set_message_handler(MessageHandler h);
  void run();
  void send_message_to(const ConnectionView& conn_view,
                       const OutgoingMessage& message);
  void set_status_to(ConnectionView& conn_view, socket_status_e status);
  // ConnectionView attach(int socket);
  ConnectionView attach(int socket, end_point_type_e socket_type);

private:
  Ev init_epoll();
  Ev init_epoll_wrapper();
  std::expected<size_t, Error> emplace_new_entry_to_pool(SlotEntry slot_entry);
  std::expected<size_t, Error> add_to_socket_pool();
  std::expected<size_t, Error> add_to_socket_pool(int socket_fd,
                                                  end_point_type_e socket_type);
  CommandStatus free_socket_handler(SlotEntry& slot_entry);
  void run_deferred_actions();
  void process_events(std::vector<size_t>& event_slots);
  void process_server_socket(SlotEntry& slot_entry);
  void process_client_socket(SlotEntry& slot_entry);
  Ev register_client(int client_socket, end_point_type_e socket_type);
  void register_clients(std::vector<int>& new_clients);
  CommandStatus process_message(SlotEntry& slot_entry, ReadResult& message);
  Ev send_error_reply(const SlotEntry& slot_entry, Error& error);

  std::vector<size_t> avaiable_slots;
  MessageHandler on_message_callback;
  EpollHandler epoll_handler;
  std::vector<SlotEntry> socket_pool;
  std::vector<std::unique_ptr<DeferredAction>> deferred_actions;
};

class ConnectionView {
public:
  ConnectionView(size_t slot);
  void send(OutgoingMessage message);
  size_t get_slot() const;

private:
  size_t slot;
};

} // namespace bsm
#endif
