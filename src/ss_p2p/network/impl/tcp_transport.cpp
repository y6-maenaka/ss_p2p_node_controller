#include "../../../../include/ss_p2p/network/impl/tcp_transport.hpp"
#include "../../../../include/logger/logger.hpp"
#include "../../../../include/json.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <algorithm>
#include <cstring>

namespace ss::network::impl {

namespace {
    /// Default TCP message size limit (16MB)
    constexpr std::uint32_t DEFAULT_MAX_TCP_MESSAGE_SIZE = 16 * 1024 * 1024;
    
    /// Keep-alive message type
    constexpr std::uint32_t KEEP_ALIVE_MESSAGE_TYPE = 0xFFFFFFFF;
    
    /// Message framing overhead (4 bytes for length prefix)
    constexpr std::size_t MESSAGE_FRAME_OVERHEAD = 4;
}

// tcp_connection implementation

std::atomic<std::uint64_t> tcp_connection::next_connection_id_{1};

tcp_connection::tcp_connection(socket_type socket, tcp_transport& transport)
    : connection_id_(next_connection_id_.fetch_add(1))
    , socket_(std::move(socket))
    , transport_(transport)
    , active_(false)
    , expected_length_(0)
    , bytes_received_(0) {
    
    try {
        remote_endpoint_ = transport.convert_endpoint(socket_.remote_endpoint());
        // LOG_DEBUG("TCP connection {} created for {}", connection_id_, remote_endpoint_.to_string());
    } catch (const std::exception& e) {
        // LOG_ERROR("Failed to get remote endpoint for connection {}: {}", connection_id_, e.what());
    }
}

tcp_connection::~tcp_connection() {
    if (active_.load()) {
        // LOG_DEBUG("TCP connection {} destroyed while active", connection_id_);
    }
}

void tcp_connection::start() {
    if (active_.exchange(true)) {
        return;
    }
    
        // LOG_INFO("Starting TCP connection {} with {}", connection_id_, remote_endpoint_.to_string());
    
    // Start reading first message length
    start_read_length();
}

ss::core::async_void tcp_connection::stop() {
    if (!active_.exchange(false)) {
        co_return;
    }
    
        // LOG_INFO("Stopping TCP connection {} with {}", connection_id_, remote_endpoint_.to_string());
    
    try {
        if (socket_.is_open()) {
            // Graceful shutdown
            socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
            socket_.close();
        }
    } catch (const std::exception& e) {
        // LOG_ERROR("Error stopping TCP connection {}: {}", connection_id_, e.what());
    }
    
    // Notify transport that connection is closed
    transport_.handle_connection_closed(connection_id_);
}

ss::core::async_result<ss::core::result<void, transport_error>>
tcp_connection::send(const std::vector<std::uint8_t>& data) {
    if (!active_.load()) {
        co_return ss::core::result<void, transport_error>::err(transport_error::not_connected);
    }
    
    try {
        std::lock_guard<std::mutex> lock(send_mutex_);
        
        // Frame message with length prefix
        auto framed_data = transport_.frame_message(data);
        
        // Send framed message
        co_await boost::asio::async_write(
            socket_,
            boost::asio::buffer(framed_data),
            boost::asio::use_awaitable
        );
        
        // LOG_TRACE("Sent {} bytes to connection {}", data.size(), connection_id_);
        
        co_return ss::core::result<void, transport_error>::ok();
        
    } catch (const boost::system::system_error& e) {
        auto error = transport_.convert_error(e.code());
        // LOG_ERROR("Failed to send {} bytes to connection {}: {}", 
        //          data.size(), connection_id_, e.what());
        
        // Handle connection error
        handle_error(e.code());
        
        co_return ss::core::result<void, transport_error>::err(error);
    }
}

ss::core::endpoint tcp_connection::remote_endpoint() const noexcept {
    return remote_endpoint_;
}

void tcp_connection::start_read_length() {
    if (!active_.load()) {
        return;
    }
    
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(length_buffer_),
        [self = shared_from_this()](const boost::system::error_code& error, std::size_t bytes_transferred) {
            self->handle_read_length(error, bytes_transferred);
        }
    );
}

void tcp_connection::handle_read_length(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        handle_error(error);
        return;
    }
    
    if (bytes_transferred != 4) {
        // LOG_ERROR("Invalid length read for connection {}: {} bytes", connection_id_, bytes_transferred);
        handle_error(boost::asio::error::invalid_argument);
        return;
    }
    
    // Extract message length (network byte order)
    std::uint32_t network_length;
    std::memcpy(&network_length, length_buffer_.data(), 4);
    expected_length_ = ntohl(network_length);
    
    // Validate message length
    if (expected_length_ == 0 || expected_length_ > DEFAULT_MAX_TCP_MESSAGE_SIZE) {
        // LOG_ERROR("Invalid message length for connection {}: {} bytes", connection_id_, expected_length_);
        handle_error(boost::asio::error::message_size);
        return;
    }
    
    // Prepare message buffer
    message_buffer_.resize(expected_length_);
    bytes_received_ = 0;
    
    // Start reading message data
    start_read_data();
}

