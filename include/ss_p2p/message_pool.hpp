#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <queue>
#include <atomic>
#include <shared_mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <chrono>
#include <optional>
#include <array>

#include <boost/asio.hpp>

#include "message.hpp"
#include "message_pool.fwd.hpp"

namespace ss {

// Simple concurrent message queue (replacing boost::lockfree for compatibility)
template<size_t MaxSize = 1024>
class concurrent_message_queue {
public:
    struct queue_entry {
        message_ptr msg;
        endpoint_t source;
        time_point_t enqueue_time;
        
        queue_entry() = default;
        queue_entry(message_ptr m, endpoint_t ep) 
            : msg(std::move(m)), source(ep), enqueue_time(std::chrono::steady_clock::now()) {}
    };
    
    concurrent_message_queue() = default;
    
    bool push(message_ptr msg, const endpoint_t& source) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= MaxSize) {
            return false;
        }
        queue_.emplace(std::move(msg), source);
        return true;
    }
    
    result<queue_entry> pop() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        queue_entry entry = std::move(queue_.front());
        queue_.pop();
        return entry;
    }
    
    bool empty() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    size_t size() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
private:
    mutable std::mutex mutex_;
    std::queue<queue_entry> queue_;
};

// Memory pool for efficient message buffer allocation
class message_memory_pool {
public:
    explicit message_memory_pool(size_t pool_size = constants::default_pool_size);
    ~message_memory_pool();
    
    // Disable copy and move (due to mutex)
    message_memory_pool(const message_memory_pool&) = delete;
    message_memory_pool& operator=(const message_memory_pool&) = delete;
    message_memory_pool(message_memory_pool&&) = delete;
    message_memory_pool& operator=(message_memory_pool&&) = delete;
    
    // Allocate buffer from pool
    std::unique_ptr<uint8_t[]> allocate(size_t size) noexcept;
    
    // Return buffer to pool
    void deallocate(std::unique_ptr<uint8_t[]> buffer, size_t size) noexcept;
    
    // Pool statistics
    struct pool_stats {
        std::atomic<size_t> total_allocations{0};
        std::atomic<size_t> active_allocations{0};
        std::atomic<size_t> pool_hits{0};
        std::atomic<size_t> pool_misses{0};
        std::atomic<size_t> total_memory{0};
    };
    
    const pool_stats& get_stats() const noexcept { return stats_; }
    
private:
    struct buffer_entry {
        std::unique_ptr<uint8_t[]> buffer;
        size_t size;
        time_point_t last_used;
    };
    
    mutable std::shared_mutex mutex_;
    std::vector<buffer_entry> available_buffers_;
    mutable pool_stats stats_;
    
    void cleanup_old_buffers() noexcept;
};

// Per-peer message buffer with timeout management
class message_buffer {
public:
    explicit message_buffer(endpoint_t endpoint, size_t max_size = constants::default_buffer_size);
    ~message_buffer() = default;
    
    // Disable copy and move (due to mutex)
    message_buffer(const message_buffer&) = delete;
    message_buffer& operator=(const message_buffer&) = delete;
    message_buffer(message_buffer&&) = delete;
    message_buffer& operator=(message_buffer&&) = delete;
    
    // Message operations
    bool push_message(message_ptr msg) noexcept;
    result<message_ptr> pop_message(duration_t timeout = duration_t{0}) noexcept;
    result<message_ptr> peek_message() const noexcept;
    
    // Buffer management
    void clear_messages() noexcept;
    size_t message_count() const noexcept;
    size_t estimated_memory_usage() const noexcept;
    
    // Timeout operations
    void remove_expired_messages(duration_t max_age = constants::default_message_timeout) noexcept;
    
    // Access control
    endpoint_t get_endpoint() const noexcept { return endpoint_; }
    time_point_t last_activity() const noexcept;
    void update_activity() noexcept;
    
private:
    struct buffered_message {
        message_ptr msg;
        time_point_t received_at;
        
        buffered_message(message_ptr m) 
            : msg(std::move(m)), received_at(std::chrono::steady_clock::now()) {}
    };
    
    endpoint_t endpoint_;
    mutable std::shared_mutex mutex_;
    std::queue<buffered_message> message_queue_;
    std::condition_variable_any condition_;
    std::atomic<time_point_t> last_activity_;
    std::atomic<size_t> current_size_;
    const size_t max_size_;
};

// Message routing and distribution hub
class message_hub {
public:
    explicit message_hub(boost::asio::io_context& io_context);
    ~message_hub();
    
    // Disable copy and move
    message_hub(const message_hub&) = delete;
    message_hub& operator=(const message_hub&) = delete;
    message_hub(message_hub&&) = delete;
    message_hub& operator=(message_hub&&) = delete;
    
    // Lifecycle management
    void start(message_handler_t handler);
    void stop() noexcept;
    bool is_running() const noexcept;
    
    // Message distribution
    void distribute_message(message_ptr msg, const endpoint_t& source) noexcept;
    
    // Handler registration
    void set_message_handler(message_handler_t handler) noexcept;
    void set_peer_resolver(endpoint_to_peer_func_t resolver) noexcept;
    
