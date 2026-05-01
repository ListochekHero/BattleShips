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
  auto init_engine(end_point_e socket_type, int root_socket)
      -> std::expected<ConnectionView, Error>;
  using MessageHandler = std::function<CommandStatus(const CommandContext&)>;
  void set_message_handler(MessageHandler message_handler);
  void run_event_loop();
  auto send_message_to(const ConnectionView& recipient_view,
                       const OutgoingMessage& outgoing_message)
      -> std::optional<Error>;
  auto attach_socket(int socket, end_point_e socket_type)
      -> std::expected<ConnectionView, Error>;
  auto transfer(const ConnectionView& destination_view,
                const ConnectionView& source_view) -> CommandStatus;
  void process_client(const ConnectionView& view);

  template <typename Filter>
    requires std::predicate<Filter, const ConnectionMeta&>
  auto send_message(const OutgoingMessage& message, Filter&& filter)
      -> DeliveryReport {
    DeliveryReport delivery_report;
    for (auto* entry : socket_pool_) {
      size_t current_slot{0};
      if (entry->connection_type_ != end_point_e::NONE) {
        ConnectionMeta meta{
            .slot = current_slot,
            .type = entry->connection_type_,
        };
        if (std::invoke(filter, meta)) {
          if (send_message_impl(*entry, message)) {
            delivery_report.delivered++;
          } else {
            entry->reset();
            socket_pool_.release(current_slot);
            delivery_report.failed++;
          }
        }
      }
      current_slot++;
    }
    return delivery_report;
  }

  template <typename Filter>
    requires std::predicate<Filter, const ConnectionMeta&>
  auto get_view_by_type(Filter&& filter)
      -> std::expected<ConnectionView, Error> {
    for (auto* entry : socket_pool_) {
      size_t current_slot{0};
      if (entry->connection_type_ != end_point_e::NONE) {
        ConnectionMeta meta{
            .slot = current_slot,
            .type = entry->connection_type_,
        };
        if (std::invoke(filter, meta)) {
          return ConnectionView{current_slot};
        }
      }
      current_slot++;
    }
    return std::unexpected(Error{.backtrace = {"No such View could be found"}});
  }

private:
  auto init_epoll() -> Ev;
  auto subscribe_to_events(ConnectionEntry& slot_entry, size_t slot) -> Ev;
  void unsubscribe_from_events(ConnectionEntry& slot_entry);
  void release_client(ConnectionEntry& slot_entry, size_t slot);
  auto register_client(end_point_e socket_type, int client_socket)
      -> std::expected<size_t, Error>;
  void register_clients(std::vector<int>& new_clients);
  void process_events(const std::vector<size_t>& event_slots);
  void process_server_socket(size_t slot);
  void process_client_socket(size_t slot);
  auto process_message(ConnectionEntry& slot_entry, size_t slot,
                       ReadResult& message) -> CommandStatus;
  static auto send_message_impl(ConnectionEntry& connection_entry,
                                const OutgoingMessage& message)
      -> std::optional<Error>;

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