void tcp_connection::start_read_data() {
    if (!active_.load()) {
        return;
    }
    
    auto remaining = expected_length_ - bytes_received_;
    auto read_size = std::min(remaining, static_cast<std::uint32_t>(read_buffer_.size()));
    
    socket_.async_read_some(
        boost::asio::buffer(read_buffer_.data(), read_size),
        [self = shared_from_this()](const boost::system::error_code& error, std::size_t bytes_transferred) {
            self->handle_read_data(error, bytes_transferred);
        }
    );
}

void tcp_connection::handle_read_data(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (error) {
        handle_error(error);
        return;
    }
    
    // Copy received data to message buffer
    std::memcpy(message_buffer_.data() + bytes_received_, read_buffer_.data(), bytes_transferred);
    bytes_received_ += static_cast<std::uint32_t>(bytes_transferred);
    
    // Check if message is complete
    if (bytes_received_ >= expected_length_) {
        handle_complete_message();
        
        // Start reading next message length
        start_read_length();
    } else {
        // Continue reading remaining data
        start_read_data();
    }
}

void tcp_connection::handle_complete_message() {
    try {
        // Check for keep-alive message
        if (expected_length_ == 4) {
            std::uint32_t message_type;
            std::memcpy(&message_type, message_buffer_.data(), 4);
            if (ntohl(message_type) == KEEP_ALIVE_MESSAGE_TYPE) {
        // LOG_TRACE("Received keep-alive from connection {}", connection_id_);
                return;
            }
        }
        
        // Create incoming message
        incoming_message msg(message_buffer_, remote_endpoint_);
        
        // LOG_TRACE("Received {} bytes from connection {}", message_buffer_.size(), connection_id_);
        
        // Forward to transport
        transport_.handle_incoming_message(std::move(msg), connection_id_);
        
    } catch (const std::exception& e) {
        // LOG_ERROR("Error handling complete message for connection {}: {}", connection_id_, e.what());
    }
}

void tcp_connection::handle_error(const boost::system::error_code& error) {
    if (error == boost::asio::error::eof || error == boost::asio::error::connection_reset) {
        // LOG_INFO("TCP connection {} closed by peer", connection_id_);
    } else if (error != boost::asio::error::operation_aborted) {
        // LOG_ERROR("TCP connection {} error: {}", connection_id_, error.message());
    }
    
    // Stop connection
    boost::asio::co_spawn(socket_.get_executor(),
        [self = shared_from_this()]() -> ss::core::async_void {
            co_await self->stop();
        },
        boost::asio::detached
    );
}

// tcp_transport implementation

tcp_transport::tcp_transport(ss::core::io_context& io_context, const transport_config& config)
    : io_context_(io_context)
    , config_(config)
    , running_(false)
    , bound_(false)
    , connected_(false)
    , shutdown_requested_(false)
    , server_mode_(false)
    , send_in_progress_(false) {
    
    // Initialize message buffers
    message_buffer::config buffer_config;
    buffer_config.buffer_capacity = config_.max_concurrent_ops;
    buffer_config.pool_config.block_size = std::max(config_.send_buffer_size, config_.receive_buffer_size);
    
    incoming_buffer_ = std::make_unique<message_buffer>(buffer_config);
    outgoing_buffer_ = std::make_unique<message_buffer>(buffer_config);
    
    // Initialize keep-alive timer
    keep_alive_timer_ = std::make_unique<ss::core::steady_timer>(io_context_);
    
        // LOG_INFO("TCP transport created with config: send_buffer={}, receive_buffer={}", 
        //          config_.send_buffer_size, config_.receive_buffer_size);
}

tcp_transport::~tcp_transport() {
    if (running_.load()) {
        try {
            // Synchronous shutdown for destructor
            boost::asio::co_spawn(io_context_, shutdown(), boost::asio::detached);
            io_context_.run_for(std::chrono::seconds(1));
        } catch (const std::exception& e) {
        // LOG_ERROR("Error during TCP transport destruction: {}", e.what());
        }
    }
    
        // LOG_DEBUG("TCP transport destroyed");
}

bool tcp_transport::is_healthy() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!running_.load() || shutdown_requested_.load()) {
        return false;
    }
    
    if (server_mode_.load()) {
        return acceptor_ && acceptor_->is_open();
    } else {
        return primary_connection_ && primary_connection_->is_active();
    }
}

