#include "../../../include/ss_p2p/network/message_buffer.hpp"
#include "../../../include/logger/logger.hpp"

#include <algorithm>
#include <cstring>
#include <new>

namespace ss::network {

// memory_pool implementation

memory_pool::memory_pool(const config& cfg) : config_(cfg) {
    try {
        expand_pool();
        // LOG_INFO("Memory pool created with {} initial blocks of {} bytes each", 
        //          config_.initial_blocks, config_.block_size);
    } catch (const std::exception& e) {
        // LOG_ERROR("Failed to create memory pool: {}", e.what());
        throw;
    }
}

memory_pool::~memory_pool() {
    std::lock_guard<std::mutex> lock(mutex_);
    allocated_chunks_.clear();
    available_blocks_.clear();
    // LOG_DEBUG("Memory pool destroyed");
}

void* memory_pool::allocate() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (available_blocks_.empty()) {
        if (allocated_chunks_.size() >= config_.max_blocks) {
            if (config_.enable_stats) {
                stats_.allocation_failures++;
            }
            return nullptr;
        }
        
        try {
            expand_pool();
        } catch (...) {
            if (config_.enable_stats) {
                stats_.allocation_failures++;
            }
            return nullptr;
        }
    }
    
    void* ptr = available_blocks_.back();
    available_blocks_.pop_back();
    
    if (config_.enable_stats) {
        stats_.total_allocated++;
        stats_.blocks_in_use++;
        stats_.blocks_available = available_blocks_.size();
        stats_.peak_blocks_in_use = std::max(stats_.peak_blocks_in_use, stats_.blocks_in_use);
        stats_.total_bytes_allocated += config_.block_size;
    }
    
    return ptr;
}

void memory_pool::deallocate(void* ptr) noexcept {
    if (!ptr || !owns(ptr)) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    available_blocks_.push_back(ptr);
    
    if (config_.enable_stats) {
        stats_.total_deallocated++;
        stats_.blocks_in_use--;
        stats_.blocks_available = available_blocks_.size();
    }
}

memory_pool::stats memory_pool::get_stats() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto current_stats = stats_;
    current_stats.blocks_available = available_blocks_.size();
    return current_stats;
}

void memory_pool::reset_stats() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = stats{};
}

bool memory_pool::owns(void* ptr) const noexcept {
    return is_pool_pointer(ptr);
}

void memory_pool::expand_pool() {
    auto chunk_size = config_.initial_blocks * config_.block_size;
    auto chunk = std::make_unique<std::uint8_t[]>(chunk_size);
    
    // Align blocks within the chunk
    std::uint8_t* aligned_start = chunk.get();
    if (reinterpret_cast<std::uintptr_t>(aligned_start) % config_.alignment != 0) {
        auto offset = config_.alignment - (reinterpret_cast<std::uintptr_t>(aligned_start) % config_.alignment);
        aligned_start += offset;
    }
    
    // Add aligned blocks to available list
    for (std::size_t i = 0; i < config_.initial_blocks; ++i) {
        std::uint8_t* block_ptr = aligned_start + (i * config_.block_size);
        available_blocks_.push_back(block_ptr);
    }
    
    allocated_chunks_.push_back(std::move(chunk));
    // LOG_DEBUG("Expanded memory pool with {} new blocks", config_.initial_blocks);
}

bool memory_pool::is_pool_pointer(void* ptr) const noexcept {
    if (!ptr) return false;
    
    for (const auto& chunk : allocated_chunks_) {
        std::uint8_t* chunk_start = chunk.get();
        std::uint8_t* chunk_end = chunk_start + (config_.initial_blocks * config_.block_size);
        
        if (ptr >= chunk_start && ptr < chunk_end) {
            return true;
        }
    }
    
    return false;
}

// memory_block implementation

memory_block::memory_block(memory_pool& pool) 
    : data_(pool.allocate())
    , size_(pool.block_size())
    , pool_(&pool) {
    
    if (!data_) {
        throw std::bad_alloc{};
    }
}

memory_block::memory_block(void* data, std::size_t size, std::function<void(void*)> deleter)
    : data_(data)
    , size_(size)
    , pool_(nullptr)
    , deleter_(std::move(deleter)) {
}

