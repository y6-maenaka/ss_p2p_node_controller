#pragma once

#include "core/types.hpp"
#include "core/result.hpp"
#include "core/interfaces.hpp"
#include "network/i_transport.hpp"
#include "network/message_buffer.hpp"

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <atomic>
#include <future>
#include <unordered_map>
#include <boost/asio.hpp>

namespace ss {

/**
 * @brief Peer error codes for P2P operations
 */
enum class peer_error {
    /// No error occurred
    none = 0,
    /// Peer is not connected
    not_connected,
    /// Message send failed
    send_failed,
    /// Message receive failed  
    receive_failed,
    /// Operation timed out
    timeout,
    /// Invalid message format
    invalid_message,
    /// Peer is unreachable
    unreachable,
    /// Connection lost
    connection_lost,
    /// Authentication failed
    auth_failed,
    /// Rate limit exceeded
    rate_limited,
    /// Resource exhausted
    resource_exhausted,
    /// Unknown error
    unknown
};

/**
 * @brief Convert peer_error to string representation
 * @param error Error code to convert
 * @return String description of the error
 */
std::string to_string(peer_error error) noexcept;

/**
 * @brief Peer connection state
 */
enum class peer_state {
    /// Initial state, not connected
    disconnected,
    /// Attempting to establish connection
    connecting,
    /// Connected and ready for communication
    connected,
    /// Connection is being terminated
    disconnecting,
    /// Connection failed or lost
    failed
};

/**
 * @brief Convert peer_state to string representation  
 * @param state Peer state to convert
 * @return String description of the state
 */
std::string to_string(peer_state state) noexcept;

/**
 * @brief Peer statistics for monitoring and diagnostics
 */
struct peer_stats {
    /// Total messages sent to this peer
    std::uint64_t messages_sent = 0;
    /// Total messages received from this peer
    std::uint64_t messages_received = 0;
    /// Total bytes sent to this peer
    std::uint64_t bytes_sent = 0;
    /// Total bytes received from this peer
    std::uint64_t bytes_received = 0;
    /// Number of send errors
    std::uint64_t send_errors = 0;
    /// Number of receive errors
    std::uint64_t receive_errors = 0;
    /// Average round-trip time in microseconds
    std::uint64_t avg_rtt_us = 0;
    /// Last successful ping time
    std::chrono::steady_clock::time_point last_ping_time{};
    /// Last message send time
    std::chrono::steady_clock::time_point last_send_time{};
    /// Last message receive time
    std::chrono::steady_clock::time_point last_receive_time{};
    /// Connection establishment time
    std::chrono::steady_clock::time_point connected_at{};
    /// Current peer state
    peer_state current_state = peer_state::disconnected;
};

/**
 * @brief Peer configuration parameters
 */
struct peer_config {
    /// Maximum message queue size per peer
    std::uint32_t max_queue_size = 1000;
    /// Default send timeout
    std::chrono::milliseconds send_timeout{5000};
    /// Default receive timeout
    std::chrono::milliseconds receive_timeout{10000};
    /// Ping interval for keep-alive
    std::chrono::seconds ping_interval{30};
    /// Maximum ping timeout
    std::chrono::milliseconds ping_timeout{5000};
    /// Enable automatic reconnection
    bool auto_reconnect = true;
    /// Maximum reconnection attempts
    std::uint32_t max_reconnect_attempts = 3;
    /// Base reconnection delay
    std::chrono::seconds reconnect_base_delay{1};
    /// Enable statistics collection
    bool enable_stats = true;
    /// Enable keep-alive pings
    bool enable_keep_alive = true;
};

/**
 * @brief Message with peer context
 */
struct peer_message {
    /// Message payload
    std::vector<std::uint8_t> payload;
    /// Message type identifier
    std::string message_type;
    /// Message priority
    std::uint32_t priority = 0;
    /// Timestamp when message was created
    std::chrono::steady_clock::time_point timestamp;
    /// Optional metadata
    std::unordered_map<std::string, std::string> metadata;

    /**
     * @brief Constructor
     * @param data Message payload data
     * @param type Message type
     */
    peer_message(std::vector<std::uint8_t> data, std::string type = "default")
        : payload(std::move(data))
        , message_type(std::move(type))
        , timestamp(std::chrono::steady_clock::now()) {}

    /**
     * @brief Move constructor
     */
    peer_message(peer_message&&) = default;

    /**
     * @brief Move assignment
     */
    peer_message& operator=(peer_message&&) = default;

    /**
     * @brief Copy constructor (deleted for performance)
     */
    peer_message(const peer_message&) = delete;

