#ifndef NETWORK_ROUTINE_H
#define NETWORK_ROUTINE_H

#include "deferred_actions.h"
#include "socket_routine.h"
#include "utility.h"
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>

namespace bsm {

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

class ConnectionView {
public:
  // ConnectionView() = default;
  ConnectionView(size_t slot);
  size_t get_slot() const;

private:
  size_t slot{std::numeric_limits<std::size_t>::max()};
};

struct ConnectionMeta {
  size_t slot{std::numeric_limits<std::size_t>::max()};
  end_point_e type{end_point_e::NONE};
  std::string name{};
};

class NetworkEngine {
public:
  using MessageHandler = std::function<CommandStatus(CommandContext&)>;
  std::expected<ConnectionView, Error> init(end_point_e socket_type);
  std::expected<ConnectionView, Error>
  init(int parrent_socket, end_point_e socket_type); // init() for Lobby
  void set_message_handler(MessageHandler h);
  void run();
  void send_message_to(const ConnectionView& conn_view,
                       const OutgoingMessage& message);
  void cleanup_slot(size_t slot);
  std::expected<ConnectionView, Error> attach(int socket,
                                              end_point_e socket_type);
  CommandStatus transfer(const ConnectionView& dest, const ConnectionView& src);

  template <typename Filter>
    requires std::predicate<Filter, const ConnectionMeta&>
  DeliveryReport send_message(const OutgoingMessage& message, Filter&& filter) {
    DeliveryReport delivery_report;
    for (auto& entry : socket_pool_) {
      if (!entry.handler)
        continue;
      ConnectionMeta meta{entry.slot, entry.type};
      if (!std::invoke(filter, meta))
        continue;
      if (auto result = send_message_impl(entry, message); !result) {
        delivery_report.failed++;
        LOG(result.error()
                .add_context("Unable to send message to specific recipient")
                .full_report());
        continue;
      }
      delivery_report.delivered++;
    }
    return delivery_report;
  }

  template <typename Filter>
    requires std::predicate<Filter, const ConnectionMeta&>
  std::expected<ConnectionView, Error> get_view_by_type(Filter&& filter) {
    for (SlotEntry& entry : socket_pool_) {
      if (!entry.handler)
        continue;
      ConnectionMeta meta{.slot = entry.slot, .type = entry.type};
      if (!std::invoke(filter, meta))
        continue;
      return ConnectionView{entry.slot};
    }
    return std::unexpected(Error{{"No such View could be found"}});
  }

private:
  Ev init_epoll();
  Ev init_epoll_wrapper();
  template <typename... Args>
  std::expected<size_t, Error> emplace_new_entry_to_pool(Args&&...);
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
  void process_events(std::vector<size_t>& event_slots);
  void process_server_socket(size_t slot);
  void process_client_socket(size_t slot);
  CommandStatus process_message(SlotEntry& slot_entry, ReadResult& message);
  void send_message_impl(SlotEntry& slot_entry, const OutgoingMessage& message);
  void send_error_reply_impl(const SlotEntry& slot_entry,
                             user_error_e user_code);

  class Cleanup_Connection : public DeferredAction {
  public:
    explicit Cleanup_Connection(size_t s) : slot(s) {}
    explicit Cleanup_Connection(ConnectionView view) : slot(view.get_slot()) {}
    void prepare(NetworkEngine& engine) override;
    void execute(NetworkEngine& engine) override;

  private:
    size_t slot;
  };

  std::vector<SlotEntry> socket_pool_;
  std::vector<size_t> avaiable_slots_;
  EpollHandler epoll_handler_;
  MessageHandler on_message_callback_;
  DeferredActions deferred_actions_;

  std::mutex m_;
  std::condition_variable cv_;
};

} // namespace bsm
#endif