std::string tcp_transport::status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json status_json;
    status_json["name"] = name();
    status_json["version"] = version();
    status_json["running"] = running_.load();
    status_json["bound"] = bound_.load();
    status_json["connected"] = connected_.load();
    status_json["server_mode"] = server_mode_.load();
    status_json["local_endpoint"] = local_endpoint_.to_string();
    status_json["remote_endpoint"] = remote_endpoint_.to_string();
    status_json["connection_count"] = connections_.size();
    status_json["send_queue_size"] = send_queue_.size();
    status_json["receive_queue_size"] = incoming_buffer_->size();
    
    auto stats = get_stats();
    status_json["stats"] = {
        {"bytes_sent", stats.bytes_sent},
        {"bytes_received", stats.bytes_received},
        {"messages_sent", stats.messages_sent},
        {"messages_received", stats.messages_received},
        {"send_errors", stats.send_errors},
        {"receive_errors", stats.receive_errors},
        {"active_connections", stats.active_connections}
    };
    
    return status_json.dump();
}

ss::core::async_void tcp_transport::initialize() {
        // LOG_DEBUG("Initializing TCP transport");
    
    try {
        // Create TCP acceptor for server mode
        acceptor_ = std::make_unique<acceptor_type>(io_context_);
        
        // LOG_INFO("TCP transport initialized successfully");
    } catch (const std::exception& e) {
        // LOG_ERROR("TCP transport initialization failed: {}", e.what());
    }
    co_return;
}

ss::core::async_void tcp_transport::cleanup() {
        // LOG_DEBUG("Cleaning up TCP transport");
    
    try {
        // Stop keep-alive timer
        if (keep_alive_timer_) {
            keep_alive_timer_->cancel();
        }
        
        // Close all connections
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& [id, connection] : connections_) {
                boost::asio::co_spawn(io_context_,
                    connection->stop(),
                    boost::asio::detached
                );
            }
            connections_.clear();
        }
        
        // Close primary connection
        if (primary_connection_) {
            co_await primary_connection_->stop();
            primary_connection_.reset();
        }
        
        // Close acceptor
        if (acceptor_ && acceptor_->is_open()) {
            acceptor_->close();
        }
        
        // Clear message buffers
        if (incoming_buffer_) {
            incoming_buffer_->clear();
        }
        if (outgoing_buffer_) {
            outgoing_buffer_->clear();
        }
        
        // Clear send queue
        {
            std::lock_guard<std::mutex> lock(mutex_);
            while (!send_queue_.empty()) {
                auto& [msg, handler] = send_queue_.front();
                if (handler) {
                    handler(ss::core::result<void, transport_error>::err(transport_error::shutdown_in_progress));
                }
                send_queue_.pop();
            }
        }
        
        // LOG_INFO("TCP transport cleanup completed");
    } catch (const std::exception& e) {
        // LOG_ERROR("TCP transport cleanup failed: {}", e.what());
    }
    co_return;
}

ss::core::async_void tcp_transport::start() {
    if (running_.exchange(true)) {
        // LOG_WARN("TCP transport already running");
        co_return;
    }
    
        // LOG_INFO("Starting TCP transport");
    
    try {
        // Start I/O worker threads
        start_io_threads();
        
        // Start keep-alive timer
        start_keep_alive_timer();
        
        // LOG_INFO("TCP transport started successfully");
    } catch (const std::exception& e) {
        // LOG_ERROR("Failed to start TCP transport: {}", e.what());
        running_.store(false);
    }
}

ss::core::async_void tcp_transport::stop() {
    if (!running_.exchange(false)) {
        // LOG_WARN("TCP transport not running");
        co_return;
    }
    
        // LOG_INFO("Stopping TCP transport");
    
    try {
        shutdown_requested_.store(true);
        
        // Cancel all pending operations
        if (acceptor_ && acceptor_->is_open()) {
            acceptor_->cancel();
        }
        
        // Stop I/O worker threads
        stop_io_threads();
        
        // Cleanup will handle connection cleanup
        co_await cleanup();
        
        bound_.store(false);
        connected_.store(false);
        server_mode_.store(false);
        
        // LOG_INFO("TCP transport stopped successfully");
    } catch (const std::exception& e) {
        // LOG_ERROR("Error stopping TCP transport: {}", e.what());
    }
}

bool tcp_transport::is_running() const noexcept {
    return running_.load() && !shutdown_requested_.load();
}

