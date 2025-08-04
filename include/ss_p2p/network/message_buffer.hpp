#pragma once

#include "../core/types.hpp"
#include "../core/result.hpp"

#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <span>
#include <type_traits>
#include <new>
#include <cstring>
#include <cassert>

namespace ss::network {

/**
 * @brief Buffer error codes
 */
enum class buffer_error {
    /// No error occurred
    none = 0,
    /// Buffer is full
    buffer_full,
    /// Buffer is empty
    buffer_empty,
    /// Invalid size requested
    invalid_size,
    /// Out of memory
    out_of_memory,
    /// Buffer is corrupted
    corruption,
    /// Operation would block
    would_block,
    /// Operation timed out
    timeout,
    /// Invalid alignment
    invalid_alignment,
    /// Unknown error
    unknown
};

/**
 * @brief Convert buffer_error to string representation
 * @param error Error code to convert
 * @return String description of the error
 */
std::string to_string(buffer_error error) noexcept;

/**
 * @brief Memory pool for zero-copy buffer allocation
 * 
 * Provides efficient allocation and deallocation of memory blocks
 * with configurable alignment and pooling strategies.
 */
class memory_pool {
public:
    /**
     * @brief Pool configuration
     */
    struct config {
        /// Size of each memory block
        std::size_t block_size = 65536;  // 64KB
        /// Initial number of blocks to allocate
        std::size_t initial_blocks = 100;
        /// Maximum number of blocks in pool
        std::size_t max_blocks = 1000;
        /// Memory alignment requirement
        std::size_t alignment = alignof(std::max_align_t);
        /// Enable statistics collection
        bool enable_stats = true;
    };

    /**
     * @brief Pool statistics
     */
    struct stats {
        /// Total blocks allocated
        std::uint64_t total_allocated = 0;
        /// Total blocks deallocated  
        std::uint64_t total_deallocated = 0;
        /// Current blocks in use
        std::uint64_t blocks_in_use = 0;
        /// Current blocks available
        std::uint64_t blocks_available = 0;
        /// Peak blocks in use
        std::uint64_t peak_blocks_in_use = 0;
        /// Total allocation failures
        std::uint64_t allocation_failures = 0;
        /// Total bytes allocated
        std::uint64_t total_bytes_allocated = 0;
    };

    /**
     * @brief Constructor with configuration
     * @param cfg Pool configuration
     */
    explicit memory_pool(const config& cfg);
    
    /**
     * @brief Default constructor
     */
    memory_pool() : memory_pool(config{}) {}

    /**
     * @brief Destructor
     */
    ~memory_pool();

    /**
     * @brief Allocate a memory block
     * @return Pointer to allocated block or nullptr on failure
     */
    void* allocate() noexcept;

    /**
     * @brief Deallocate a memory block
     * @param ptr Pointer to block to deallocate
     */
    void deallocate(void* ptr) noexcept;

    /**
     * @brief Get block size
     * @return Size of each memory block in bytes
     */
    std::size_t block_size() const noexcept { return config_.block_size; }

    /**
     * @brief Get memory alignment
     * @return Alignment requirement in bytes
     */
    std::size_t alignment() const noexcept { return config_.alignment; }

    /**
     * @brief Get current statistics
     * @return Pool statistics snapshot
     */
    stats get_stats() const noexcept;

    /**
     * @brief Reset statistics
     */
    void reset_stats() noexcept;

    /**
     * @brief Check if pointer belongs to this pool
     * @param ptr Pointer to check
     * @return true if pointer is from this pool
     */
    bool owns(void* ptr) const noexcept;

private:
    config config_;
    mutable std::mutex mutex_;
    std::vector<void*> available_blocks_;
    std::vector<std::unique_ptr<std::uint8_t[]>> allocated_chunks_;
    mutable stats stats_;

    void expand_pool();
    bool is_pool_pointer(void* ptr) const noexcept;
};

/**
 * @brief RAII wrapper for memory pool allocations
 */
class memory_block {
public:
    /**
     * @brief Default constructor - empty block
     */
    memory_block() = default;

    /**
     * @brief Constructor with pool allocation
     * @param pool Memory pool to allocate from
     */
    explicit memory_block(memory_pool& pool);

    /**
     * @brief Constructor with external memory
     * @param data External memory pointer
     * @param size Memory size
     * @param deleter Custom deleter function
     */
    memory_block(void* data, std::size_t size, std::function<void(void*)> deleter = nullptr);

