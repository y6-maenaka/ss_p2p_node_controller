#include "../../../../include/ss_p2p/network/impl/udp_transport.hpp"
#include "../../../../include/logger/logger.hpp"
#include "../../../../include/json.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <algorithm>
#include <random>

namespace ss::network::impl {

namespace {
    /// Default UDP packet size limit (Ethernet MTU - headers)
    constexpr std::size_t DEFAULT_MAX_UDP_SIZE = 1472;
    
    /// Fragment timeout in seconds
    constexpr auto FRAGMENT_TIMEOUT = std::chrono::seconds{30};
    
    /// Fragment cleanup interval
    constexpr auto FRAGMENT_CLEANUP_INTERVAL = std::chrono::seconds{10};

    /// Maximum fragment ID
    constexpr std::uint32_t MAX_FRAGMENT_ID = 0xFFFFFFFF;

    /**
     * @brief Generate random fragment ID
     * @return Random fragment ID
     */
    std::uint32_t generate_fragment_id() {
        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());
        static thread_local std::uniform_int_distribution<std::uint32_t> dis(1, MAX_FRAGMENT_ID);
        return dis(gen);
    }
}

udp_transport::udp_transport(ss::core::io_context& io_context, const transport_config& config)
    : io_context_(io_context)
    , config_(config)
    , running_(false)
    , bound_(false)
    , connected_(false)
    , shutdown_requested_(false)
    , send_in_progress_(false)
    , receive_in_progress_(false) {
    
    // Initialize message buffers
    message_buffer::config buffer_config;
    buffer_config.buffer_capacity = config_.max_concurrent_ops;
    buffer_config.pool_config.block_size = std::max(config_.send_buffer_size, config_.receive_buffer_size);
    
    incoming_buffer_ = std::make_unique<message_buffer>(buffer_config);
    outgoing_buffer_ = std::make_unique<message_buffer>(buffer_config);
    
    // Initialize fragment cleanup timer
    fragment_cleanup_timer_ = std::make_unique<ss::core::steady_timer>(io_context_);
    
    // LOG_INFO("UDP transport created with config: send_buffer={}, receive_buffer={}", 
    //          config_.send_buffer_size, config_.receive_buffer_size);
}

udp_transport::~udp_transport() {
    if (running_.load()) {
        try {
            // Synchronous shutdown for destructor
            boost::asio::co_spawn(io_context_, shutdown(), boost::asio::detached);
            io_context_.run_for(std::chrono::seconds(1));
        } catch (const std::exception& e) {
            // LOG_ERROR("Error during UDP transport destruction: {}", e.what());
        }
    }
    
    // LOG_DEBUG("UDP transport destroyed");
}

bool udp_transport::is_healthy() const noexcept {
    return running_.load() && socket_ && socket_->is_open() && !shutdown_requested_.load();
}

std::string udp_transport::status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json status_json;
    status_json["name"] = name();
    status_json["version"] = version();
    status_json["running"] = running_.load();
    status_json["bound"] = bound_.load();
    status_json["connected"] = connected_.load();
    status_json["local_endpoint"] = local_endpoint_.to_string();
    status_json["remote_endpoint"] = remote_endpoint_.to_string();
    status_json["send_queue_size"] = send_queue_.size();
    status_json["receive_queue_size"] = incoming_buffer_->size();
    status_json["fragment_count"] = fragment_map_.size();
    
    auto stats = get_stats();
    status_json["stats"] = {
        {"bytes_sent", stats.bytes_sent},
        {"bytes_received", stats.bytes_received},
        {"messages_sent", stats.messages_sent},
        {"messages_received", stats.messages_received},
        {"send_errors", stats.send_errors},
        {"receive_errors", stats.receive_errors}
    };
    
    return status_json.dump();
}

ss::core::async_void udp_transport::initialize() {
    // LOG_DEBUG("Initializing UDP transport");
    
    try {
        // Create UDP socket
        socket_ = std::make_unique<socket_type>(io_context_);
        
        // Apply socket options
        auto result = apply_socket_options();
        if (!result.is_ok()) {
            // LOG_ERROR("Failed to apply socket options: {}", to_string(result.error()));
            co_return;
        }
        
        // LOG_INFO("UDP transport initialized successfully");
    } catch (const std::exception& e) {
        // LOG_ERROR("UDP transport initialization failed: {}", e.what());
    }
}