ss::core::async_result<ss::core::result<void, transport_error>>
tcp_transport::bind(const ss::core::endpoint& local_endpoint) {
    if (!acceptor_) {
        co_return ss::core::result<void, transport_error>::err(transport_error::not_connected);
    }
    
    if (bound_.load()) {
        co_return ss::core::result<void, transport_error>::err(transport_error::already_connected);
    }
    
    try {
        auto asio_endpoint = convert_endpoint(local_endpoint);
        
        // Open and bind acceptor
        acceptor_->open(asio_endpoint.protocol());
        acceptor_->set_option(boost::asio::socket_base::reuse_address(true));
        acceptor_->bind(asio_endpoint);
        acceptor_->listen();
        
        // Update local endpoint with actual bound address
        local_endpoint_ = convert_endpoint(acceptor_->local_endpoint());
        bound_.store(true);
        server_mode_.store(true);
        
        // Start accepting connections
        start_accept();
        
        // LOG_INFO("TCP transport bound to {} (server mode)", local_endpoint_.to_string());
        
        co_return ss::core::result<void, transport_error>::ok();
        
    } catch (const boost::system::system_error& e) {
        auto error = convert_error(e.code());
        // LOG_ERROR("Failed to bind TCP transport to {}: {}", 
        //          local_endpoint.to_string(), e.what());
        co_return ss::core::result<void, transport_error>::err(error);
    }
}

ss::core::async_result<ss::core::result<ss::core::endpoint, transport_error>>
tcp_transport::connect(const ss::core::endpoint& remote_endpoint) {
    if (server_mode_.load()) {
        co_return ss::core::result<ss::core::endpoint, transport_error>::err(transport_error::already_connected);
    }
    
    try {
        auto connection = co_await create_connection(remote_endpoint);
        if (!connection) {
            co_return ss::core::result<ss::core::endpoint, transport_error>::err(transport_error::connection_refused);
        }
        
        primary_connection_ = connection;
        remote_endpoint_ = remote_endpoint;
        connected_.store(true);
        
        // Add to connections map
        {
            std::lock_guard<std::mutex> lock(mutex_);
            connections_[connection->connection_id()] = connection;
        }
        
        // Start the connection
        connection->start();
        
        // LOG_INFO("TCP transport connected to {} (client mode)", remote_endpoint.to_string());
        
        co_return ss::core::result<ss::core::endpoint, transport_error>::ok(remote_endpoint);
        
    } catch (const std::exception& e) {
        // LOG_ERROR("Failed to connect TCP transport to {}: {}", 
        //          remote_endpoint.to_string(), e.what());
        co_return ss::core::result<ss::core::endpoint, transport_error>::err(transport_error::connection_refused);
    }
}

ss::core::async_result<ss::core::result<void, transport_error>>
tcp_transport::send(outgoing_message message) {
    if (shutdown_requested_.load()) {
        co_return ss::core::result<void, transport_error>::err(transport_error::shutdown_in_progress);
    }
    
    if (message.data.size() > max_message_size()) {
        co_return ss::core::result<void, transport_error>::err(transport_error::message_too_large);
    }
    
    try {
        tcp_connection_ptr connection;
        
        if (server_mode_.load()) {
            // Find existing connection to target
            connection = find_connection(message.target);
            if (!connection) {
                co_return ss::core::result<void, transport_error>::err(transport_error::not_connected);
            }
        } else {
            // Use primary connection
            connection = primary_connection_;
            if (!connection || !connection->is_active()) {
                co_return ss::core::result<void, transport_error>::err(transport_error::not_connected);
            }
        }
        
        // Send through connection
        auto result = co_await connection->send(message.data);
        
        if (result.is_ok()) {
            update_stats(message.data.size(), 0);
        // LOG_TRACE("Sent {} bytes to {}", message.data.size(), message.target.to_string());
        } else {
            update_stats(0, 0, true, false);
        }
        
        co_return result;
        
    } catch (const std::exception& e) {
        // LOG_ERROR("Failed to send message to {}: {}", message.target.to_string(), e.what());
        update_stats(0, 0, true, false);
        co_return ss::core::result<void, transport_error>::err(transport_error::network_failure);
    }
}

void tcp_transport::send_async(outgoing_message message, send_completion_handler handler) {
    if (shutdown_requested_.load()) {
        if (handler) {
            handler(ss::core::result<void, transport_error>::err(transport_error::shutdown_in_progress));
        }
        return;
    }
    
    if (message.data.size() > max_message_size()) {
        if (handler) {
            handler(ss::core::result<void, transport_error>::err(transport_error::message_too_large));
        }
        return;
    }
    
    // Add to send queue
    {
        std::lock_guard<std::mutex> lock(mutex_);
        send_queue_.emplace(std::move(message), std::move(handler));
    }
    
    // Process send queue if not already in progress
    if (!send_in_progress_.exchange(true)) {
        boost::asio::co_spawn(io_context_, 
            [this]() -> boost::asio::awaitable<void> {
                co_await process_send_queue();
                co_return;
            },
            boost::asio::detached
        );
    }
}