    /**
     * @brief Copy assignment (deleted for performance)
     */
    peer_message& operator=(const peer_message&) = delete;
};

/**
 * @brief Message handler callback type
 */
using message_handler = std::function<core::async_void(peer_message)>;

/**
 * @brief Connection state change handler callback type
 */
using state_change_handler = std::function<void(peer_state, peer_state)>;

/**
 * @brief Error handler callback type
 */
using error_handler = std::function<void(peer_error, const std::string&)>;

/**
 * @brief Modern type-safe peer implementation
 * 
 * Provides efficient peer-to-peer communication with the following features:
 * - Type-safe messaging using core::peer_id
 * - Async/await support with coroutines
 * - Automatic connection management and keep-alive
 * - Comprehensive error handling with Result<T,E>
 * - Zero-copy message buffers where possible
 * - Built-in statistics and monitoring
 * - Configurable timeouts and retry logic
 * - Thread-safe operations
 */
class peer : public core::i_component {
public:
    /**
     * @brief Smart pointer type for peer instances
     */
    using ptr = std::shared_ptr<peer>;
    
    /**
     * @brief Reference type for peer instances
     */
    using ref = peer&;

    /**
     * @brief Weak pointer type for peer instances
     */
    using weak_ptr = std::weak_ptr<peer>;

    /**
     * @brief Constructor
     * @param id Unique peer identifier
     * @param endpoint Peer network endpoint
     * @param transport Transport layer for communication
     * @param io_ctx IO context for async operations
     * @param config Peer configuration
     */
    peer(core::peer_id id, 
         core::endpoint endpoint,
         network::transport_ptr transport,
         boost::asio::io_context& io_ctx,
         const peer_config& config = {});

    /**
     * @brief Destructor
     */
    ~peer() override;

    /**
     * @brief Copy constructor (deleted)
     */
    peer(const peer&) = delete;

    /**
     * @brief Move constructor (deleted)
     */
    peer(peer&&) = delete;

    /**
     * @brief Copy assignment (deleted)
     */
    peer& operator=(const peer&) = delete;

    /**
     * @brief Move assignment (deleted)
     */
    peer& operator=(peer&&) = delete;

    // Core identity and endpoint access

    /**
     * @brief Get peer identifier
     * @return Peer ID
     */
    const core::peer_id& id() const noexcept { return peer_id_; }

    /**
     * @brief Get peer endpoint
     * @return Network endpoint
     */
    const core::endpoint& endpoint() const noexcept { return endpoint_; }

    /**
     * @brief Get current peer state
     * @return Current connection state
     */
    peer_state state() const noexcept;

    /**
     * @brief Check if peer is connected
     * @return true if connected and ready for communication
     */
    bool is_connected() const noexcept;

    // Connection management

    /**
     * @brief Establish connection to peer
     * @param timeout Connection timeout
     * @return Awaitable result indicating success or error
     */
    core::async_result<core::result<void, peer_error>>
    connect(std::chrono::milliseconds timeout = std::chrono::milliseconds{5000});

    /**
     * @brief Disconnect from peer
     * @param graceful Whether to perform graceful shutdown
     * @return Awaitable void result
     */
    core::async_void disconnect(bool graceful = true);

    // Message operations

    /**
     * @brief Send message to peer asynchronously
     * @param message Message to send
     * @return Awaitable result indicating success or error
     */
    core::async_result<core::result<void, peer_error>>
    send(peer_message message);

    /**
     * @brief Send message with custom timeout
     * @param message Message to send
     * @param timeout Send timeout
     * @return Awaitable result indicating success or error
     */
    core::async_result<core::result<void, peer_error>>
    send_timeout(peer_message message, std::chrono::milliseconds timeout);

    /**
     * @brief Receive next available message from peer
     * @param timeout Receive timeout
     * @return Awaitable result with message or timeout/error
     */
    core::async_result<core::result<peer_message, peer_error>>
    receive(std::chrono::milliseconds timeout = std::chrono::milliseconds{10000});

    /**
     * @brief Try to receive message without blocking
     * @return Result with message or would_block error
     */
    core::result<peer_message, peer_error> try_receive();

    // Request-response operations

    /**
     * @brief Send request and wait for response
     * @param request Request message
     * @param timeout Total timeout for request-response cycle
     * @return Awaitable result with response or timeout/error
     */
    core::async_result<core::result<peer_message, peer_error>>
    request(peer_message request, std::chrono::milliseconds timeout = std::chrono::milliseconds{10000});

    /**
     * @brief Send response to a received request
     * @param response Response message
     * @param request_id ID of the original request
     * @return Awaitable result indicating success or error  
     */
    core::async_result<core::result<void, peer_error>>
    respond(peer_message response, core::message_id request_id);

    // Keep-alive and health monitoring

    /**
     * @brief Send ping to peer and measure round-trip time
     * @param timeout Ping timeout
     * @return Awaitable result with RTT or timeout/error
     */
    core::async_result<core::result<std::chrono::microseconds, peer_error>>
    ping(std::chrono::milliseconds timeout = std::chrono::milliseconds{5000});

    /**
     * @brief Check if peer is reachable (non-blocking)
     * @return true if peer responded to recent pings
     */
    bool is_alive() const noexcept;

