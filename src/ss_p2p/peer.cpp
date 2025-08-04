#include <ss_p2p/peer.hpp>
#include <ss_p2p/core/types.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <chrono>
#include <random>
#include <sstream>
#include <algorithm>
#include <cassert>

using namespace boost::asio;
using namespace boost::asio::experimental::awaitable_operators;

namespace ss {

namespace {
    // Internal message types
    constexpr const char* PING_MESSAGE_TYPE = "__ss_ping";
    constexpr const char* PONG_MESSAGE_TYPE = "__ss_pong";
    constexpr const char* REQUEST_PREFIX = "__ss_req_";
    constexpr const char* RESPONSE_PREFIX = "__ss_resp_";
    
    // Metadata keys
    constexpr const char* REQUEST_ID_KEY = "__request_id";
    constexpr const char* TIMESTAMP_KEY = "__timestamp";
    constexpr const char* RTT_KEY = "__rtt_us";
    
    // Thread-safe message ID generator
    std::atomic<std::uint64_t> global_message_counter{1};
}

// Utility functions for error/state conversion
std::string to_string(peer_error error) noexcept {
    switch (error) {
        case peer_error::none: return "none";
        case peer_error::not_connected: return "not_connected";
        case peer_error::send_failed: return "send_failed";
        case peer_error::receive_failed: return "receive_failed";
        case peer_error::timeout: return "timeout";
        case peer_error::invalid_message: return "invalid_message";
        case peer_error::unreachable: return "unreachable";
        case peer_error::connection_lost: return "connection_lost";
        case peer_error::auth_failed: return "auth_failed";
        case peer_error::rate_limited: return "rate_limited";
        case peer_error::resource_exhausted: return "resource_exhausted";
        case peer_error::unknown: return "unknown";
        default: return "unknown";
    }
}

std::string to_string(peer_state state) noexcept {
    switch (state) {
        case peer_state::disconnected: return "disconnected";
        case peer_state::connecting: return "connecting";
        case peer_state::connected: return "connected";
        case peer_state::disconnecting: return "disconnecting";
        case peer_state::failed: return "failed";
        default: return "unknown";
    }
}

// peer implementation
peer::peer(core::peer_id id, 
           core::endpoint endpoint,
           network::transport_ptr transport,
           boost::asio::io_context& io_ctx,
           const peer_config& config)
    : peer_id_(std::move(id))
    , endpoint_(std::move(endpoint))
    , transport_(std::move(transport))
    , io_context_(io_ctx)
    , config_(config)
    , last_activity_time_(std::chrono::steady_clock::now()) {
    
    assert(transport_ && "Transport must not be null");
    
    // Initialize message buffers with pool configuration
    network::message_buffer::config buffer_config{};
    buffer_config.buffer_capacity = config_.max_queue_size;
    buffer_config.default_timeout = config_.send_timeout;
    
    send_buffer_ = std::make_unique<network::message_buffer>(buffer_config);
    receive_buffer_ = std::make_unique<network::message_buffer>(buffer_config);
    
    // Initialize keep-alive timer if enabled
    if (config_.enable_keep_alive) {
        keep_alive_timer_ = std::make_unique<boost::asio::steady_timer>(io_context_);
    }
    
    // Set up transport message handler
    transport_->set_message_handler(
        [this](network::incoming_message msg) -> core::async_void {
            handle_incoming_message(std::move(msg));
            co_return;
        });
    
    // Initialize statistics
    if (config_.enable_stats) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.current_state = current_state_.load();
    }
}

peer::~peer() {
    // Ensure clean shutdown
    if (current_state_.load() != peer_state::disconnected) {
        try {
            // Best effort graceful shutdown without blocking destructor
            // Note: In practice, this would need access to an IO context
        } catch (...) {
            // Ignore exceptions in destructor
        }
    }
}

peer_state peer::state() const noexcept {
    return current_state_.load();
}

bool peer::is_connected() const noexcept {
    return current_state_.load() == peer_state::connected;
}