    /**
     * @brief Move constructor
     */
    memory_block(memory_block&& other) noexcept;

    /**
     * @brief Move assignment operator
     */
    memory_block& operator=(memory_block&& other) noexcept;

    /**
     * @brief Destructor
     */
    ~memory_block();

    /**
     * @brief Copy constructor (deleted)
     */
    memory_block(const memory_block&) = delete;

    /**
     * @brief Copy assignment operator (deleted)
     */
    memory_block& operator=(const memory_block&) = delete;

    /**
     * @brief Get raw data pointer
     * @return Pointer to memory block
     */
    void* data() noexcept { return data_; }

    /**
     * @brief Get const raw data pointer
     * @return Const pointer to memory block
     */
    const void* data() const noexcept { return data_; }

    /**
     * @brief Get memory block size
     * @return Size in bytes
     */
    std::size_t size() const noexcept { return size_; }

    /**
     * @brief Check if block is valid
     * @return true if block contains valid memory
     */
    bool is_valid() const noexcept { return data_ != nullptr; }

    /**
     * @brief Reset block to empty state
     */
    void reset() noexcept;

    /**
     * @brief Get typed pointer
     * @tparam T Target type
     * @return Typed pointer to memory
     */
    template<typename T>
    T* as() noexcept {
        static_assert(!std::is_void_v<T>, "Cannot convert to void type");
        return static_cast<T*>(data_);
    }

    /**
     * @brief Get const typed pointer
     * @tparam T Target type
     * @return Const typed pointer to memory
     */
    template<typename T>
    const T* as() const noexcept {
        static_assert(!std::is_void_v<T>, "Cannot convert to void type");
        return static_cast<const T*>(data_);
    }

    /**
     * @brief Get memory span
     * @tparam T Element type
     * @return Span over memory block
     */
    template<typename T = std::uint8_t>
    std::span<T> span() noexcept {
        static_assert(!std::is_void_v<T>, "Cannot create span of void");
        return std::span<T>(as<T>(), size_ / sizeof(T));
    }

    /**
     * @brief Get const memory span
     * @tparam T Element type
     * @return Const span over memory block
     */
    template<typename T = std::uint8_t>
    std::span<const T> span() const noexcept {
        static_assert(!std::is_void_v<T>, "Cannot create span of void");
        return std::span<const T>(as<T>(), size_ / sizeof(T));
    }

private:
    void* data_ = nullptr;
    std::size_t size_ = 0;
    memory_pool* pool_ = nullptr;
    std::function<void(void*)> deleter_;

    void cleanup();
};

/**
 * @brief Lock-free single-producer single-consumer ring buffer
 * 
 * High-performance circular buffer implementation optimized for
 * single producer and single consumer scenarios with zero-copy semantics.
 */
template<typename T>
class spsc_ring_buffer {
public:
    // Note: T should be move constructible and move assignable
    static_assert(std::is_move_constructible_v<T>, "T must be move constructible");
    static_assert(std::is_move_assignable_v<T>, "T must be move assignable");

    /**
     * @brief Constructor
     * @param capacity Ring buffer capacity (must be power of 2)
     */
    explicit spsc_ring_buffer(std::size_t capacity)
        : capacity_(next_power_of_two(capacity))
        , mask_(capacity_ - 1)
        , buffer_(std::make_unique<T[]>(capacity_))
        , head_(0)
        , tail_(0) {
        assert(is_power_of_two(capacity_));
    }

    /**
     * @brief Destructor
     */
    ~spsc_ring_buffer() = default;

    /**
     * @brief Copy constructor (deleted)
     */
    spsc_ring_buffer(const spsc_ring_buffer&) = delete;

    /**
     * @brief Copy assignment (deleted)
     */
    spsc_ring_buffer& operator=(const spsc_ring_buffer&) = delete;

    /**
     * @brief Move constructor
     */
    spsc_ring_buffer(spsc_ring_buffer&&) = default;

    /**
     * @brief Move assignment
     */
    spsc_ring_buffer& operator=(spsc_ring_buffer&&) = default;

    /**
     * @brief Push element to buffer (producer side)
     * @param item Item to push
     * @return true if successful, false if buffer full
     */
    bool push(const T& item) noexcept {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next_head = (head + 1) & mask_;
        
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false;  // Buffer full
        }
        
        buffer_[head] = item;
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    /**
     * @brief Push element to buffer with move semantics
     * @param item Item to push
     * @return true if successful, false if buffer full
     */
    bool push(T&& item) noexcept {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next_head = (head + 1) & mask_;
        
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false;  // Buffer full
        }
        
