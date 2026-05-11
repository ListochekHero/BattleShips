#ifndef NETWORK_ROUTINE_H
#define NETWORK_ROUTINE_H

#include "core/atomic_queue.h"
#include "core/object_pool.h"
#include "deferred_actions.h"
#include "interfaces/modules.h"
#include "protocol/message_types.h"
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
#include <optional>
#include <string>
#include <vector>

#define MAX_EVENTS 10

namespace bsm {

enum class task_tag_e : uint8_t;
struct OutgoingMessage;
struct ReceivedMessage;

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
  using MessageHandler = std::function<ActionResult(const ActionContext&)>;
  void set_message_handler(MessageHandler message_handler);
  void run_event_loop();
  auto send_message_to(const ConnectionView& recipient_view,
                       const OutgoingMessage& outgoing_message)
      -> std::optional<Error>;
  auto attach_socket(int socket, end_point_e socket_type)
      -> std::expected<ConnectionView, Error>;
  auto transfer(const ConnectionView& destination_view,
                const ConnectionView& source_view) -> TransferResult;
  auto process_connection(const ConnectionView& pending_view)
      -> std::optional<Error>;
  auto reset_base_connection(end_point_e socket_type) -> std::optional<Error>;
  void release_connection_by_view(ConnectionView view_to_release);

  template <typename Filter>
    requires std::predicate<Filter, const ConnectionMeta&>
  auto send_message(const OutgoingMessage& message, Filter&& filter)
      -> DeliveryReport {
    DeliveryReport delivery_report;
    size_t current_slot{0};
    auto snapshot{connection_pool_.get_vector()};
    for (auto* entry : snapshot) {
      if (entry->connection_type_ != end_point_e::NONE) {
        ConnectionMeta meta{
            .slot = current_slot,
            .type = entry->connection_type_,
        };
        if (std::invoke(filter, meta)) {
          auto send_error{send_message_impl(*entry, message)};
          if (send_error) {
            LOG(send_error->full_report());
            entry->reset();
            connection_pool_.release(current_slot);
            delivery_report.failed++;
          } else {
            delivery_report.delivered++;
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
    for (auto* entry : connection_pool_) {
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
  auto init_epoll() -> std::optional<Error>;
  static auto init_connection_type(end_point_e socket_type,
                                   ConnectionEntry& connection)
      -> std::optional<Error>;
  auto subscribe_to_events(ConnectionEntry& connection, size_t pool_slot)
      -> std::optional<Error>;
  auto unsubscribe_from_events(ConnectionEntry& connection)
      -> std::optional<Error>;
  void release_connection(ConnectionEntry& connection, size_t pool_slot);
  auto register_connection(end_point_e socket_type, int client_socket)
      -> std::expected<size_t, Error>;
  auto register_connections(std::vector<int>& new_sockets)
      -> RegistrationReport;
  void process_events(const std::vector<size_t>& event_slots);
  void process_server_socket(const ConnectionEntry& server_connection,
                             size_t server_slot);
  auto process_connection_impl(size_t pool_slot) -> std::optional<Error>;
  auto process_message(ConnectionEntry& pending_connection, size_t pool_slot,
                       ReceivedMessage& received_message) -> ActionResult;
  static auto send_message_impl(ConnectionEntry& connection_entry,
                                const OutgoingMessage& message)
      -> std::optional<Error>;

  ObjectPool<ConnectionEntry> connection_pool_;
  EpollHandler epoll_handler_;
  MessageHandler on_message_callback_;

  AtomicQueue<size_t>& network_raw_tasks_;
  AtomicQueue<task_tag_e>& available_task_tags_;
};

} // namespace bsm

#endif