core::async_result<core::result<void, peer_error>>
peer::connect(std::chrono::milliseconds timeout) {
    // Check if already connected
    auto current = current_state_.load();
    if (current == peer_state::connected) {
        co_return core::result<void, peer_error>::ok();
    }
    
    if (current == peer_state::connecting) {
        co_return core::result<void, peer_error>::err(peer_error::resource_exhausted);
    }
    
    update_state(peer_state::connecting);
    
    try {
        // Establish transport connection
        auto connect_result = co_await transport_->connect(endpoint_);
        
        if (!connect_result.is_ok()) {
            update_state(peer_state::failed);
            co_return core::result<void, peer_error>::err(peer_error::unreachable);
        }
        
        update_state(peer_state::connected);
        
        // Update statistics
        if (config_.enable_stats) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.connected_at = std::chrono::steady_clock::now();
        }
        
        // Start keep-alive if enabled
        if (config_.enable_keep_alive) {
            start_keep_alive();
        }
        
        co_return core::result<void, peer_error>::ok();
        
    } catch (const std::exception& e) {
        update_state(peer_state::failed);
        handle_error(peer_error::connection_lost, e.what());
        co_return core::result<void, peer_error>::err(peer_error::connection_lost);
    }
}

core::async_void peer::disconnect(bool graceful) {
    auto current = current_state_.load();
    if (current == peer_state::disconnected || current == peer_state::disconnecting) {
        co_return;
    }
    
    update_state(peer_state::disconnecting);
    
    try {
        // Stop keep-alive
        stop_keep_alive();
        
        // Clear message queues if not graceful
        if (!graceful) {
            clear_queues();
        }
        
        // Disconnect transport
        co_await transport_->disconnect();
        
        update_state(peer_state::disconnected);
        
    } catch (const std::exception& e) {
        handle_error(peer_error::connection_lost, e.what());
        update_state(peer_state::failed);
    }
}

core::async_result<core::result<void, peer_error>>
peer::send(peer_message message) {
    co_return co_await send_timeout(std::move(message), config_.send_timeout);
}

core::async_result<core::result<void, peer_error>>
peer::send_timeout(peer_message message, std::chrono::milliseconds timeout) {
    if (!is_connected()) {
        co_return core::result<void, peer_error>::err(peer_error::not_connected);
    }
    
    if (!can_send()) {
        co_return core::result<void, peer_error>::err(peer_error::resource_exhausted);
    }
    
    try {
        // Create outgoing message
        network::outgoing_message out_msg(std::move(message.payload), endpoint_);
        out_msg.priority = message.priority;
        out_msg.timeout = timeout;
        out_msg.metadata = message.message_type;
        
        // Add metadata
        if (!message.metadata.empty()) {
            std::ostringstream oss;
            for (const auto& [key, value] : message.metadata) {
                oss << key << "=" << value << ";";
            }
            out_msg.metadata += "|" + oss.str();
        }
        
        // Send via transport
        auto send_result = co_await transport_->send(std::move(out_msg));
        
        if (send_result.is_ok()) {
            update_stats_on_send(message.payload.size());
            last_activity_time_.store(std::chrono::steady_clock::now());
            co_return core::result<void, peer_error>::ok();
        } else {
            handle_error(peer_error::send_failed, "Transport send failed");
            co_return core::result<void, peer_error>::err(peer_error::send_failed);
        }
        
    } catch (const std::exception& e) {
        handle_error(peer_error::send_failed, e.what());
        co_return core::result<void, peer_error>::err(peer_error::send_failed);
    }
}

core::async_result<core::result<peer_message, peer_error>>
peer::receive(std::chrono::milliseconds timeout) {
    if (!is_connected()) {
        co_return core::result<peer_message, peer_error>::err(peer_error::not_connected);
    }
    
    try {
        // Try to get message from receive buffer
        auto buffer_result = receive_buffer_->pop_timeout(timeout);
        
        if (!buffer_result.is_ok()) {
            if (buffer_result.error() == network::buffer_error::timeout) {
                co_return core::result<peer_message, peer_error>::err(peer_error::timeout);
            } else {
                co_return core::result<peer_message, peer_error>::err(peer_error::receive_failed);
            }
        }
        
        auto& buffered_msg = buffer_result.value();
        
        // Convert to peer_message
        peer_message msg(
            std::vector<std::uint8_t>(buffered_msg.data.span<std::uint8_t>().begin(),
                                     buffered_msg.data.span<std::uint8_t>().end()),
            "default" // Will be overridden by metadata parsing
        );
        
        // Parse metadata if available
        // Implementation would parse the metadata string from transport
        
        update_stats_on_receive(msg.payload.size());
        last_activity_time_.store(std::chrono::steady_clock::now());
        
        co_return core::result<peer_message, peer_error>::ok(std::move(msg));
        
    } catch (const std::exception& e) {
        handle_error(peer_error::receive_failed, e.what());
        co_return core::result<peer_message, peer_error>::err(peer_error::receive_failed);
    }
}

