#pragma once

#include "../core/types.hpp"
#include "../core/result.hpp"
#include "../core/interfaces.hpp"

#include <memory>
#include <vector>
#include <functional>
#include <chrono>
#include <string>
#include <boost/asio.hpp>

namespace ss::network {

/**
 * @brief Transport error codes for network operations
 */
enum class transport_error {
    /// No error occurred
    none = 0,
    /// Network I/O operation failed
    network_failure,
    /// Connection was refused by remote host
    connection_refused,
    /// Operation timed out
    timeout,
    /// Host is unreachable
    host_unreachable,
    /// Port is unreachable
    port_unreachable,
    /// Connection was reset by peer
    connection_reset,
    /// Connection was aborted locally
    connection_aborted,
    /// No route to host
    no_route_to_host,
    /// Address already in use
    address_in_use,
    /// Address not available
    address_not_available,
    /// Message too large for transport
    message_too_large,
    /// Transport is not connected
    not_connected,
    /// Transport is already connected
    already_connected,
    /// Invalid argument provided
    invalid_argument,
    /// Permission denied
    permission_denied,
    /// Out of memory
    out_of_memory,
    /// Transport is shutting down
    shutdown_in_progress,
    /// Unknown error
    unknown
};

/**
 * @brief Convert transport_error to string representation
 * @param error Error code to convert
 * @return String description of the error
 */
std::string to_string(transport_error error) noexcept;

/**
 * @brief Transport statistics for monitoring and diagnostics
 */
struct transport_stats {
    /// Total bytes sent since transport creation
    std::uint64_t bytes_sent = 0;
    /// Total bytes received since transport creation
    std::uint64_t bytes_received = 0;
    /// Total messages sent since transport creation
    std::uint64_t messages_sent = 0;
    /// Total messages received since transport creation
    std::uint64_t messages_received = 0;
    /// Number of send errors encountered
    std::uint64_t send_errors = 0;
    /// Number of receive errors encountered
    std::uint64_t receive_errors = 0;
    /// Average send latency in microseconds
    std::uint64_t avg_send_latency_us = 0;
    /// Average receive latency in microseconds
    std::uint64_t avg_receive_latency_us = 0;
    /// Current number of active connections (for connection-based transports)
    std::uint32_t active_connections = 0;
    /// Maximum message size supported by transport
    std::uint32_t max_message_size = 0;
    /// Timestamp of last successful send operation
    std::chrono::steady_clock::time_point last_send_time{};
    /// Timestamp of last successful receive operation
    std::chrono::steady_clock::time_point last_receive_time{};
};

/**
 * @brief Transport configuration parameters
 */
struct transport_config {
    /// Maximum number of concurrent operations
    std::uint32_t max_concurrent_ops = 1000;
    /// Send buffer size in bytes
    std::uint32_t send_buffer_size = 65536;  // 64KB
    /// Receive buffer size in bytes  
    std::uint32_t receive_buffer_size = 65536;  // 64KB
    /// Default operation timeout
    std::chrono::milliseconds default_timeout{5000};
    /// Keep-alive interval for connection-based transports
    std::chrono::seconds keep_alive_interval{30};
    /// Enable statistics collection
    bool enable_stats = true;
    /// Enable detailed logging
    bool enable_logging = false;
    /// Maximum message size (0 = no limit)
    std::uint32_t max_message_size = 0;
    /// Number of I/O worker threads
    std::uint32_t io_threads = 1;
};

/**
 * @brief Incoming message data with metadata
 */
struct incoming_message {
    /// Message payload data
    std::vector<std::uint8_t> data;
    /// Sender endpoint
    ss::core::endpoint sender;
    /// Timestamp when message was received
    std::chrono::steady_clock::time_point received_at;
    /// Transport-specific metadata (optional)
    std::string metadata;

