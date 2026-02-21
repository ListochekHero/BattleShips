#ifndef NETWORK_ROUTINE_H
#define NETWORK_ROUTINE_H

#include "socket_routine.h"
#include "utility.h"
#include <cstddef>
#include <functional>
#include <memory>

namespace bsm {

struct DeferredAction;
enum class end_point_e : uint8_t {
  NONE = 0,
  CLIENT = 1 << 0,
  LOBBY = 1 << 1,
  PARENT = 1 << 2,
  SERVER = 1 << 3,
};
struct SlotEntry {
  std::unique_ptr<SocketHandler> handler;
  end_point_e type;
  size_t slot;
  size_t generation{std::numeric_limits<std::size_t>::max()};
};
class NetworkEngine {
public:
  using MessageHandler = std::function<CommandStatus(CommandContext&)>;

  Ev init();
  Ev init(int parrent_socket, end_point_e socket_type); // init() for Lobby
  void set_message_handler(MessageHandler h);
  void run();
  Ev send_message_to(const ConnectionView& conn_view,
                       const OutgoingMessage& message);
  Ev send_error_message_to(const ConnectionView& conn_view,
                           user_error_e user_code);
  void set_status_to(ConnectionView& conn_view, socket_status_e status);
  // ConnectionView attach(int socket);
  std::expected<ConnectionView, Error> attach(int socket,
                                              end_point_e socket_type);
  Ev transfer(const ConnectionView& dest, const ConnectionView& src);

private:
  Ev init_epoll();
  Ev init_epoll_wrapper();
  template <typename... Args>
  std::expected<size_t, Error> emplace_new_entry_to_pool(Args&& ...);
  std::expected<size_t, Error> add_to_socket_pool();
  std::expected<size_t, Error> add_to_socket_pool(int socket_fd,
                                                  end_point_e socket_type);
  Ev free_slot_entry(SlotEntry& slot_entry);
  std::expected<size_t, Error>
  find_spot_for_new_client(int client_socket, end_point_e socket_type);
  Ev subscribe_to_events(SlotEntry& slot_entry);
  void unsubscribe_from_events(SlotEntry& slot_entry);
  void release_client(SlotEntry& slot_entry);
  std::expected<size_t, Error> register_client(int client_socket,
                                               end_point_e socket_type);
  void register_clients(std::vector<int>& new_clients);
  void run_deferred_actions();
  void process_events(std::vector<size_t>& event_slots);
  void process_server_socket(SlotEntry& slot_entry);
  void process_client_socket(SlotEntry& slot_entry);
  CommandStatus process_message(SlotEntry& slot_entry, ReadResult& message);
  Ev send_error_reply(const SlotEntry& slot_entry, user_error_e user_code);

  std::vector<SlotEntry> socket_pool_;
  std::vector<size_t> avaiable_slots_;
  EpollHandler epoll_handler_;
  MessageHandler on_message_callback_;
  std::vector<std::unique_ptr<DeferredAction>> deferred_actions_;
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