core::result<peer_message, peer_error> peer::try_receive() {
    if (!is_connected()) {
        return core::result<peer_message, peer_error>::err(peer_error::not_connected);
    }
    
    try {
        auto buffer_result = receive_buffer_->try_pop();
        
        if (!buffer_result.is_ok()) {
            return core::result<peer_message, peer_error>::err(peer_error::receive_failed);
        }
        
        auto& buffered_msg = buffer_result.value();
        
        peer_message msg(
            std::vector<std::uint8_t>(buffered_msg.data.span<std::uint8_t>().begin(),
                                     buffered_msg.data.span<std::uint8_t>().end())
        );
        
        update_stats_on_receive(msg.payload.size());
        
        return core::result<peer_message, peer_error>::ok(std::move(msg));
        
    } catch (const std::exception& e) {
        handle_error(peer_error::receive_failed, e.what());
        return core::result<peer_message, peer_error>::err(peer_error::receive_failed);
    }
}

core::async_result<core::result<peer_message, peer_error>>
peer::request(peer_message request, std::chrono::milliseconds timeout) {
    if (!is_connected()) {
        co_return core::result<peer_message, peer_error>::err(peer_error::not_connected);
    }
    
    try {
        // Generate unique request ID
        auto request_id = generate_request_id();
        
        // Mark as request message
        request.message_type = std::string(REQUEST_PREFIX) + request.message_type;
        request.metadata[REQUEST_ID_KEY] = std::to_string(request_id.value());
        
        // Set up promise for response
        std::promise<peer_message> response_promise;
        auto response_future = response_promise.get_future();
        
        {
            std::lock_guard<std::mutex> lock(pending_requests_mutex_);
            pending_requests_[request_id] = std::move(response_promise);
        }
        
        // Send request
        auto send_result = co_await send_timeout(std::move(request), timeout);
        if (!send_result.is_ok()) {
            // Clean up pending request
            std::lock_guard<std::mutex> lock(pending_requests_mutex_);
            pending_requests_.erase(request_id);
            co_return core::result<peer_message, peer_error>::err(send_result.error());
        }
        
        // Wait for response with timeout
        auto wait_status = response_future.wait_for(timeout);
        if (wait_status == std::future_status::timeout) {
            std::lock_guard<std::mutex> lock(pending_requests_mutex_);
            pending_requests_.erase(request_id);
            co_return core::result<peer_message, peer_error>::err(peer_error::timeout);
        }
        
        auto response = response_future.get();
        co_return core::result<peer_message, peer_error>::ok(std::move(response));
        
    } catch (const std::exception& e) {
        handle_error(peer_error::receive_failed, e.what());
        co_return core::result<peer_message, peer_error>::err(peer_error::receive_failed);
    }
}

core::async_result<core::result<void, peer_error>>
peer::respond(peer_message response, core::message_id request_id) {
    if (!is_connected()) {
        co_return core::result<void, peer_error>::err(peer_error::not_connected);
    }
    
    // Mark as response message
    response.message_type = std::string(RESPONSE_PREFIX) + response.message_type;
    response.metadata[REQUEST_ID_KEY] = std::to_string(request_id.value());
    
    co_return co_await send(std::move(response));
}

core::async_result<core::result<std::chrono::microseconds, peer_error>>
peer::ping(std::chrono::milliseconds timeout) {
    if (!is_connected()) {
        co_return core::result<std::chrono::microseconds, peer_error>::err(peer_error::not_connected);
    }
    
    try {
        auto start_time = std::chrono::steady_clock::now();
        
        // Create ping message
        peer_message ping_msg(std::vector<std::uint8_t>{}, PING_MESSAGE_TYPE);
        ping_msg.metadata[TIMESTAMP_KEY] = std::to_string(
            std::chrono::duration_cast<std::chrono::microseconds>(
                start_time.time_since_epoch()).count());
        
        // Send ping and wait for pong
        auto response_result = co_await request(std::move(ping_msg), timeout);
        
        if (!response_result.is_ok()) {
            co_return core::result<std::chrono::microseconds, peer_error>::err(response_result.error());
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto rtt = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        // Update statistics
        if (config_.enable_stats) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.last_ping_time = end_time;
            stats_.avg_rtt_us = (stats_.avg_rtt_us + rtt.count()) / 2; // Simple moving average
        }
        
        co_return core::result<std::chrono::microseconds, peer_error>::ok(rtt);
        
    } catch (const std::exception& e) {
        handle_error(peer_error::unreachable, e.what());
        co_return core::result<std::chrono::microseconds, peer_error>::err(peer_error::unreachable);
    }
}