ss::core::async_result<ss::core::result<incoming_message, transport_error>>
tcp_transport::receive(std::optional<std::chrono::milliseconds> timeout) {
    if (!incoming_buffer_) {
        co_return ss::core::result<incoming_message, transport_error>::err(transport_error::not_connected);
    }
    
    try {
        if (timeout.has_value()) {
            auto result = incoming_buffer_->pop_timeout(timeout.value());
            if (result.is_ok()) {
                auto buffered_msg = std::move(result.value());
                incoming_message msg(
                    std::vector<std::uint8_t>(
                        buffered_msg.data.as<std::uint8_t>(),
                        buffered_msg.data.as<std::uint8_t>() + buffered_msg.size
                    ),
                    ss::core::endpoint()  // Will be set from buffered metadata
                );
                msg.received_at = buffered_msg.timestamp;
                co_return ss::core::result<incoming_message, transport_error>::ok(std::move(msg));
            } else {
                auto error = (result.error() == buffer_error::timeout) ? 
                           transport_error::timeout : transport_error::unknown;
                co_return ss::core::result<incoming_message, transport_error>::err(error);
            }
        } else {
            auto result = incoming_buffer_->pop();
            if (result.is_ok()) {
                auto buffered_msg = std::move(result.value());
                incoming_message msg(
                    std::vector<std::uint8_t>(
                        buffered_msg.data.as<std::uint8_t>(),
                        buffered_msg.data.as<std::uint8_t>() + buffered_msg.size
                    ),
                    ss::core::endpoint()  // Will be set from buffered metadata
                );
                msg.received_at = buffered_msg.timestamp;
                co_return ss::core::result<incoming_message, transport_error>::ok(std::move(msg));
            } else {
                co_return ss::core::result<incoming_message, transport_error>::err(transport_error::unknown);
            }
        }
    } catch (const std::exception& e) {
        // LOG_ERROR("Error receiving message: {}", e.what());
        co_return ss::core::result<incoming_message, transport_error>::err(transport_error::unknown);
    }
}

void tcp_transport::set_message_handler(message_handler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    message_handler_ = std::move(handler);
        // LOG_DEBUG("Message handler set for TCP transport");
}

void tcp_transport::clear_message_handler() {
    std::lock_guard<std::mutex> lock(mutex_);
    message_handler_ = nullptr;
        // LOG_DEBUG("Message handler cleared for TCP transport");
}

ss::core::endpoint tcp_transport::local_endpoint() const noexcept {
    return local_endpoint_;
}

ss::core::endpoint tcp_transport::remote_endpoint() const noexcept {
    return remote_endpoint_;
}

bool tcp_transport::is_connected() const noexcept {
    if (server_mode_.load()) {
        std::lock_guard<std::mutex> lock(mutex_);
        return bound_.load() && !connections_.empty();
    } else {
        return connected_.load() && primary_connection_ && primary_connection_->is_active();
    }
}

transport_stats tcp_transport::get_stats() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto stats = stats_;
    // stats.current_size = incoming_buffer_ ? incoming_buffer_->size() : 0; // TODO: Add current_size to transport_stats
    stats.max_message_size = max_message_size();
    stats.active_connections = static_cast<std::uint32_t>(connections_.size());
    return stats;
}

void tcp_transport::reset_stats() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = transport_stats{};
    if (incoming_buffer_) {
        incoming_buffer_->reset_stats();
    }
    if (outgoing_buffer_) {
        outgoing_buffer_->reset_stats();
    }
}

transport_config tcp_transport::get_config() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

ss::core::result<void, transport_error> tcp_transport::set_config(const transport_config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Validate configuration
    if (config.send_buffer_size == 0 || config.receive_buffer_size == 0) {
        return ss::core::result<void, transport_error>::err(transport_error::invalid_argument);
    }
    
    config_ = config;
    
        // LOG_INFO("TCP transport configuration updated");
    return ss::core::result<void, transport_error>::ok();
}

std::uint32_t tcp_transport::max_message_size() const noexcept {
    return config_.max_message_size > 0 ? config_.max_message_size : DEFAULT_MAX_TCP_MESSAGE_SIZE;
}

bool tcp_transport::can_send() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return send_queue_.size() < config_.max_concurrent_ops;
}

std::uint32_t tcp_transport::send_queue_size() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<std::uint32_t>(send_queue_.size());
}