ss::core::async_void udp_transport::cleanup() {
    // LOG_DEBUG("Cleaning up UDP transport");
    
    try {
        // Stop fragment cleanup timer
        if (fragment_cleanup_timer_) {
            fragment_cleanup_timer_->cancel();
        }
        
        // Clear message buffers
        if (incoming_buffer_) {
            incoming_buffer_->clear();
        }
        if (outgoing_buffer_) {
            outgoing_buffer_->clear();
        }
        
        // Clear fragment map
        {
            std::lock_guard<std::mutex> lock(mutex_);
            fragment_map_.clear();
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
        
        // LOG_INFO("UDP transport cleanup completed");
    } catch (const std::exception& e) {
        // LOG_ERROR("UDP transport cleanup failed: {}", e.what());
    }
    co_return;
}

ss::core::async_void udp_transport::start() {
    if (running_.exchange(true)) {
        // LOG_WARN("UDP transport already running");
        co_return;
    }
    
    // LOG_INFO("Starting UDP transport");
    
    try {
        // Start I/O worker threads
        start_io_threads();
        
        // Start fragment cleanup timer
        start_fragment_cleanup_timer();
        
        // LOG_INFO("UDP transport started successfully");
    } catch (const std::exception& e) {
        // LOG_ERROR("Failed to start UDP transport: {}", e.what());
        running_.store(false);
    }
}

ss::core::async_void udp_transport::stop() {
    if (!running_.exchange(false)) {
        // LOG_WARN("UDP transport not running");
        co_return;
    }
    
    // LOG_INFO("Stopping UDP transport");
    
    try {
        shutdown_requested_.store(true);
        
        // Cancel all pending operations
        if (socket_ && socket_->is_open()) {
            socket_->cancel();
        }
        
        // Stop I/O worker threads
        stop_io_threads();
        
        // Close socket
        if (socket_ && socket_->is_open()) {
            socket_->close();
        }
        
        bound_.store(false);
        connected_.store(false);
        
        // LOG_INFO("UDP transport stopped successfully");
    } catch (const std::exception& e) {
        // LOG_ERROR("Error stopping UDP transport: {}", e.what());
    }
}

bool udp_transport::is_running() const noexcept {
    return running_.load() && !shutdown_requested_.load();
}

ss::core::async_result<ss::core::result<void, transport_error>>
udp_transport::bind(const ss::core::endpoint& local_endpoint) {
    if (!socket_ || !socket_->is_open()) {
        co_return ss::core::result<void, transport_error>::err(transport_error::not_connected);
    }
    
    if (bound_.load()) {
        co_return ss::core::result<void, transport_error>::err(transport_error::already_connected);
    }
    
    try {
        auto asio_endpoint = convert_endpoint(local_endpoint);
        
        // Bind socket to local endpoint
        socket_->bind(asio_endpoint);
        
        // Update local endpoint with actual bound address
        local_endpoint_ = convert_endpoint(socket_->local_endpoint());
        bound_.store(true);
        
        // Start receive operations
        start_receive();
        
        // LOG_INFO("UDP transport bound to {}", local_endpoint_.to_string());
        
        co_return ss::core::result<void, transport_error>::ok();
        
    } catch (const boost::system::system_error& e) {
        auto error = convert_error(e.code());
        // LOG_ERROR("Failed to bind UDP transport to {}: {}", 
        //          local_endpoint.to_string(), e.what());
        co_return ss::core::result<void, transport_error>::err(error);
    }
}

ss::core::async_result<ss::core::result<ss::core::endpoint, transport_error>>
udp_transport::connect(const ss::core::endpoint& remote_endpoint) {
    // UDP is connectionless, but we can set a default remote endpoint
    remote_endpoint_ = remote_endpoint;
    connected_.store(true);
    
    // LOG_INFO("UDP transport pseudo-connected to {}", remote_endpoint.to_string());
    
    co_return ss::core::result<ss::core::endpoint, transport_error>::ok(remote_endpoint);
}

ss::core::async_result<ss::core::result<void, transport_error>>
udp_transport::send(outgoing_message message) {
    if (!socket_ || !socket_->is_open()) {
        co_return ss::core::result<void, transport_error>::err(transport_error::not_connected);
    }
    
    if (shutdown_requested_.load()) {
        co_return ss::core::result<void, transport_error>::err(transport_error::shutdown_in_progress);
    }
    
    if (!is_valid_message_size(message.data.size())) {
        co_return ss::core::result<void, transport_error>::err(transport_error::message_too_large);
    }
    
    try {
        // auto start_time = std::chrono::steady_clock::now(); // TODO: Use for latency calculation
        auto asio_endpoint = convert_endpoint(message.target);
        
        // Check if message needs fragmentation
        if (message.data.size() > DEFAULT_MAX_UDP_SIZE) {
            auto fragments = fragment_message(message);
            
            for (auto& fragment : fragments) {
                auto asio_frag_endpoint = convert_endpoint(fragment.target);
                co_await socket_->async_send_to(
                    boost::asio::buffer(fragment.data),
                    asio_frag_endpoint,
                    boost::asio::use_awaitable
                );
            }
        } else {
            // Send message directly
            co_await socket_->async_send_to(
                boost::asio::buffer(message.data),
                asio_endpoint,
                boost::asio::use_awaitable
            );
        }
        
        // Update statistics
        // auto latency = calculate_latency(start_time); // TODO: Use for stats
        update_stats(message.data.size(), 0);
        
        // LOG_TRACE("Sent {} bytes to {}", message.data.size(), message.target.to_string());
        
        co_return ss::core::result<void, transport_error>::ok();
        
    } catch (const boost::system::system_error& e) {
        auto error = convert_error(e.code());
        update_stats(0, 0, true, false);
        // LOG_ERROR("Failed to send message to {}: {}", 
        //          message.target.to_string(), e.what());
        co_return ss::core::result<void, transport_error>::err(error);
    }
}

void udp_transport::send_async(outgoing_message message, send_completion_handler handler) {
    if (!socket_ || !socket_->is_open()) {
        if (handler) {
            handler(ss::core::result<void, transport_error>::err(transport_error::not_connected));
        }
        return;
    }
    
    if (shutdown_requested_.load()) {
        if (handler) {
            handler(ss::core::result<void, transport_error>::err(transport_error::shutdown_in_progress));
        }
        return;
    }
    
    if (!is_valid_message_size(message.data.size())) {
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
udp_transport::receive(std::optional<std::chrono::milliseconds> timeout) {
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

void udp_transport::set_message_handler(message_handler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    message_handler_ = std::move(handler);
    // LOG_DEBUG("Message handler set for UDP transport");
}

void udp_transport::clear_message_handler() {
    std::lock_guard<std::mutex> lock(mutex_);
    message_handler_ = nullptr;
    // LOG_DEBUG("Message handler cleared for UDP transport");
}

ss::core::endpoint udp_transport::local_endpoint() const noexcept {
    return local_endpoint_;
}

ss::core::endpoint udp_transport::remote_endpoint() const noexcept {
    return remote_endpoint_;
}

bool udp_transport::is_connected() const noexcept {
    return bound_.load() && socket_ && socket_->is_open();
}

transport_stats udp_transport::get_stats() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto stats = stats_;
    // stats.current_size = incoming_buffer_ ? incoming_buffer_->size() : 0; // TODO: Add current_size to transport_stats
    stats.max_message_size = max_message_size();
    return stats;
}

void udp_transport::reset_stats() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = transport_stats{};
    if (incoming_buffer_) {
        incoming_buffer_->reset_stats();
    }
    if (outgoing_buffer_) {
        outgoing_buffer_->reset_stats();
    }
}

transport_config udp_transport::get_config() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

ss::core::result<void, transport_error> udp_transport::set_config(const transport_config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Validate configuration
    if (config.send_buffer_size == 0 || config.receive_buffer_size == 0) {
        return ss::core::result<void, transport_error>::err(transport_error::invalid_argument);
    }
    
    config_ = config;
    
    // Apply new socket options if socket is open
    if (socket_ && socket_->is_open()) {
        auto result = apply_socket_options();
        if (!result.is_ok()) {
            return result;
        }
    }
    
    // LOG_INFO("UDP transport configuration updated");
    return ss::core::result<void, transport_error>::ok();
}

std::uint32_t udp_transport::max_message_size() const noexcept {
    return config_.max_message_size > 0 ? config_.max_message_size : DEFAULT_MAX_UDP_SIZE;
}

bool udp_transport::can_send() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return send_queue_.size() < config_.max_concurrent_ops;
}

std::uint32_t udp_transport::send_queue_size() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<std::uint32_t>(send_queue_.size());
}