    // Statistics
    struct hub_metrics {
        std::atomic<uint64_t> messages_processed{0};
        std::atomic<uint64_t> messages_dropped{0};
        std::atomic<uint64_t> processing_errors{0};
        std::atomic<duration_t::rep> avg_processing_time{0};
    };
    
    const hub_metrics& get_metrics() const noexcept { return metrics_; }
    
private:
    boost::asio::io_context& io_context_;
    std::unique_ptr<std::thread> worker_thread_;
    concurrent_message_queue<> message_queue_;
    
    std::atomic<bool> running_{false};
    message_handler_t message_handler_;
    endpoint_to_peer_func_t peer_resolver_;
    hub_metrics metrics_;
    
    void worker_loop() noexcept;
    void process_message(message_ptr msg, const endpoint_t& source) noexcept;
};

// Utility functions for endpoint handling
namespace pool_utils {
    
    // Endpoint hash function for unordered_map
    struct endpoint_hash {
        size_t operator()(const endpoint_t& ep) const noexcept {
            auto addr_hash = std::hash<std::string>{}(ep.address().to_string());
            auto port_hash = std::hash<uint16_t>{}(ep.port());
            return addr_hash ^ (port_hash << 1);
        }
    };
    
    // Endpoint comparison
    struct endpoint_equal {
        bool operator()(const endpoint_t& lhs, const endpoint_t& rhs) const noexcept {
            return lhs.address() == rhs.address() && lhs.port() == rhs.port();
        }
    };
    
} // namespace pool_utils

// Main message pool with comprehensive management
class message_pool {
public:
    // Type alias for backward compatibility
    using message_hub = ss::message_hub;
    explicit message_pool(
        boost::asio::io_context& io_context,
        endpoint_to_peer_func_t peer_resolver,
        size_t pool_size = constants::default_pool_size
    );
    
    ~message_pool();
    
    // Disable copy and move
    message_pool(const message_pool&) = delete;
    message_pool& operator=(const message_pool&) = delete;
    message_pool(message_pool&&) = delete;
    message_pool& operator=(message_pool&&) = delete;
    
    // Lifecycle management
    void start();
    void stop() noexcept;
    bool is_running() const noexcept;
    
    // Message operations
    void store_message(message_ptr msg, const endpoint_t& source) noexcept;
    result<message_ptr> retrieve_message(const endpoint_t& endpoint, duration_t timeout = duration_t{100}) noexcept;
    
    // Buffer management
    message_buffer_ptr get_or_create_buffer(const endpoint_t& endpoint) noexcept;
    bool remove_buffer(const endpoint_t& endpoint) noexcept;
    void cleanup_inactive_buffers(duration_t max_idle = std::chrono::minutes{10}) noexcept;
    
    // Message hub access
    message_hub& get_message_hub() noexcept { return hub_; }
    const message_hub& get_message_hub() const noexcept { return hub_; }
    
    // Memory management
    message_memory_pool& get_memory_pool() noexcept { return memory_pool_; }
    const message_memory_pool& get_memory_pool() const noexcept { return memory_pool_; }
    
    // Statistics and monitoring
    struct pool_statistics {
        size_t active_buffers{0};
        size_t total_messages{0};
        size_t memory_usage{0};
        duration_t avg_message_age{0};
        
        // Performance metrics
        uint64_t messages_stored{0};
        uint64_t messages_retrieved{0};
        uint64_t buffer_cache_hits{0};
        uint64_t buffer_cache_misses{0};
    };
    
    pool_statistics get_statistics() const noexcept;
    
    // Configuration
    void set_cleanup_interval(duration_t interval) noexcept;
    void set_message_timeout(duration_t timeout) noexcept;
    void set_max_buffer_size(size_t size) noexcept;
    
private:
    boost::asio::io_context& io_context_;
    boost::asio::steady_timer cleanup_timer_;
    
    mutable std::shared_mutex buffers_mutex_;
    std::unordered_map<endpoint_t, message_buffer_ptr, pool_utils::endpoint_hash, pool_utils::endpoint_equal> message_buffers_;
    
    message_memory_pool memory_pool_;
    message_hub hub_;
    endpoint_to_peer_func_t peer_resolver_;
    
    std::atomic<bool> running_{false};
    std::atomic<duration_t::rep> cleanup_interval_{std::chrono::minutes{5}.count()};
    std::atomic<duration_t::rep> message_timeout_{constants::default_message_timeout.count()};
    std::atomic<size_t> max_buffer_size_{constants::default_buffer_size};
    
    // Performance counters
    mutable std::atomic<uint64_t> messages_stored_{0};
    mutable std::atomic<uint64_t> messages_retrieved_{0};
    mutable std::atomic<uint64_t> buffer_cache_hits_{0};
    mutable std::atomic<uint64_t> buffer_cache_misses_{0};
    
    void schedule_cleanup() noexcept;
    void perform_cleanup(const boost::system::error_code& ec) noexcept;
    message_buffer_ptr find_buffer(const endpoint_t& endpoint) const noexcept;
};

namespace pool_utils {
    // Create typed message pools
    std::unique_ptr<message_pool> create_pool(
        boost::asio::io_context& io_context,
        endpoint_to_peer_func_t peer_resolver,
        size_t pool_size = constants::default_pool_size
    );
} // namespace pool_utils

} // namespace ss