memory_block::memory_block(memory_block&& other) noexcept
    : data_(other.data_)
    , size_(other.size_)
    , pool_(other.pool_)
    , deleter_(std::move(other.deleter_)) {
    
    other.data_ = nullptr;
    other.size_ = 0;
    other.pool_ = nullptr;
}

memory_block& memory_block::operator=(memory_block&& other) noexcept {
    if (this != &other) {
        cleanup();
        
        data_ = other.data_;
        size_ = other.size_;
        pool_ = other.pool_;
        deleter_ = std::move(other.deleter_);
        
        other.data_ = nullptr;
        other.size_ = 0;
        other.pool_ = nullptr;
    }
    return *this;
}

memory_block::~memory_block() {
    cleanup();
}

void memory_block::reset() noexcept {
    cleanup();
    data_ = nullptr;
    size_ = 0;
    pool_ = nullptr;
    deleter_ = nullptr;
}

void memory_block::cleanup() {
    if (data_) {
        if (pool_) {
            pool_->deallocate(data_);
        } else if (deleter_) {
            deleter_(data_);
        }
    }
}

// message_buffer implementation

message_buffer::message_buffer(const config& cfg) 
    : config_(cfg)
    , pool_(cfg.pool_config)
    , ring_buffer_(cfg.buffer_capacity)
    , backpressure_active_(false) {
    
    // TODO: Add logging
    // // LOG_INFO("Message buffer created with capacity {} and pool block size {}", 
    //          cfg.buffer_capacity, cfg.pool_config.block_size);
}

message_buffer::~message_buffer() {
    clear();
    // LOG_DEBUG("Message buffer destroyed");
}

ss::core::result<void, buffer_error> 
message_buffer::push(const std::vector<std::uint8_t>& data, std::uint32_t priority) {
    if (data.empty()) {
        return ss::core::result<void, buffer_error>::err(buffer_error::invalid_size);
    }
    
    try {
        // Allocate memory block
        memory_block block(pool_);
        if (!block.is_valid()) {
            return ss::core::result<void, buffer_error>::err(buffer_error::out_of_memory);
        }
        
        // Check if data fits in block
        if (data.size() > block.size()) {
            return ss::core::result<void, buffer_error>::err(buffer_error::invalid_size);
        }
        
        // Copy data to block
        std::memcpy(block.data(), data.data(), data.size());
        
        return push_zero_copy(std::move(block), data.size(), priority);
        
    } catch (const std::exception& e) {
        // LOG_ERROR("Failed to push message: {}", e.what());
        return ss::core::result<void, buffer_error>::err(buffer_error::unknown);
    }
}

ss::core::result<void, buffer_error>
message_buffer::push_zero_copy(memory_block block, std::size_t size, std::uint32_t priority) {
    if (!block.is_valid() || size == 0 || size > block.size()) {
        return ss::core::result<void, buffer_error>::err(buffer_error::invalid_size);
    }
    
    std::unique_lock<std::mutex> lock(mutex_);
    
    // Check if buffer is full
    if (ring_buffer_.full()) {
        if (!config_.enable_backpressure) {
            stats_.push_failures++;
            return ss::core::result<void, buffer_error>::err(buffer_error::buffer_full);
        }
        
        // Wait for space to become available
        auto result = not_full_cv_.wait_for(lock, config_.default_timeout, 
            [this] { return !ring_buffer_.full(); });
        
        if (!result) {
            stats_.push_failures++;
            return ss::core::result<void, buffer_error>::err(buffer_error::timeout);
        }
    }
    
    // Create buffered message
    buffered_message msg(std::move(block), size);
    msg.priority = priority;
    
    // Add to ring buffer
    if (!ring_buffer_.push(std::move(msg))) {
        stats_.push_failures++;
        return ss::core::result<void, buffer_error>::err(buffer_error::buffer_full);
    }
    
    // Update statistics
    stats_.messages_pushed++;
    stats_.current_size = ring_buffer_.size();
    stats_.peak_size = std::max(stats_.peak_size, stats_.current_size);
    stats_.total_bytes_buffered += size;
    
    // Update backpressure state
    update_backpressure();
    
    // Notify waiting consumers
    not_empty_cv_.notify_one();
    
    return ss::core::result<void, buffer_error>::ok();
}