std::uint32_t tcp_transport::receive_queue_size() const noexcept {
    return incoming_buffer_ ? static_cast<std::uint32_t>(incoming_buffer_->size()) : 0;
}

ss::core::async_void tcp_transport::disconnect() {
    if (server_mode_.load()) {
        // Close acceptor
        if (acceptor_ && acceptor_->is_open()) {
            acceptor_->close();
        }
        bound_.store(false);
    } else {
        // Close primary connection
        if (primary_connection_) {
            co_await primary_connection_->stop();
            primary_connection_.reset();
        }
        connected_.store(false);
    }
    
        // LOG_INFO("TCP transport disconnected");
}

ss::core::async_void tcp_transport::shutdown(std::chrono::milliseconds timeout) {
        // LOG_INFO("Shutting down TCP transport with timeout {}ms", timeout.count());
    
    shutdown_requested_.store(true);
    
    try {
        // Cancel all pending operations
        if (acceptor_ && acceptor_->is_open()) {
            acceptor_->cancel();
        }
        
        // Wait for operations to complete or timeout
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (send_in_progress_.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // Force stop if still running
        if (running_.load()) {
            co_await stop();
        }
        
        // LOG_INFO("TCP transport shutdown completed");
        
    } catch (const std::exception& e) {
        // LOG_ERROR("Error during TCP transport shutdown: {}", e.what());
    }
}

void tcp_transport::handle_incoming_message(incoming_message message, std::uint64_t connection_id) {
    (void)connection_id; // TODO: Use for connection tracking
    try {
        update_stats(0, message.data.size());
        
        // Handle message
        if (message_handler_) {
            boost::asio::co_spawn(io_context_,
                message_handler_(std::move(message)),
                boost::asio::detached
            );
        } else if (incoming_buffer_) {
            // Store in buffer
            auto block = incoming_buffer_->pool().allocate();
            if (block) {
                std::memcpy(block, message.data.data(), message.data.size());
                memory_block mem_block(block, message.data.size(), 
                    [&pool = incoming_buffer_->pool()](void* ptr) {
                        pool.deallocate(ptr);
                    });
                
                incoming_buffer_->push_zero_copy(std::move(mem_block), message.data.size());
            }
        }
        
        // LOG_TRACE("Handled incoming message of {} bytes from connection {}", 
        //          message.data.size(), connection_id);
        
    } catch (const std::exception& e) {
        // LOG_ERROR("Error handling incoming message from connection {}: {}", connection_id, e.what());
        update_stats(0, 0, false, true);
    }
}

void tcp_transport::handle_connection_closed(std::uint64_t connection_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = connections_.find(connection_id);
    if (it != connections_.end()) {
        // LOG_INFO("TCP connection {} closed", connection_id);
        connections_.erase(it);
        
        // If this was the primary connection, mark as disconnected
        if (primary_connection_ && primary_connection_->connection_id() == connection_id) {
            primary_connection_.reset();
            connected_.store(false);
        }
    }
}

// Private method implementations

void tcp_transport::start_accept() {
    if (!acceptor_ || !acceptor_->is_open() || !is_running()) {
        return;
    }
    
    acceptor_->async_accept(
        [this](const boost::system::error_code& error, socket_type socket) {
            handle_accept(error, std::move(socket));
        }
    );
}

void tcp_transport::handle_accept(const boost::system::error_code& error, socket_type socket) {
    if (error) {
        if (error != boost::asio::error::operation_aborted) {
        // LOG_ERROR("TCP accept error: {}", error.message());
        }
        return;
    }
    
    try {
        // Apply socket options
        auto result = apply_socket_options(socket);
        if (!result.is_ok()) {
        // LOG_ERROR("Failed to apply socket options to new connection");
            socket.close();
        } else {
            // Create new connection
            auto connection = std::make_shared<tcp_connection>(std::move(socket), *this);
            
            {
                std::lock_guard<std::mutex> lock(mutex_);
                connections_[connection->connection_id()] = connection;
            }
            
            // Start the connection
            connection->start();
            
        // LOG_INFO("Accepted new TCP connection {} from {}", 
        //          connection->connection_id(), connection->remote_endpoint().to_string());
        }
    } catch (const std::exception& e) {
        // LOG_ERROR("Error handling accepted connection: {}", e.what());
    }
    
    // Continue accepting connections
    start_accept();
}

boost::asio::awaitable<void> tcp_transport::process_send_queue() {
    while (is_running()) {
        std::optional<std::pair<outgoing_message, send_completion_handler>> item;
        
        // Get next item from queue
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (send_queue_.empty()) {
                send_in_progress_.store(false);
                co_return;
            }
            item = std::move(send_queue_.front());
            send_queue_.pop();
        }
        
        if (!item) {
            continue;
        }
        
        auto& [message, handler] = *item;
        
        try {
            tcp_connection_ptr connection;
            
            if (server_mode_.load()) {
                connection = find_connection(message.target);
            } else {
                connection = primary_connection_;
            }
            
            if (!connection || !connection->is_active()) {
                if (handler) {
                    handler(ss::core::result<void, transport_error>::err(transport_error::not_connected));
                }
                continue;
            }
            
            // Send through connection
            auto result = co_await connection->send(message.data);
            
            if (result.is_ok()) {
                update_stats(message.data.size(), 0);
            } else {
                update_stats(0, 0, true, false);
            }
            
            if (handler) {
                handler(result);
            }
            
        } catch (const std::exception& e) {
        // LOG_ERROR("Error processing send queue item: {}", e.what());
            if (handler) {
                handler(ss::core::result<void, transport_error>::err(transport_error::network_failure));
            }
        }
    }
    
    send_in_progress_.store(false);
    co_return;
}