        buffer_[head] = std::move(item);
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    /**
     * @brief Pop element from buffer (consumer side)
     * @param item Reference to store popped item
     * @return true if successful, false if buffer empty
     */
    bool pop(T& item) noexcept {
        const auto tail = tail_.load(std::memory_order_relaxed);
        
        if (tail == head_.load(std::memory_order_acquire)) {
            return false;  // Buffer empty
        }
        
        item = std::move(buffer_[tail]);
        tail_.store((tail + 1) & mask_, std::memory_order_release);
        return true;
    }

    /**
     * @brief Peek at front element without removing it
     * @return Pointer to front element or nullptr if empty
     */
    const T* front() const noexcept {
        const auto tail = tail_.load(std::memory_order_relaxed);
        
        if (tail == head_.load(std::memory_order_acquire)) {
            return nullptr;  // Buffer empty
        }
        
        return &buffer_[tail];
    }

    /**
     * @brief Check if buffer is empty
     * @return true if empty, false otherwise
     */
    bool empty() const noexcept {
        return tail_.load(std::memory_order_relaxed) == head_.load(std::memory_order_acquire);
    }

    /**
     * @brief Check if buffer is full
     * @return true if full, false otherwise
     */
    bool full() const noexcept {
        const auto head = head_.load(std::memory_order_relaxed);
        return ((head + 1) & mask_) == tail_.load(std::memory_order_acquire);
    }

    /**
     * @brief Get current size (approximate)
     * @return Number of elements in buffer
     */
    std::size_t size() const noexcept {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto tail = tail_.load(std::memory_order_relaxed);
        return (head - tail) & mask_;
    }

    /**
     * @brief Get buffer capacity
     * @return Maximum number of elements
     */
    std::size_t capacity() const noexcept { return capacity_ - 1; }  // One slot reserved

    /**
     * @brief Clear all elements
     */
    void clear() noexcept {
        T item;
        while (pop(item)) {
            // Just consume all elements
        }
    }

private:
    const std::size_t capacity_;
    const std::size_t mask_;
    std::unique_ptr<T[]> buffer_;
    std::atomic<std::size_t> head_;
    std::atomic<std::size_t> tail_;

    static constexpr bool is_power_of_two(std::size_t n) noexcept {
        return n > 0 && (n & (n - 1)) == 0;
    }

    static constexpr std::size_t next_power_of_two(std::size_t n) noexcept {
        if (n <= 1) return 2;
        
        n--;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        if constexpr (sizeof(std::size_t) > 4) {
            n |= n >> 32;
        }
        return n + 1;
    }
};

/**
 * @brief Thread-safe message buffer with zero-copy semantics
 * 
 * Provides efficient buffering for network messages with support for
 * zero-copy operations, memory pooling, and flow control.
 */
class message_buffer {
public:
    /**
     * @brief Buffer configuration
     */
    struct config {
        /// Ring buffer capacity
        std::size_t buffer_capacity = 1024;
        /// Memory pool configuration
        memory_pool::config pool_config;
        /// Enable backpressure handling
        bool enable_backpressure = true;
        /// High water mark for backpressure (percentage of capacity)
        double high_water_mark = 0.8;
        /// Low water mark for backpressure (percentage of capacity)
        double low_water_mark = 0.6;
        /// Default wait timeout for blocking operations
        std::chrono::milliseconds default_timeout{1000};
    };

    /**
     * @brief Buffer statistics
     */
    struct stats {
        /// Total messages pushed
        std::uint64_t messages_pushed = 0;
        /// Total messages popped
        std::uint64_t messages_popped = 0;
        /// Total push failures (buffer full)
        std::uint64_t push_failures = 0;
        /// Total pop failures (buffer empty)
        std::uint64_t pop_failures = 0;
        /// Current buffer size
        std::uint64_t current_size = 0;
        /// Peak buffer size
        std::uint64_t peak_size = 0;
        /// Total bytes buffered
        std::uint64_t total_bytes_buffered = 0;
        /// Current backpressure state
        bool backpressure_active = false;
        /// Memory pool statistics
        memory_pool::stats pool_stats;
    };

    /**
     * @brief Buffered message with metadata
     */
    struct buffered_message {
        /// Message data block
        memory_block data;
        /// Message size
        std::size_t size = 0;
        /// Timestamp when message was buffered
        std::chrono::steady_clock::time_point timestamp;
        /// Message priority
        std::uint32_t priority = 0;