std::uint32_t udp_transport::receive_queue_size() const noexcept {
    return incoming_buffer_ ? static_cast<std::uint32_t>(incoming_buffer_->size()) : 0;
}

ss::core::async_void udp_transport::disconnect() {
    // UDP is connectionless, just clear the pseudo-connection
    connected_.store(false);
    remote_endpoint_ = ss::core::endpoint{};
    // LOG_INFO("UDP transport disconnected");
    co_return;
}

ss::core::async_void udp_transport::shutdown(std::chrono::milliseconds timeout) {
    // LOG_INFO("Shutting down UDP transport with timeout {}ms", timeout.count());
    
    shutdown_requested_.store(true);
    
    try {
        // Cancel all pending operations
        if (socket_ && socket_->is_open()) {
            socket_->cancel();
        }
        
        // Wait for operations to complete or timeout
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while ((send_in_progress_.load() || receive_in_progress_.load()) &&
               std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // Force stop if still running
        if (running_.load()) {
            co_await stop();
        }
        
        // LOG_INFO("UDP transport shutdown completed");
        
    } catch (const std::exception& e) {
        // LOG_ERROR("Error during UDP transport shutdown: {}", e.what());
    }
}

// Private method implementations

void udp_transport::start_receive() {
    if (receive_in_progress_.exchange(true)) {
        return;
    }
    
    boost::asio::co_spawn(io_context_,
        [this]() -> ss::core::async_void {
            while (is_running() && socket_ && socket_->is_open()) {
                try {
                    auto bytes_received = co_await socket_->async_receive_from(
                        boost::asio::buffer(receive_buffer_),
                        sender_endpoint_,
                        boost::asio::use_awaitable
                    );
                    
                    // Handle received data
                    handle_receive(boost::system::error_code{}, bytes_received);
                    
                } catch (const boost::system::system_error& e) {
                    if (e.code() != boost::asio::error::operation_aborted) {
                        handle_receive(e.code(), 0);
                    }
                    break;
                }
            }
            receive_in_progress_.store(false);
        },
        boost::asio::detached
    );
}

void udp_transport::handle_receive(const boost::system::error_code& error, std::size_t bytes_received) {
    if (error) {
        if (error != boost::asio::error::operation_aborted) {
            // LOG_ERROR("UDP receive error: {}", error.message());
            update_stats(0, 0, false, true);
        }
        return;
    }
    
    if (bytes_received == 0) {
        return;
    }
    
    try {
        // Create incoming message
        std::vector<std::uint8_t> data(receive_buffer_.begin(), 
                                      receive_buffer_.begin() + bytes_received);
        
        incoming_message msg(std::move(data), convert_endpoint(sender_endpoint_));
        
        // Check if this is a fragment
        auto complete_msg = defragment_message(msg);
        if (complete_msg.has_value()) {
            msg = std::move(complete_msg.value());
        } else {
            // Fragment not yet complete, continue receiving
            update_stats(0, bytes_received);
            return;
        }
        
        // Update statistics
        update_stats(0, bytes_received);
        
        // Handle message
        if (message_handler_) {
            boost::asio::co_spawn(io_context_,
                message_handler_(std::move(msg)),
                boost::asio::detached
            );
        } else if (incoming_buffer_) {
            // Store in buffer
            auto block = incoming_buffer_->pool().allocate();
            if (block) {
                std::memcpy(block, msg.data.data(), msg.data.size());
                memory_block mem_block(block, msg.data.size(), 
                    [&pool = incoming_buffer_->pool()](void* ptr) {
                        pool.deallocate(ptr);
                    });
                
                incoming_buffer_->push_zero_copy(std::move(mem_block), msg.data.size());
            }
        }
        
        // LOG_TRACE("Received {} bytes from {}", bytes_received, msg.sender.to_string());
        
    } catch (const std::exception& e) {
        // LOG_ERROR("Error handling received data: {}", e.what());
        update_stats(0, 0, false, true);
    }
}

boost::asio::awaitable<void> udp_transport::process_send_queue() {
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
            // auto start_time = std::chrono::steady_clock::now(); // TODO: Use for latency calculation
            auto asio_endpoint = convert_endpoint(message.target);
            
            // Send message
            auto bytes_sent = co_await socket_->async_send_to(
                boost::asio::buffer(message.data),
                asio_endpoint,
                boost::asio::use_awaitable
            );
            
            // Handle completion
            handle_send(boost::system::error_code{}, bytes_sent, std::move(handler));
            
        } catch (const boost::system::system_error& e) {
            handle_send(e.code(), 0, std::move(handler));
        }
    }
    
    send_in_progress_.store(false);
}

