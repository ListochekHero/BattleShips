#ifndef NETWORK_ROUTINE_H
#define NETWORK_ROUTINE_H

#include "core/atomic_queue.h"
#include "core/scheduler.h"
#include "deferred_actions.h"
#include "interfaces/modules.h"
#include "protocol/network_defs.h"
#include "protocol/network_types.h"
#include "socket_routine.h"
#include "utility/utility.h"

#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>

#define MAX_EVENTS 10

namespace bsm {

struct SlotEntry {
  std::unique_ptr<SocketHandler> handler;
  end_point_e type{end_point_e::NONE};
  size_t slot{std::numeric_limits<std::size_t>::max()};
  size_t generation{std::numeric_limits<std::size_t>::max()};
};

struct ConnectionMeta {
  size_t slot{std::numeric_limits<std::size_t>::max()};
  end_point_e type{end_point_e::NONE};
  std::string name{};
};

class NetworkEngine : public Module {
public:
  NetworkEngine(AtomicQueue<size_t>& network_q,
                AtomicQueue<task_tag_e>& available_q)
      : network_raw_tasks_(network_q), available_task_tags_(available_q) {}
  std::expected<ConnectionView, Error> init_engine(end_point_e socket_type,
                                                   int parrent_socket);
  using MessageHandler = std::function<CommandStatus(const CommandContext&)>;
  void set_message_handler(MessageHandler h);
  void run();
  bool send_message_to(const ConnectionView& conn_view,
                       const OutgoingMessage& message);
  std::expected<ConnectionView, Error> attach_socket(int socket,
                                                     end_point_e socket_type);
  CommandStatus transfer(const ConnectionView& dest, const ConnectionView& src);
  void process_client(size_t slot);

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
      if (!send_message_impl(entry, message)) {
        delivery_report.failed++;
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
  void init_with_scheduler(Scheduler& scheduler);
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
  bool send_message_impl(SlotEntry& slot_entry, const OutgoingMessage& message);

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
  AtomicQueue<size_t> avaiable_slots_;
  EpollHandler epoll_handler_;
  MessageHandler on_message_callback_;
  DeferredActions deferred_actions_;

  AtomicQueue<size_t>& network_raw_tasks_;
  AtomicQueue<task_tag_e>& available_task_tags_;

  std::mutex m_;
  std::condition_variable cv_;
};

} // namespace bsm

#endif