bool peer::is_alive() const noexcept {
    if (!config_.enable_stats) {
        return is_connected();
    }
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    auto now = std::chrono::steady_clock::now();
    auto last_activity = last_activity_time_.load();
    
    // Consider alive if recent activity within 2x ping interval
    auto activity_threshold = config_.ping_interval * 2;
    return (now - last_activity) < activity_threshold;
}

void peer::set_message_handler(message_handler handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    message_handler_ = std::move(handler);
}

void peer::set_state_change_handler(state_change_handler handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    state_change_handler_ = std::move(handler);
}

void peer::set_error_handler(error_handler handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    error_handler_ = std::move(handler);
}

void peer::clear_handlers() {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    message_handler_ = {};
    state_change_handler_ = {};
    error_handler_ = {};
}

peer_stats peer::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    peer_stats result = stats_;
    result.current_state = current_state_.load();
    return result;
}

void peer::reset_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = peer_stats{};
    stats_.current_state = current_state_.load();
}

peer_config peer::get_config() const {
    return config_;
}

core::result<void, peer_error> peer::set_config(const peer_config& config) {
    config_ = config;
    
    // Update buffer configurations if needed
    // This would require extending message_buffer interface
    
    return core::result<void, peer_error>::ok();
}

std::uint32_t peer::send_queue_size() const noexcept {
    return static_cast<std::uint32_t>(send_buffer_->size());
}

std::uint32_t peer::receive_queue_size() const noexcept {
    return static_cast<std::uint32_t>(receive_buffer_->size());
}

bool peer::can_send() const noexcept {
    return !send_buffer_->full() && transport_->can_send();
}

void peer::clear_queues() {
    send_buffer_->clear();
    receive_buffer_->clear();
}

core::async_void peer::start() {
    try {
        auto connect_result = co_await connect();
        if (connect_result.is_ok()) {
            co_return ;
        } else {
            co_return ;
        }
    } catch (const std::exception&) {
        co_return ;
    }
}

core::async_void peer::stop() {
    co_await disconnect(true);
}

// Private implementation methods

void peer::update_state(peer_state new_state) {
    auto old_state = current_state_.load();
    current_state_.store(new_state);
    
    // Update statistics
    if (config_.enable_stats) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.current_state = new_state;
    }
    
    // Notify handler if state changed
    if (old_state != new_state) {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        if (state_change_handler_) {
            state_change_handler_(old_state, new_state);
        }
    }
}

void peer::handle_incoming_message(network::incoming_message message) {
    try {
        // Convert to peer_message
        peer_message peer_msg(std::move(message.data));
        
        // Parse message type and metadata from transport metadata
        auto& metadata_str = message.metadata;
        size_t sep_pos = metadata_str.find('|');
        if (sep_pos != std::string::npos) {
            peer_msg.message_type = metadata_str.substr(0, sep_pos);
            // Parse additional metadata from remaining string
            // Implementation would parse key=value pairs
        } else {
            peer_msg.message_type = metadata_str;
        }
        
        // Handle special message types
        if (peer_msg.message_type == PING_MESSAGE_TYPE) {
            // Send pong response
            peer_message pong_msg(std::vector<std::uint8_t>{}, PONG_MESSAGE_TYPE);
            pong_msg.metadata = peer_msg.metadata; // Echo metadata
            
            boost::asio::co_spawn(io_context_,
                                 send(std::move(pong_msg)),
                                 boost::asio::detached);
            return;
        }
        
        // Handle request-response messages
        if (peer_msg.message_type.starts_with(RESPONSE_PREFIX)) {
            auto req_id_it = peer_msg.metadata.find(REQUEST_ID_KEY);
            if (req_id_it != peer_msg.metadata.end()) {
                auto request_id = core::message_id(std::stoull(req_id_it->second));
                complete_pending_request(request_id, std::move(peer_msg));
                return;
            }
        }
        
        // Buffer regular messages
        auto buffer_block = network::memory_block(receive_buffer_->pool());
        if (buffer_block.is_valid() && buffer_block.size() >= peer_msg.payload.size()) {
            std::memcpy(buffer_block.data(), peer_msg.payload.data(), peer_msg.payload.size());
            
            auto buffer_result = receive_buffer_->push_zero_copy(
                std::move(buffer_block), peer_msg.payload.size(), peer_msg.priority);
            
            if (!buffer_result.is_ok()) {
                handle_error(peer_error::resource_exhausted, "Receive buffer full");
            }
        }
        
        // Notify message handler
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        if (message_handler_) {
            boost::asio::co_spawn(io_context_,
                                 message_handler_(std::move(peer_msg)),
                                 boost::asio::detached);
        }
        
        update_stats_on_receive(peer_msg.payload.size());
        last_activity_time_.store(std::chrono::steady_clock::now());
        
    } catch (const std::exception& e) {
        handle_error(peer_error::invalid_message, e.what());
    }
}