void udp_transport::handle_send(const boost::system::error_code& error, 
                               std::size_t bytes_sent,
                               send_completion_handler handler) {
    if (error) {
        auto transport_err = convert_error(error);
        update_stats(0, 0, true, false);
        
        if (handler) {
            handler(ss::core::result<void, transport_error>::err(transport_err));
        }
        
        // LOG_ERROR("UDP send error: {}", error.message());
    } else {
        update_stats(bytes_sent, 0);
        
        if (handler) {
            handler(ss::core::result<void, transport_error>::ok());
        }
        
        // LOG_TRACE("Sent {} bytes", bytes_sent);
    }
}

transport_error udp_transport::convert_error(const boost::system::error_code& error) const noexcept {
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

ss::core::endpoint udp_transport::convert_endpoint(const endpoint_type& ep) const noexcept {
    try {
        return ss::core::endpoint(ep.address().to_string(), ep.port());
    } catch (...) {
        return ss::core::endpoint{};
    }
}

udp_transport::endpoint_type udp_transport::convert_endpoint(const ss::core::endpoint& ep) const noexcept {
    try {
        return endpoint_type(boost::asio::ip::make_address(ep.address()), ep.port());
    } catch (...) {
        return endpoint_type{};
    }
}

void udp_transport::update_stats(std::size_t bytes_sent, std::size_t bytes_received,
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

std::uint64_t udp_transport::calculate_latency(const std::chrono::steady_clock::time_point& start_time) const noexcept {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    return static_cast<std::uint64_t>(duration.count());
}

void udp_transport::start_io_threads() {
    auto thread_count = std::max(1u, config_.io_threads);
    io_threads_.reserve(thread_count);
    
    for (std::uint32_t i = 0; i < thread_count; ++i) {
        io_threads_.emplace_back([this]() { io_worker(); });
    }
    
    // LOG_INFO("Started {} I/O worker threads", thread_count);
}

void udp_transport::stop_io_threads() {
    // Threads will stop when the io_context stops or when operations are cancelled
    for (auto& thread : io_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    io_threads_.clear();
    
    // LOG_INFO("Stopped I/O worker threads");
}

void udp_transport::io_worker() {
    // LOG_DEBUG("I/O worker thread started");
    
    try {
        while (is_running()) {
            try {
                io_context_.run_for(std::chrono::milliseconds(100));
            } catch (const std::exception& e) {
                if (is_running()) {
                    // LOG_ERROR("I/O worker error: {}", e.what());
                }
            }
        }
    } catch (const std::exception& e) {
        // LOG_ERROR("I/O worker thread error: {}", e.what());
    }
    
    // LOG_DEBUG("I/O worker thread stopped");
}

bool udp_transport::is_valid_message_size(std::size_t size) const noexcept {
    if (size == 0) {
        return false;
    }
    
    auto max_size = max_message_size();
    return max_size == 0 || size <= max_size;
}

ss::core::result<void, transport_error> udp_transport::apply_socket_options() {
    if (!socket_ || !socket_->is_open()) {
        return ss::core::result<void, transport_error>::err(transport_error::not_connected);
    }
    
    try {
        // Set socket buffer sizes
        socket_->set_option(boost::asio::socket_base::send_buffer_size(config_.send_buffer_size));
        socket_->set_option(boost::asio::socket_base::receive_buffer_size(config_.receive_buffer_size));
        
        // Enable address reuse
        socket_->set_option(boost::asio::socket_base::reuse_address(true));
        
        // LOG_DEBUG("Applied socket options: send_buffer={}, receive_buffer={}", 
        //          config_.send_buffer_size, config_.receive_buffer_size);
        
        return ss::core::result<void, transport_error>::ok();
        
    } catch (const boost::system::system_error& e) {
        auto error = convert_error(e.code());
        // LOG_ERROR("Failed to apply socket options: {}", e.what());
        return ss::core::result<void, transport_error>::err(error);
    }
}

std::vector<outgoing_message> udp_transport::fragment_message(const outgoing_message& message) const {
    std::vector<outgoing_message> fragments;
    
    if (message.data.size() <= DEFAULT_MAX_UDP_SIZE) {
        fragments.push_back(message);
        return fragments;
    }
    
    auto fragment_id = generate_fragment_id();
    auto total_fragments = (message.data.size() + DEFAULT_MAX_UDP_SIZE - 1) / DEFAULT_MAX_UDP_SIZE;
    
    for (std::size_t i = 0; i < total_fragments; ++i) {
        auto offset = i * DEFAULT_MAX_UDP_SIZE;
        auto fragment_size = std::min(DEFAULT_MAX_UDP_SIZE, message.data.size() - offset);
        
        // Create fragment header (simplified)
        struct fragment_header {
            std::uint32_t fragment_id;
            std::uint32_t fragment_index;
            std::uint32_t total_fragments;
            std::uint32_t fragment_size;
        };
        
        fragment_header header{
            fragment_id,
            static_cast<std::uint32_t>(i),
            static_cast<std::uint32_t>(total_fragments),
            static_cast<std::uint32_t>(fragment_size)
        };
        
        // Create fragment data
        std::vector<std::uint8_t> fragment_data(sizeof(header) + fragment_size);
        std::memcpy(fragment_data.data(), &header, sizeof(header));
        std::memcpy(fragment_data.data() + sizeof(header), 
                   message.data.data() + offset, fragment_size);
        
        // Create fragment message
        outgoing_message fragment(std::move(fragment_data), message.target);
        fragment.priority = message.priority;
        fragment.timeout = message.timeout;
        
        fragments.push_back(std::move(fragment));
    }
    
    return fragments;
}

std::optional<incoming_message> udp_transport::defragment_message(const incoming_message& fragment) {
    // Check if this is a fragment (simplified check)
    if (fragment.data.size() < sizeof(std::uint32_t) * 4) {
        // Not a fragment, return as-is
        return fragment;
    }
    
    // Extract fragment header
    struct fragment_header {
        std::uint32_t fragment_id;
        std::uint32_t fragment_index;
        std::uint32_t total_fragments;
        std::uint32_t fragment_size;
    };
    
    fragment_header header;
    std::memcpy(&header, fragment.data.data(), sizeof(header));
    
    // Validate header
    if (header.fragment_size + sizeof(header) != fragment.data.size()) {
        // Invalid fragment, return as-is
        return fragment;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto& state = fragment_map_[header.fragment_id];
    
    // Initialize fragment state if first fragment
    if (state.expected_size == 0) {
        state.expected_size = header.total_fragments * DEFAULT_MAX_UDP_SIZE;
        state.data.reserve(state.expected_size);
        state.first_fragment_time = std::chrono::steady_clock::now();
    }
    
    // Check if fragment already received
    if (state.received_fragments[header.fragment_index]) {
        return std::nullopt;
    }
    
    // Store fragment data
    auto payload_offset = sizeof(header);
    auto payload_size = header.fragment_size;
    
    // Ensure data vector is large enough
    auto required_size = header.fragment_index * DEFAULT_MAX_UDP_SIZE + payload_size;
    if (state.data.size() < required_size) {
        state.data.resize(required_size);
    }
    
    // Copy fragment payload
    std::memcpy(state.data.data() + header.fragment_index * DEFAULT_MAX_UDP_SIZE,
               fragment.data.data() + payload_offset, payload_size);
    
    state.received_fragments[header.fragment_index] = true;
    state.received_size += payload_size;
    
    // Check if all fragments received
    if (state.received_fragments.size() == header.total_fragments) {
        // Create complete message
        incoming_message complete_msg(std::move(state.data), fragment.sender);
        complete_msg.received_at = fragment.received_at;
        
        // Clean up fragment state
        fragment_map_.erase(header.fragment_id);
        
        return complete_msg;
    }
    
    return std::nullopt;
}

void udp_transport::cleanup_expired_fragments() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto it = fragment_map_.begin();
    
    while (it != fragment_map_.end()) {
        if (now - it->second.first_fragment_time > FRAGMENT_TIMEOUT) {
            // LOG_WARN("Fragment {} expired, removing", it->first);
            it = fragment_map_.erase(it);
        } else {
            ++it;
        }
    }
}

void udp_transport::start_fragment_cleanup_timer() {
    if (!fragment_cleanup_timer_) {
        return;
    }
    
    fragment_cleanup_timer_->expires_after(FRAGMENT_CLEANUP_INTERVAL);
    fragment_cleanup_timer_->async_wait(
        [this](const boost::system::error_code& error) {
            if (!error && is_running()) {
                cleanup_expired_fragments();
                start_fragment_cleanup_timer();
            }
        }
    );
}

} // namespace ss::network::impl