tcp_connection_ptr tcp_transport::find_connection(const ss::core::endpoint& target) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [id, connection] : connections_) {
        if (connection->is_active() && connection->remote_endpoint() == target) {
            return connection;
        }
    }
    
    return nullptr;
}

ss::core::async_result<tcp_connection_ptr> tcp_transport::create_connection(const ss::core::endpoint& target) {
    try {
        socket_type socket(io_context_);
        auto asio_endpoint = convert_endpoint(target);
        
        // Connect to target
        co_await socket.async_connect(asio_endpoint, boost::asio::use_awaitable);
        
        // Apply socket options
        auto result = apply_socket_options(socket);
        if (!result.is_ok()) {
            socket.close();
            co_return nullptr;
        }
        
        // Create connection
        auto connection = std::make_shared<tcp_connection>(std::move(socket), *this);
        
        // LOG_INFO("Created TCP connection {} to {}", 
        //          connection->connection_id(), target.to_string());
        
        co_return connection;
        
    } catch (const std::exception& e) {
        // LOG_ERROR("Failed to create connection to {}: {}", target.to_string(), e.what());
        co_return nullptr;
    }
}

transport_error tcp_transport::convert_error(const boost::system::error_code& error) const noexcept {
    if (!error) {
        return transport_error::none;
    }
    
    switch (error.value()) {
        case boost::asio::error::connection_refused:
            return transport_error::connection_refused;
        case boost::asio::error::timed_out:
            return transport_error::timeout;
        case boost::asio::error::host_unreachable:
            return transport_error::host_unreachable;
        case boost::asio::error::network_unreachable:
            return transport_error::no_route_to_host;
        case boost::asio::error::address_in_use:
            return transport_error::address_in_use;
        case boost::system::errc::address_not_available:
            return transport_error::address_not_available;
        case boost::asio::error::connection_reset:
            return transport_error::connection_reset;
        case boost::asio::error::connection_aborted:
            return transport_error::connection_aborted;
        case boost::asio::error::message_size:
            return transport_error::message_too_large;
        case boost::asio::error::invalid_argument:
            return transport_error::invalid_argument;
        case boost::asio::error::access_denied:
            return transport_error::permission_denied;
        case boost::asio::error::no_memory:
            return transport_error::out_of_memory;
        case boost::asio::error::operation_aborted:
            return transport_error::shutdown_in_progress;
        default:
            return transport_error::network_failure;
    }
}

ss::core::endpoint tcp_transport::convert_endpoint(const endpoint_type& ep) const noexcept {
    try {
        return ss::core::endpoint(ep.address().to_string(), ep.port());
    } catch (...) {
        return ss::core::endpoint{};
    }
}

tcp_transport::endpoint_type tcp_transport::convert_endpoint(const ss::core::endpoint& ep) const noexcept {
    try {
        return endpoint_type(boost::asio::ip::make_address(ep.address()), ep.port());
    } catch (...) {
        return endpoint_type{};
    }
}

void tcp_transport::update_stats(std::size_t bytes_sent, std::size_t bytes_received,
                                bool send_error, bool receive_error) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (bytes_sent > 0) {
        stats_.bytes_sent += bytes_sent;
        stats_.messages_sent++;
        stats_.last_send_time = std::chrono::steady_clock::now();
    }
    
    if (bytes_received > 0) {
        stats_.bytes_received += bytes_received;
        stats_.messages_received++;
        stats_.last_receive_time = std::chrono::steady_clock::now();
    }
    
    if (send_error) {
        stats_.send_errors++;
    }
    
    if (receive_error) {
        stats_.receive_errors++;
    }
}

