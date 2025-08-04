#pragma once

#include "../i_transport.hpp"
#include "../message_buffer.hpp"
#include "../../core/types.hpp"

#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <unordered_map>

namespace ss::network::impl {

/**
 * @brief UDP transport implementation using Boost.Asio
 * 
 * Provides connectionless UDP transport with support for asynchronous
 * operations, message fragmentation, and flow control.
 */
class udp_transport : public i_transport {
public:
    /**
     * @brief Constructor
     * @param io_context IO context for async operations
     * @param config Transport configuration
     */
    explicit udp_transport(ss::core::io_context& io_context, 
                          const transport_config& config = {});

    /**
     * @brief Destructor
     */
    ~udp_transport() override;

    // i_component interface implementation
    std::string name() const noexcept override { return "udp_transport"; }
    std::string version() const noexcept override { return "1.0.0"; }
    bool is_healthy() const noexcept override;
    std::string status() const override;
    ss::core::async_void initialize() override;
    ss::core::async_void cleanup() override;
    ss::core::async_void start() override;
    ss::core::async_void stop() override;
    bool is_running() const noexcept override;

    // i_transport interface implementation
    ss::core::async_result<ss::core::result<void, transport_error>>
    bind(const ss::core::endpoint& local_endpoint) override;

    ss::core::async_result<ss::core::result<ss::core::endpoint, transport_error>>
    connect(const ss::core::endpoint& remote_endpoint) override;

    ss::core::async_result<ss::core::result<void, transport_error>>
    send(outgoing_message message) override;

    void send_async(outgoing_message message, send_completion_handler handler) override;

    ss::core::async_result<ss::core::result<incoming_message, transport_error>>
    receive(std::optional<std::chrono::milliseconds> timeout = std::nullopt) override;

    void set_message_handler(message_handler handler) override;
    void clear_message_handler() override;

    ss::core::endpoint local_endpoint() const noexcept override;
    ss::core::endpoint remote_endpoint() const noexcept override;
    bool is_connection_based() const noexcept override { return false; }
    bool is_connected() const noexcept override;

    transport_stats get_stats() const noexcept override;
    void reset_stats() noexcept override;

    transport_config get_config() const noexcept override;
    ss::core::result<void, transport_error> set_config(const transport_config& config) override;

    std::uint32_t max_message_size() const noexcept override;
    bool can_send() const noexcept override;
    std::uint32_t send_queue_size() const noexcept override;
    std::uint32_t receive_queue_size() const noexcept override;

    ss::core::async_void disconnect() override;
    ss::core::async_void shutdown(std::chrono::milliseconds timeout = std::chrono::milliseconds{5000}) override;

private:
    /// Socket type alias
    using socket_type = boost::asio::ip::udp::socket;
    using endpoint_type = boost::asio::ip::udp::endpoint;

    /// IO context reference
    ss::core::io_context& io_context_;
    
    /// UDP socket
    std::unique_ptr<socket_type> socket_;
    
    /// Transport configuration
    transport_config config_;
    
    /// Transport statistics
    mutable transport_stats stats_;
    
    /// Message handler
    message_handler message_handler_;
    
    /// Message buffer for incoming messages
    std::unique_ptr<message_buffer> incoming_buffer_;
    
    /// Message buffer for outgoing messages
    std::unique_ptr<message_buffer> outgoing_buffer_;
    
    /// Receive buffer
    std::array<std::uint8_t, 65536> receive_buffer_;
    
    /// Sender endpoint for current receive operation
    endpoint_type sender_endpoint_;
    
    /// Local bound endpoint
    ss::core::endpoint local_endpoint_;
    
    /// Remote connected endpoint (for pseudo-connection mode)
    ss::core::endpoint remote_endpoint_;
    
    /// Running state
    std::atomic<bool> running_;
    
    /// Bound state
    std::atomic<bool> bound_;
    
    /// Connected state (pseudo-connection for UDP)
    std::atomic<bool> connected_;
    
    /// Shutdown requested
    std::atomic<bool> shutdown_requested_;
    
    /// Thread safety mutex
    mutable std::mutex mutex_;
    