void peer::handle_transport_error(network::transport_error error) {
    peer_error peer_err = peer_error::connection_lost;
    
    switch (error) {
        case network::transport_error::timeout:
            peer_err = peer_error::timeout;
            break;
        case network::transport_error::host_unreachable:
        case network::transport_error::port_unreachable:
            peer_err = peer_error::unreachable;
            break;
        case network::transport_error::connection_reset:
        case network::transport_error::connection_aborted:
            peer_err = peer_error::connection_lost;
            break;
        default:
            peer_err = peer_error::unknown;
            break;
    }
    
    handle_error(peer_err, "Transport error: " + to_string(error));
    
    if (current_state_.load() == peer_state::connected) {
        update_state(peer_state::failed);
    }
}

void peer::start_keep_alive() {
    if (!keep_alive_timer_ || !config_.enable_keep_alive) {
        return;
    }
    
    boost::asio::co_spawn(io_context_,
                         keep_alive_loop(),
                         boost::asio::detached);
}

void peer::stop_keep_alive() {
    if (keep_alive_timer_) {
        boost::system::error_code ec;
        keep_alive_timer_->cancel();
    }
}

core::async_void peer::keep_alive_loop() {
    while (is_connected() && config_.enable_keep_alive) {
        try {
            keep_alive_timer_->expires_after(config_.ping_interval);
            co_await keep_alive_timer_->async_wait(boost::asio::use_awaitable);
            
            if (is_connected()) {
                auto ping_result = co_await ping(config_.ping_timeout);
                if (!ping_result.is_ok()) {
                    handle_error(peer_error::unreachable, "Keep-alive ping failed");
                    update_state(peer_state::failed);
                    break;
                }
            }
        } catch (const boost::system::system_error& e) {
            if (e.code() == boost::asio::error::operation_aborted) {
                break; // Timer was cancelled
            }
            handle_error(peer_error::connection_lost, e.what());
            break;
        } catch (const std::exception& e) {
            handle_error(peer_error::unknown, e.what());
            break;
        }
    }
}

void peer::update_stats_on_send(std::size_t bytes) {
    if (!config_.enable_stats) return;
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.messages_sent++;
    stats_.bytes_sent += bytes;
    stats_.last_send_time = std::chrono::steady_clock::now();
}

void peer::update_stats_on_receive(std::size_t bytes) {
    if (!config_.enable_stats) return;
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.messages_received++;
    stats_.bytes_received += bytes;
    stats_.last_receive_time = std::chrono::steady_clock::now();
}

void peer::handle_error(peer_error error, const std::string& details) {
    if (!config_.enable_stats) return;
    
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        if (error == peer_error::send_failed) {
            stats_.send_errors++;
        } else if (error == peer_error::receive_failed) {
            stats_.receive_errors++;
        }
    }
    
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    if (error_handler_) {
        error_handler_(error, details);
    }
}

core::message_id peer::generate_request_id() {
    return core::message_id(global_message_counter.fetch_add(1, std::memory_order_relaxed));
}

bool peer::is_request_response(const peer_message& message) const {
    return message.message_type.starts_with(REQUEST_PREFIX) ||
           message.message_type.starts_with(RESPONSE_PREFIX);
}

std::optional<core::message_id> peer::extract_request_id(const peer_message& message) const {
    auto it = message.metadata.find(REQUEST_ID_KEY);
    if (it != message.metadata.end()) {
        try {
            return core::message_id(std::stoull(it->second));
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

void peer::complete_pending_request(core::message_id request_id, peer_message response) {
    std::lock_guard<std::mutex> lock(pending_requests_mutex_);
    auto it = pending_requests_.find(request_id);
    if (it != pending_requests_.end()) {
        try {
            it->second.set_value(std::move(response));
        } catch (const std::exception&) {
            // Promise already satisfied or moved
        }
        pending_requests_.erase(it);
    }
}

} // namespace ss