void tcp_transport::start_io_threads() {
    auto thread_count = std::max(1u, config_.io_threads);
    io_threads_.reserve(thread_count);
    
    for (std::uint32_t i = 0; i < thread_count; ++i) {
        io_threads_.emplace_back([this]() { io_worker(); });
    }
    
        // LOG_INFO("Started {} I/O worker threads", thread_count);
}

void tcp_transport::stop_io_threads() {
    for (auto& thread : io_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    io_threads_.clear();
    
        // LOG_INFO("Stopped I/O worker threads");
}

void tcp_transport::io_worker() {
        // LOG_DEBUG("TCP I/O worker thread started");
    
    try {
        while (is_running()) {
            try {
                io_context_.run_for(std::chrono::milliseconds(100));
            } catch (const std::exception& e) {
                if (is_running()) {
        // LOG_ERROR("TCP I/O worker error: {}", e.what());
                }
            }
        }
    } catch (const std::exception& e) {
        // LOG_ERROR("TCP I/O worker thread error: {}", e.what());
    }
    
        // LOG_DEBUG("TCP I/O worker thread stopped");
}

ss::core::result<void, transport_error> tcp_transport::apply_socket_options(socket_type& socket) {
    try {
        // Set socket buffer sizes
        socket.set_option(boost::asio::socket_base::send_buffer_size(config_.send_buffer_size));
        socket.set_option(boost::asio::socket_base::receive_buffer_size(config_.receive_buffer_size));
        
        // Enable TCP keepalive
        socket.set_option(boost::asio::socket_base::keep_alive(true));
        
        // Disable Nagle's algorithm for low latency
        socket.set_option(boost::asio::ip::tcp::no_delay(true));
        
        // LOG_DEBUG("Applied TCP socket options: send_buffer={}, receive_buffer={}", 
        //          config_.send_buffer_size, config_.receive_buffer_size);
        
        return ss::core::result<void, transport_error>::ok();
        
    } catch (const boost::system::system_error& e) {
        auto error = convert_error(e.code());
        // LOG_ERROR("Failed to apply TCP socket options: {}", e.what());
        return ss::core::result<void, transport_error>::err(error);
    }
}

void tcp_transport::start_keep_alive_timer() {
    if (!keep_alive_timer_ || config_.keep_alive_interval == std::chrono::seconds{0}) {
        return;
    }
    
    keep_alive_timer_->expires_after(config_.keep_alive_interval);
    keep_alive_timer_->async_wait(
        [this](const boost::system::error_code& error) {
            if (!error && is_running()) {
                handle_keep_alive_timeout();
                start_keep_alive_timer();
            }
        }
    );
}

void tcp_transport::handle_keep_alive_timeout() {
    try {
        // Send keep-alive messages to all connections
        send_keep_alive_messages();
        
        // Cleanup inactive connections
        cleanup_inactive_connections();
        
    } catch (const std::exception& e) {
        // LOG_ERROR("Error handling keep-alive timeout: {}", e.what());
    }
}

void tcp_transport::send_keep_alive_messages() {
    std::vector<std::uint8_t> keep_alive_data(4);
    std::uint32_t message_type = htonl(KEEP_ALIVE_MESSAGE_TYPE);
    std::memcpy(keep_alive_data.data(), &message_type, 4);
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [id, connection] : connections_) {
        if (connection->is_active()) {
            boost::asio::co_spawn(io_context_,
                connection->send(keep_alive_data),
                boost::asio::detached
            );
        }
    }
    
        // LOG_TRACE("Sent keep-alive messages to {} connections", connections_.size());
}

void tcp_transport::cleanup_inactive_connections() {
    std::vector<std::uint64_t> inactive_connections;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& [id, connection] : connections_) {
            if (!connection->is_active()) {
                inactive_connections.push_back(id);
            }
        }
        
        for (auto id : inactive_connections) {
        // LOG_DEBUG("Cleaning up inactive TCP connection {}", id);
            connections_.erase(id);
        }
    }
    
    if (!inactive_connections.empty()) {
        // LOG_INFO("Cleaned up {} inactive TCP connections", inactive_connections.size());
    }
}

std::vector<std::uint8_t> tcp_transport::frame_message(const std::vector<std::uint8_t>& data) const {
    std::vector<std::uint8_t> framed_data(MESSAGE_FRAME_OVERHEAD + data.size());
    
    // Add length prefix (network byte order)
    std::uint32_t network_length = htonl(static_cast<std::uint32_t>(data.size()));
    std::memcpy(framed_data.data(), &network_length, MESSAGE_FRAME_OVERHEAD);
    
    // Add message data
    std::memcpy(framed_data.data() + MESSAGE_FRAME_OVERHEAD, data.data(), data.size());
    
    return framed_data;
}

} // namespace ss::network::impl