    // Event handlers

    /**
     * @brief Set message handler for incoming messages
     * @param handler Message handler callback
     */
    void set_message_handler(message_handler handler);

    /**
     * @brief Set state change handler
     * @param handler State change callback
     */
    void set_state_change_handler(state_change_handler handler);

    /**
     * @brief Set error handler
     * @param handler Error callback
     */
    void set_error_handler(error_handler handler);

    /**
     * @brief Clear all event handlers
     */
    void clear_handlers();

    // Statistics and monitoring

    /**
     * @brief Get current peer statistics
     * @return Current statistics snapshot
     */
    peer_stats get_stats() const;

    /**
     * @brief Reset peer statistics
     */
    void reset_stats();

    /**
     * @brief Get current configuration
     * @return Current peer configuration
     */
    peer_config get_config() const;

    /**
     * @brief Update peer configuration
     * @param config New configuration
     * @return Result indicating success or error
     */
    core::result<void, peer_error> set_config(const peer_config& config);

    // Queue management

    /**
     * @brief Get current send queue size
     * @return Number of pending outgoing messages
     */
    std::uint32_t send_queue_size() const noexcept;

    /**
     * @brief Get current receive queue size
     * @return Number of pending incoming messages
     */
    std::uint32_t receive_queue_size() const noexcept;

    /**
     * @brief Check if send queue has space
     * @return true if can accept more messages
     */
    bool can_send() const noexcept;

    /**
     * @brief Clear all pending messages
     */
    void clear_queues();

    // i_component interface implementation

    /**
     * @brief Start peer operations
     * @return Awaitable result indicating success or error
     */
    core::async_void start() override;

    /**
     * @brief Stop peer operations
     * @return Awaitable void result
     */
    core::async_void stop() override;

    /**
     * @brief Get component name
     * @return Component name string
     */
    std::string name() const noexcept override { return "peer"; }

    /**
     * @brief Check if peer is running
     * @return true if peer is operational
     */
    bool is_running() const noexcept override { 
        return current_state_.load() == peer_state::connected; 
    }

private:
    // Core member variables
    core::peer_id peer_id_;
    core::endpoint endpoint_;
    network::transport_ptr transport_;
    boost::asio::io_context& io_context_;
    peer_config config_;

    // State management
    mutable std::mutex state_mutex_;
    std::atomic<peer_state> current_state_{peer_state::disconnected};
    
    // Message handling
    std::unique_ptr<network::message_buffer> send_buffer_;
    std::unique_ptr<network::message_buffer> receive_buffer_;
    
    // Event handlers
    mutable std::mutex handlers_mutex_;
    message_handler message_handler_;
    state_change_handler state_change_handler_;
    error_handler error_handler_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    peer_stats stats_;
    
    // Keep-alive management
    std::unique_ptr<boost::asio::steady_timer> keep_alive_timer_;
    std::atomic<std::chrono::steady_clock::time_point> last_activity_time_;
    
    // Request-response tracking
    mutable std::mutex pending_requests_mutex_;
    std::unordered_map<core::message_id, std::promise<peer_message>> pending_requests_;
    
    // Internal operations
    void update_state(peer_state new_state);
    void handle_incoming_message(network::incoming_message message);
    void handle_transport_error(network::transport_error error);
    void start_keep_alive();
    void stop_keep_alive();
    core::async_void keep_alive_loop();
    void update_stats_on_send(std::size_t bytes);
    void update_stats_on_receive(std::size_t bytes);
    void handle_error(peer_error error, const std::string& details);
    core::message_id generate_request_id();
    bool is_request_response(const peer_message& message) const;
    std::optional<core::message_id> extract_request_id(const peer_message& message) const;
    void complete_pending_request(core::message_id request_id, peer_message response);
};

/**
 * @brief Factory function to create peer instances
 * @param id Peer identifier
 * @param endpoint Peer endpoint
 * @param transport Transport layer
 * @param io_ctx IO context for async operations
 * @param config Peer configuration
 * @return Shared pointer to peer instance
 */
inline peer::ptr make_peer(core::peer_id id,
                          core::endpoint endpoint,
                          network::transport_ptr transport,
                          boost::asio::io_context& io_ctx,
                          const peer_config& config = {}) {
    return std::make_shared<peer>(std::move(id), std::move(endpoint), 
                                 std::move(transport), io_ctx, config);
}

} // namespace ss

/**
 * @brief Hash specialization for peer_error
 */
template<>
struct std::hash<ss::peer_error> {
    std::size_t operator()(ss::peer_error error) const noexcept {
        return std::hash<int>{}(static_cast<int>(error));
    }
};

/**
 * @brief Hash specialization for peer_state
 */
template<>
struct std::hash<ss::peer_state> {
    std::size_t operator()(ss::peer_state state) const noexcept {
        return std::hash<int>{}(static_cast<int>(state));
    }
};