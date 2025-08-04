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
#include <unordered_set>

namespace ss::network::impl {

/**
 * @brief TCP connection wrapper for managing individual connections
 */
class tcp_connection : public std::enable_shared_from_this<tcp_connection> {
public:
    using socket_type = boost::asio::ip::tcp::socket;
    using endpoint_type = boost::asio::ip::tcp::endpoint;

    /**
     * @brief Constructor
     * @param socket TCP socket for this connection
     * @param transport Parent transport reference
     */
    tcp_connection(socket_type socket, class tcp_transport& transport);

    /**
     * @brief Destructor
     */
    ~tcp_connection();

    /**
     * @brief Start connection (begin reading)
     */
    void start();

    /**
     * @brief Stop connection gracefully
     */
    ss::core::async_void stop();

    /**
     * @brief Send message through this connection
     * @param data Message data to send
     * @return Awaitable result indicating success or error
     */
    ss::core::async_result<ss::core::result<void, transport_error>>
    send(const std::vector<std::uint8_t>& data);

    /**
     * @brief Get remote endpoint
     * @return Remote endpoint of this connection
     */
    ss::core::endpoint remote_endpoint() const noexcept;

    /**
     * @brief Check if connection is active
     * @return true if connection is active
     */
    bool is_active() const noexcept { return active_.load(); }

    /**
     * @brief Get connection ID
     * @return Unique connection identifier
     */
    std::uint64_t connection_id() const noexcept { return connection_id_; }

private:
    /// Connection ID counter
    static std::atomic<std::uint64_t> next_connection_id_;
    
    /// Unique connection ID
    std::uint64_t connection_id_;
    
    /// TCP socket
    socket_type socket_;
    
    /// Parent transport reference
    tcp_transport& transport_;
    
    /// Connection active state
    std::atomic<bool> active_;
    
    /// Remote endpoint
    ss::core::endpoint remote_endpoint_;
    
    /// Read buffer
    std::array<std::uint8_t, 65536> read_buffer_;
    
    /// Message length buffer (for framing)
    std::array<std::uint8_t, 4> length_buffer_;
    
    /// Current message buffer
    std::vector<std::uint8_t> message_buffer_;
    
    /// Expected message length
    std::uint32_t expected_length_;
    
    /// Bytes received for current message
    std::uint32_t bytes_received_;
    
    /// Send mutex
    mutable std::mutex send_mutex_;

    /**
     * @brief Start reading message length
     */
    void start_read_length();

    /**
     * @brief Handle message length read completion
     * @param error Error code
     * @param bytes_transferred Bytes read
     */
    void handle_read_length(const boost::system::error_code& error, std::size_t bytes_transferred);

    /**
     * @brief Start reading message data
     */
    void start_read_data();

    /**
     * @brief Handle message data read completion
     * @param error Error code
     * @param bytes_transferred Bytes read
     */
    void handle_read_data(const boost::system::error_code& error, std::size_t bytes_transferred);

    /**
     * @brief Handle complete message
     */
    void handle_complete_message();

    /**
     * @brief Handle connection error
     * @param error Error code
     */
    void handle_error(const boost::system::error_code& error);
};

using tcp_connection_ptr = std::shared_ptr<tcp_connection>;

/**
 * @brief TCP transport implementation using Boost.Asio
 * 
 * Provides connection-oriented TCP transport with support for multiple
 * concurrent connections, message framing, and connection management.
 */
class tcp_transport : public i_transport {
public:
    /**
     * @brief Constructor
     * @param io_context IO context for async operations
     * @param config Transport configuration
     */
    explicit tcp_transport(ss::core::io_context& io_context, 
                          const transport_config& config = {});

    /**
     * @brief Destructor
     */
    ~tcp_transport() override;

    // i_component interface implementation
    std::string name() const noexcept override { return "tcp_transport"; }
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
    bool is_connection_based() const noexcept override { return true; }
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

    /**
     * @brief Handle incoming message from connection
     * @param message Received message
     * @param connection_id Source connection ID
     */
    void handle_incoming_message(incoming_message message, std::uint64_t connection_id);

    /**
     * @brief Handle connection closed
     * @param connection_id Closed connection ID
     */
    void handle_connection_closed(std::uint64_t connection_id);

private:
    /// Socket type aliases
    using acceptor_type = boost::asio::ip::tcp::acceptor;
    using socket_type = boost::asio::ip::tcp::socket;
    using endpoint_type = boost::asio::ip::tcp::endpoint;

    /// IO context reference
    ss::core::io_context& io_context_;
    
    /// TCP acceptor (for server mode)
    std::unique_ptr<acceptor_type> acceptor_;
    
    /// Active connections map
    std::unordered_map<std::uint64_t, tcp_connection_ptr> connections_;
    
    /// Primary connection (for client mode)
    tcp_connection_ptr primary_connection_;
    
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
    
    /// Local bound endpoint
    ss::core::endpoint local_endpoint_;
    
    /// Remote connected endpoint (for client mode)
    ss::core::endpoint remote_endpoint_;
    
    /// Running state
    std::atomic<bool> running_;
    
    /// Bound state
    std::atomic<bool> bound_;
    
    /// Connected state
    std::atomic<bool> connected_;
    
    /// Shutdown requested
    std::atomic<bool> shutdown_requested_;
    
    /// Server mode (true) vs client mode (false)
    std::atomic<bool> server_mode_;
    
    /// Thread safety mutex
    mutable std::mutex mutex_;
    
    /// Send queue
    std::queue<std::pair<outgoing_message, send_completion_handler>> send_queue_;
    
    /// Send operation in progress
    std::atomic<bool> send_in_progress_;
    
    /// I/O worker threads
    std::vector<std::thread> io_threads_;
    
    /// Keep-alive timer
    std::unique_ptr<ss::core::steady_timer> keep_alive_timer_;

    /**
     * @brief Start accepting connections (server mode)
     */
    void start_accept();

    /**
     * @brief Handle new connection accepted
     * @param error Error code from accept operation
     * @param socket New connection socket
     */
    void handle_accept(const boost::system::error_code& error, socket_type socket);

    /**
     * @brief Process next send operation from queue
     */
    boost::asio::awaitable<void> process_send_queue();

    /**
     * @brief Find connection for target endpoint
     * @param target Target endpoint
     * @return Connection pointer or nullptr if not found
     */
    tcp_connection_ptr find_connection(const ss::core::endpoint& target);

    /**
     * @brief Create new outgoing connection
     * @param target Target endpoint
     * @return Connection pointer or nullptr on failure
     */
    ss::core::async_result<tcp_connection_ptr> create_connection(const ss::core::endpoint& target);

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
     * @brief Apply socket options based on configuration
     * @param socket Socket to configure
     */
    ss::core::result<void, transport_error> apply_socket_options(socket_type& socket);

    /**
     * @brief Start keep-alive timer
     */
    void start_keep_alive_timer();

    /**
     * @brief Handle keep-alive timeout
     */
    void handle_keep_alive_timeout();

    /**
     * @brief Send keep-alive messages to all connections
     */
    void send_keep_alive_messages();

    /**
     * @brief Cleanup inactive connections
     */
    void cleanup_inactive_connections();

    /**
     * @brief Frame message with length prefix
     * @param data Message data
     * @return Framed message with length prefix
     */
    std::vector<std::uint8_t> frame_message(const std::vector<std::uint8_t>& data) const;

    friend class tcp_connection;
};

} // namespace ss::network::impl