    /**
     * @brief Constructor
     * @param payload Message data
     * @param from Sender endpoint
     */
    incoming_message(std::vector<std::uint8_t> payload, ss::core::endpoint from)
        : data(std::move(payload))
        , sender(std::move(from))
        , received_at(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Outgoing message data with metadata
 */
struct outgoing_message {
    /// Message payload data
    std::vector<std::uint8_t> data;
    /// Target endpoint
    ss::core::endpoint target;
    /// Priority (higher values = higher priority)
    std::uint32_t priority = 0;
    /// Optional timeout override
    std::optional<std::chrono::milliseconds> timeout;
    /// Transport-specific metadata (optional)
    std::string metadata;

    /**
     * @brief Constructor
     * @param payload Message data
     * @param to Target endpoint
     */
    outgoing_message(std::vector<std::uint8_t> payload, ss::core::endpoint to)
        : data(std::move(payload))
        , target(std::move(to)) {}
};

/**
 * @brief Message handler callback type
 */
using message_handler = std::function<ss::core::async_void(incoming_message)>;

/**
 * @brief Send completion callback type
 */
using send_completion_handler = std::function<void(ss::core::result<void, transport_error>)>;

/**
 * @brief Interface for network transport layer
 * 
 * Provides abstraction over different transport protocols (UDP, TCP, QUIC, etc.)
 * with support for asynchronous operations, backpressure handling, and monitoring.
 * 
 * All transport implementations must be thread-safe for concurrent send/receive
 * operations from multiple threads.
 */
class i_transport : public ss::core::i_component {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~i_transport() = default;

    /**
     * @brief Bind transport to local endpoint for listening
     * @param local_endpoint Local endpoint to bind to
     * @return Awaitable result indicating success or error
     */
    virtual ss::core::async_result<ss::core::result<void, transport_error>>
    bind(const ss::core::endpoint& local_endpoint) = 0;

    /**
     * @brief Connect to remote endpoint (for connection-based transports)
     * @param remote_endpoint Remote endpoint to connect to
     * @return Awaitable result with established connection or error
     */
    virtual ss::core::async_result<ss::core::result<ss::core::endpoint, transport_error>>
    connect(const ss::core::endpoint& remote_endpoint) = 0;

    /**
     * @brief Send message asynchronously
     * @param message Message to send
     * @return Awaitable result indicating success or error
     */
    virtual ss::core::async_result<ss::core::result<void, transport_error>>
    send(outgoing_message message) = 0;

    /**
     * @brief Send message with completion callback
     * @param message Message to send
     * @param handler Completion callback
     */
    virtual void send_async(outgoing_message message, send_completion_handler handler) = 0;

    /**
     * @brief Receive next available message
     * @param timeout Optional timeout for receive operation
     * @return Awaitable result with message or timeout/error
     */
    virtual ss::core::async_result<ss::core::result<incoming_message, transport_error>>
    receive(std::optional<std::chrono::milliseconds> timeout = std::nullopt) = 0;

    /**
     * @brief Set message handler for incoming messages
     * @param handler Message handler callback
     */
    virtual void set_message_handler(message_handler handler) = 0;

    /**
     * @brief Remove current message handler
     */
    virtual void clear_message_handler() = 0;

    /**
     * @brief Get local endpoint that transport is bound to
     * @return Local endpoint or invalid endpoint if not bound
     */
    virtual ss::core::endpoint local_endpoint() const noexcept = 0;

    /**
     * @brief Get remote endpoint for connection-based transports
     * @return Remote endpoint or invalid endpoint if not connected
     */
    virtual ss::core::endpoint remote_endpoint() const noexcept = 0;

    /**
     * @brief Check if transport is connection-based
     * @return true for TCP/QUIC, false for UDP
     */
    virtual bool is_connection_based() const noexcept = 0;

    /**
     * @brief Check if transport is currently connected
     * @return true if connected/bound and ready for operations
     */
    virtual bool is_connected() const noexcept = 0;

    /**
     * @brief Get current transport statistics
     * @return Current statistics snapshot
     */
    virtual transport_stats get_stats() const noexcept = 0;

    /**
     * @brief Reset transport statistics
     */
    virtual void reset_stats() noexcept = 0;

    /**
     * @brief Get current transport configuration
     * @return Current configuration
     */
    virtual transport_config get_config() const noexcept = 0;

    /**
     * @brief Update transport configuration
     * @param config New configuration
     * @return Result indicating success or error
     */
    virtual ss::core::result<void, transport_error> set_config(const transport_config& config) = 0;

    /**
     * @brief Get maximum message size supported
     * @return Maximum message size in bytes (0 = unlimited)
     */
    virtual std::uint32_t max_message_size() const noexcept = 0;

    /**
     * @brief Check if transport can send more messages (backpressure)
     * @return true if send queue has space, false if backpressure active
     */
    virtual bool can_send() const noexcept = 0;

    /**
     * @brief Get approximate send queue size
     * @return Number of pending send operations
     */
    virtual std::uint32_t send_queue_size() const noexcept = 0;

    /**
     * @brief Get approximate receive queue size  
     * @return Number of pending incoming messages
     */
    virtual std::uint32_t receive_queue_size() const noexcept = 0;

    /**
     * @brief Disconnect and close transport (for connection-based transports)
     * @return Awaitable void result
     */
    virtual ss::core::async_void disconnect() = 0;

    /**
     * @brief Shutdown transport gracefully
     * @param timeout Maximum time to wait for graceful shutdown
     * @return Awaitable void result
     */
    virtual ss::core::async_void shutdown(std::chrono::milliseconds timeout = std::chrono::milliseconds{5000}) = 0;

protected:
    /**
     * @brief Protected default constructor
     */
    i_transport() = default;

    /**
     * @brief Protected copy constructor (deleted)
     */
    i_transport(const i_transport&) = delete;

    /**
     * @brief Protected move constructor (deleted)
     */
    i_transport(i_transport&&) = delete;

    /**
     * @brief Protected copy assignment (deleted)
     */
    i_transport& operator=(const i_transport&) = delete;

    /**
     * @brief Protected move assignment (deleted)
     */
    i_transport& operator=(i_transport&&) = delete;
};

/**
 * @brief Smart pointer type for transport instances
 */
using transport_ptr = std::shared_ptr<i_transport>;

/**
 * @brief Weak pointer type for transport instances
 */
using transport_weak_ptr = std::weak_ptr<i_transport>;

} // namespace ss::network