ss::core::result<message_buffer::buffered_message, buffer_error> message_buffer::pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (ring_buffer_.empty()) {
        stats_.pop_failures++;
        return ss::core::result<buffered_message, buffer_error>::err(buffer_error::buffer_empty);
    }
    
    buffered_message msg(memory_block{}, 0);
    if (!ring_buffer_.pop(msg)) {
        stats_.pop_failures++;
        return ss::core::result<buffered_message, buffer_error>::err(buffer_error::buffer_empty);
    }
    
    // Update statistics
    stats_.messages_popped++;
    stats_.current_size = ring_buffer_.size();
    
    // Update backpressure state
    update_backpressure();
    
    // Notify waiting producers
    not_full_cv_.notify_one();
    
    return ss::core::result<buffered_message, buffer_error>::ok(std::move(msg));
}

ss::core::result<message_buffer::buffered_message, buffer_error> 
message_buffer::pop_timeout(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // Wait for message to become available
    auto result = not_empty_cv_.wait_for(lock, timeout, 
        [this] { return !ring_buffer_.empty(); });
    
    if (!result) {
        return ss::core::result<buffered_message, buffer_error>::err(buffer_error::timeout);
    }
    
    return pop();
}

ss::core::result<void, buffer_error>
message_buffer::try_push(const std::vector<std::uint8_t>& data, std::uint32_t priority) {
    if (data.empty()) {
        return ss::core::result<void, buffer_error>::err(buffer_error::invalid_size);
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (ring_buffer_.full()) {
        stats_.push_failures++;
        return ss::core::result<void, buffer_error>::err(buffer_error::buffer_full);
    }
    
    // Unlock and use regular push
    return push(data, priority);
}

ss::core::result<message_buffer::buffered_message, buffer_error> message_buffer::try_pop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (ring_buffer_.empty()) {
        stats_.pop_failures++;
        return ss::core::result<buffered_message, buffer_error>::err(buffer_error::buffer_empty);
    }
    
    return pop();
}

const message_buffer::buffered_message* message_buffer::front() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ring_buffer_.front();
}

bool message_buffer::empty() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return ring_buffer_.empty();
}

bool message_buffer::full() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return ring_buffer_.full();
}

std::size_t message_buffer::size() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return ring_buffer_.size();
}

std::size_t message_buffer::capacity() const noexcept {
    return ring_buffer_.capacity();
}

bool message_buffer::backpressure_active() const noexcept {
    return backpressure_active_.load();
}

void message_buffer::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    ring_buffer_.clear();
    stats_.current_size = 0;
    backpressure_active_.store(false);
    
    // Notify all waiting threads
    not_empty_cv_.notify_all();
    not_full_cv_.notify_all();
}

message_buffer::stats message_buffer::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto current_stats = stats_;
    current_stats.current_size = ring_buffer_.size();
    current_stats.backpressure_active = backpressure_active_.load();
    current_stats.pool_stats = pool_.get_stats();
    return current_stats;
}

void message_buffer::reset_stats() {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = stats{};
    pool_.reset_stats();
}

void message_buffer::update_backpressure() {
    if (!config_.enable_backpressure) {
        return;
    }
    
    auto current_size = ring_buffer_.size();
    auto capacity = ring_buffer_.capacity();
    double utilization = static_cast<double>(current_size) / capacity;
    
    if (utilization >= config_.high_water_mark && !backpressure_active_.load()) {
        backpressure_active_.store(true);
        // LOG_DEBUG("Backpressure activated at {}% utilization", utilization * 100);
    } else if (utilization <= config_.low_water_mark && backpressure_active_.load()) {
        backpressure_active_.store(false);
        // LOG_DEBUG("Backpressure deactivated at {}% utilization", utilization * 100);
    }
}

bool message_buffer::check_high_water_mark() const {
    auto current_size = ring_buffer_.size();
    auto capacity = ring_buffer_.capacity();
    double utilization = static_cast<double>(current_size) / capacity;
    return utilization >= config_.high_water_mark;
}

bool message_buffer::check_low_water_mark() const {
    auto current_size = ring_buffer_.size();
    auto capacity = ring_buffer_.capacity();
    double utilization = static_cast<double>(current_size) / capacity;
    return utilization <= config_.low_water_mark;
}

} // namespace ss::network