    /// Send queue
    std::queue<std::pair<outgoing_message, send_completion_handler>> send_queue_;
    
    /// Send operation in progress
    std::atomic<bool> send_in_progress_;
    
    /// Receive operation in progress
    std::atomic<bool> receive_in_progress_;
    
    /// I/O worker threads
    std::vector<std::thread> io_threads_;

    /**
     * @brief Start receive operation
     */
    void start_receive();

    /**
     * @brief Handle received data
     * @param error Error code from receive operation
     * @param bytes_received Number of bytes received
     */
    void handle_receive(const boost::system::error_code& error, std::size_t bytes_received);

    /**
     * @brief Process next send operation from queue
     */
    boost::asio::awaitable<void> process_send_queue();

    /**
     * @brief Handle send completion
     * @param error Error code from send operation
     * @param bytes_sent Number of bytes sent
     * @param handler Completion handler to call
     */
    void handle_send(const boost::system::error_code& error, 
                    std::size_t bytes_sent,
                    send_completion_handler handler);

    /**
     * @brief Convert Boost.Asio error to transport error
     * @param error Boost.Asio error code
     * @return Corresponding transport error
     */
    transport_error convert_error(const boost::system::error_code& error) const noexcept;

    /**
     * @brief Convert Boost.Asio endpoint to ss::core::endpoint
     * @param ep Boost.Asio endpoint
     * @return ss::core::endpoint
     */
    ss::core::endpoint convert_endpoint(const endpoint_type& ep) const noexcept;

    /**
     * @brief Convert ss::core::endpoint to Boost.Asio endpoint
     * @param ep ss::core::endpoint
     * @return Boost.Asio endpoint
     */
    endpoint_type convert_endpoint(const ss::core::endpoint& ep) const noexcept;

    /**
     * @brief Update transport statistics
     * @param bytes_sent Number of bytes sent (0 if receive operation)
     * @param bytes_received Number of bytes received (0 if send operation)
     * @param send_error true if send error occurred
     * @param receive_error true if receive error occurred
     */
    void update_stats(std::size_t bytes_sent, std::size_t bytes_received,
                     bool send_error = false, bool receive_error = false) noexcept;

    /**
     * @brief Calculate latency for operation
     * @param start_time Operation start time
     * @return Latency in microseconds
     */
    std::uint64_t calculate_latency(const std::chrono::steady_clock::time_point& start_time) const noexcept;

    /**
     * @brief Start I/O worker threads
     */
    void start_io_threads();

    /**
     * @brief Stop I/O worker threads
     */
    void stop_io_threads();

    /**
     * @brief I/O worker thread function
     */
    void io_worker();

    /**
     * @brief Check if message size is valid
     * @param size Message size to check
     * @return true if valid, false otherwise
     */
    bool is_valid_message_size(std::size_t size) const noexcept;

    /**
     * @brief Apply socket options based on configuration
     */
    ss::core::result<void, transport_error> apply_socket_options();

    /**
     * @brief Fragment large message if needed
     * @param message Message to potentially fragment
     * @return Vector of message fragments
     */
    std::vector<outgoing_message> fragment_message(const outgoing_message& message) const;

    /**
     * @brief Defragment received message fragments
     * @param fragment Message fragment
     * @return Complete message if defragmentation finished, nullopt otherwise
     */
    std::optional<incoming_message> defragment_message(const incoming_message& fragment);

    /// Fragment reassembly state
    struct fragment_state {
        std::vector<std::uint8_t> data;
        std::size_t expected_size = 0;
        std::size_t received_size = 0;
        std::chrono::steady_clock::time_point first_fragment_time;
        std::unordered_map<std::uint32_t, bool> received_fragments;
    };

    /// Fragment reassembly map (keyed by fragment ID)
    std::unordered_map<std::uint32_t, fragment_state> fragment_map_;

    /// Fragment cleanup timer
    std::unique_ptr<ss::core::steady_timer> fragment_cleanup_timer_;

    /**
     * @brief Clean up expired fragments
     */
    void cleanup_expired_fragments();

    /**
     * @brief Start fragment cleanup timer
     */
    void start_fragment_cleanup_timer();
};

} // namespace ss::network::impl