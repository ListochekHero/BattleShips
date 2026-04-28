#ifndef NETWORK_ROUTINE_H
#define NETWORK_ROUTINE_H

#include "core/atomic_queue.h"
#include "core/object_pool.h"
#include "deferred_actions.h"
#include "interfaces/modules.h"
#include "protocol/network_defs.h"
#include "protocol/network_types.h"
#include "socket_routine.h"
#include "utility/error.h"
#include "utility/utility.h"

#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#define MAX_EVENTS 10

namespace bsm {

enum class task_tag_e : uint8_t;
struct OutgoingMessage;
struct ReadResult;

struct ConnectionEntry {
  ConnectionEntry() = default;
  ConnectionEntry(const ConnectionEntry&) = delete;
  ConnectionEntry(end_point_e type, int socket);
  ConnectionEntry(ConnectionEntry&&) noexcept;
  auto operator=(ConnectionEntry&& other_entry) noexcept -> ConnectionEntry&;
  auto operator=(const ConnectionEntry&) = delete;
  void reset();

  end_point_e connection_type_{end_point_e::NONE};
  SocketHandler socket_handler_;
  size_t generation_{0};
};

struct ConnectionMeta {
  size_t slot{std::numeric_limits<std::size_t>::max()};
  end_point_e type{end_point_e::NONE};
  std::string name;
};

class NetworkEngine : public Module {
public:
  NetworkEngine(AtomicQueue<size_t>& network_q,
                AtomicQueue<task_tag_e>& available_q)
      : network_raw_tasks_(network_q), available_task_tags_(available_q) {}
  auto init_engine(end_point_e socket_type, int parrent_socket)
      -> std::expected<ConnectionView, Error>;
  using MessageHandler = std::function<CommandStatus(const CommandContext&)>;
  void set_message_handler(MessageHandler msg_handler);
  void run();
  auto send_message_to(const ConnectionView& conn_view,
                       const OutgoingMessage& message) -> bool;
  auto attach_socket(int socket, end_point_e socket_type)
      -> std::expected<ConnectionView, Error>;
  auto transfer(const ConnectionView& dest, const ConnectionView& src)
      -> CommandStatus;
  void process_client(size_t slot);

  template <typename Filter>
    requires std::predicate<Filter, const ConnectionMeta&>
  auto send_message(const OutgoingMessage& message, Filter&& filter)
      -> DeliveryReport {
    DeliveryReport delivery_report;
    for (auto& entry : socket_pool_) {
      if (entry.connection_type_ == end_point_e::NONE) {
        continue;
      }
      ConnectionMeta meta{.slot = entry.occupied_slot_,
                          .type = entry.connection_type_};
      if (!std::invoke(filter, meta)) {
        continue;
      }
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
  auto get_view_by_type(Filter&& filter)
      -> std::expected<ConnectionView, Error> {
    for (ConnectionEntry& entry : socket_pool_) {
      if (entry.connection_type_ == end_point_e::NONE) {
        continue;
      }
      ConnectionMeta meta{.slot = entry.occupied_slot_,
                          .type = entry.connection_type_};
      if (!std::invoke(filter, meta)) {
        continue;
      }
      return ConnectionView{entry.occupied_slot_};
    }
    return std::unexpected(Error{.backtrace = {"No such View could be found"}});
  }

private:
  auto init_epoll() -> Ev;
  auto init_epoll_wrapper() -> Ev;
  template <typename... Args>
  auto emplace_new_entry_to_pool(Args&&...) -> std::expected<size_t, Error>;
  auto add_to_socket_pool() -> std::expected<size_t, Error>;
  auto add_to_socket_pool(int socket_fd, end_point_e socket_type)
      -> std::expected<size_t, Error>;
  auto free_slot_entry(ConnectionEntry& slot_entry) -> Ev;
  auto find_spot_for_new_client(int client_socket, end_point_e socket_type)
      -> std::expected<size_t, Error>;
  auto subscribe_to_events(ConnectionEntry& slot_entry) -> Ev;
  void unsubscribe_from_events(ConnectionEntry& slot_entry);
  void release_client(ConnectionEntry& slot_entry);
  auto register_client(int client_socket, end_point_e socket_type)
      -> std::expected<size_t, Error>;
  void register_clients(std::vector<int>& new_clients);
  void process_events(const std::vector<size_t>& event_slots);
  void process_server_socket(size_t slot);
  void process_client_socket(size_t slot);
  auto process_message(ConnectionEntry& slot_entry, ReadResult& message)
      -> CommandStatus;
  auto send_message_impl(ConnectionEntry& slot_entry,
                         const OutgoingMessage& message) -> bool;

  class Cleanup_Connection : public DeferredAction {
  public:
    explicit Cleanup_Connection(size_t slot) : slot_(slot) {}
    explicit Cleanup_Connection(ConnectionView view) : slot_(view.get_slot()) {}
    void prepare(NetworkEngine& engine) override;
    void execute(NetworkEngine& engine) override;

  private:
    size_t slot_;
  };

  ObjectPool<ConnectionEntry> socket_pool_;
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