        /**
         * @brief Default constructor (for ring buffer)
         */
        buffered_message() = default;
        
        /**
         * @brief Constructor
         * @param block Memory block containing message
         * @param msg_size Message size
         */
        buffered_message(memory_block block, std::size_t msg_size)
            : data(std::move(block))
            , size(msg_size)
            , timestamp(std::chrono::steady_clock::now()) {}

        /**
         * @brief Move constructor
         */
        buffered_message(buffered_message&&) = default;

        /**
         * @brief Move assignment
         */
        buffered_message& operator=(buffered_message&&) = default;
    };

    /**
     * @brief Constructor with configuration
     * @param cfg Buffer configuration
     */
    explicit message_buffer(const config& cfg);
    
    /**
     * @brief Default constructor
     */
    message_buffer() : message_buffer(config{}) {}

    /**
     * @brief Destructor
     */
    ~message_buffer();

    /**
     * @brief Copy constructor (deleted)
     */
    message_buffer(const message_buffer&) = delete;

    /**
     * @brief Copy assignment (deleted)
     */
    message_buffer& operator=(const message_buffer&) = delete;

    /**
     * @brief Move constructor (deleted due to mutex)
     */
    message_buffer(message_buffer&&) = delete;

    /**
     * @brief Move assignment (deleted due to mutex)
     */
    message_buffer& operator=(message_buffer&&) = delete;

    /**
     * @brief Push message to buffer
     * @param data Message data
     * @param priority Message priority
     * @return Result indicating success or error
     */
    ss::core::result<void, buffer_error> 
    push(const std::vector<std::uint8_t>& data, std::uint32_t priority = 0);

    /**
     * @brief Push message to buffer with zero-copy
     * @param block Memory block containing message
     * @param size Message size within block
     * @param priority Message priority
     * @return Result indicating success or error
     */
    ss::core::result<void, buffer_error>
    push_zero_copy(memory_block block, std::size_t size, std::uint32_t priority = 0);

    /**
     * @brief Pop message from buffer
     * @return Buffered message or error
     */
    ss::core::result<buffered_message, buffer_error> pop();

    /**
     * @brief Pop message with timeout
     * @param timeout Maximum wait time
     * @return Buffered message or error
     */
    ss::core::result<buffered_message, buffer_error> 
    pop_timeout(std::chrono::milliseconds timeout);

    /**
     * @brief Try to push message without blocking
     * @param data Message data
     * @param priority Message priority
     * @return Result indicating success or error
     */
    ss::core::result<void, buffer_error>
    try_push(const std::vector<std::uint8_t>& data, std::uint32_t priority = 0);

    /**
     * @brief Try to pop message without blocking
     * @return Buffered message or error
     */
    ss::core::result<buffered_message, buffer_error> try_pop();

    /**
     * @brief Peek at front message without removing it
     * @return Pointer to front message or nullptr if empty
     */
    const buffered_message* front() const;

    /**
     * @brief Check if buffer is empty
     * @return true if empty, false otherwise
     */
    bool empty() const noexcept;

    /**
     * @brief Check if buffer is full
     * @return true if full, false otherwise
     */
    bool full() const noexcept;

    /**
     * @brief Get current buffer size
     * @return Number of messages in buffer
     */
    std::size_t size() const noexcept;

    /**
     * @brief Get buffer capacity
     * @return Maximum number of messages
     */
    std::size_t capacity() const noexcept;

    /**
     * @brief Check if backpressure is active
     * @return true if backpressure active, false otherwise
     */
    bool backpressure_active() const noexcept;

    /**
     * @brief Clear all buffered messages
     */
    void clear();

    /**
     * @brief Get current statistics
     * @return Buffer statistics snapshot
     */
    stats get_stats() const;

    /**
     * @brief Reset statistics
     */
    void reset_stats();

    /**
     * @brief Get memory pool reference
     * @return Reference to underlying memory pool
     */
    memory_pool& pool() { return pool_; }

    /**
     * @brief Get const memory pool reference
     * @return Const reference to underlying memory pool
     */
    const memory_pool& pool() const { return pool_; }

private:
    config config_;
    memory_pool pool_;
    spsc_ring_buffer<buffered_message> ring_buffer_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_cv_;
    std::condition_variable not_full_cv_;
    mutable stats stats_;
    std::atomic<bool> backpressure_active_;

    void update_backpressure();
    bool check_high_water_mark() const;
    bool check_low_water_mark() const;
};

} // namespace ss::network