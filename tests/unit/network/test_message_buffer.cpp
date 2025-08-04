#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ss_p2p/network/message_buffer.hpp"

#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include <random>

namespace ss::network::test {

/**
 * @brief Test fixture for memory pool tests
 */
class MemoryPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        memory_pool::config config;
        config.block_size = 1024;
        config.initial_blocks = 10;
        config.max_blocks = 100;
        pool_ = std::make_unique<memory_pool>(config);
    }

    void TearDown() override {
        pool_.reset();
    }

    std::unique_ptr<memory_pool> pool_;
};

TEST_F(MemoryPoolTest, BasicAllocationDeallocation) {
    // Test basic allocation
    void* ptr = pool_->allocate();
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(pool_->owns(ptr));
    
    // Test deallocation
    pool_->deallocate(ptr);
    
    // Test multiple allocations
    std::vector<void*> ptrs;
    for (int i = 0; i < 5; ++i) {
        void* p = pool_->allocate();
        ASSERT_NE(p, nullptr);
        ptrs.push_back(p);
    }
    
    // Deallocate all
    for (void* p : ptrs) {
        pool_->deallocate(p);
    }
}

TEST_F(MemoryPoolTest, Statistics) {
    auto stats = pool_->get_stats();
    EXPECT_EQ(stats.blocks_in_use, 0);
    EXPECT_GT(stats.blocks_available, 0);
    
    // Allocate some blocks
    std::vector<void*> ptrs;
    for (int i = 0; i < 3; ++i) {
        ptrs.push_back(pool_->allocate());
    }
    
    stats = pool_->get_stats();
    EXPECT_EQ(stats.blocks_in_use, 3);
    EXPECT_EQ(stats.total_allocated, 3);
    
    // Deallocate
    for (void* ptr : ptrs) {
        pool_->deallocate(ptr);
    }
    
    stats = pool_->get_stats();
    EXPECT_EQ(stats.blocks_in_use, 0);
    EXPECT_EQ(stats.total_deallocated, 3);
}

TEST_F(MemoryPoolTest, BlockProperties) {
    EXPECT_EQ(pool_->block_size(), 1024);
    EXPECT_GT(pool_->alignment(), 0);
}

TEST_F(MemoryPoolTest, OwnershipCheck) {
    void* pool_ptr = pool_->allocate();
    ASSERT_NE(pool_ptr, nullptr);
    EXPECT_TRUE(pool_->owns(pool_ptr));
    
    // External pointer should not be owned
    int external_var;
    EXPECT_FALSE(pool_->owns(&external_var));
    
    pool_->deallocate(pool_ptr);
}

/**
 * @brief Test fixture for memory block tests
 */
class MemoryBlockTest : public ::testing::Test {
protected:
    void SetUp() override {
        memory_pool::config config;
        config.block_size = 1024;
        pool_ = std::make_unique<memory_pool>(config);
    }

    std::unique_ptr<memory_pool> pool_;
};

TEST_F(MemoryBlockTest, PoolAllocation) {
    memory_block block(*pool_);
    
    EXPECT_TRUE(block.is_valid());
    EXPECT_NE(block.data(), nullptr);
    EXPECT_EQ(block.size(), pool_->block_size());
}

TEST_F(MemoryBlockTest, ExternalMemory) {
    std::vector<std::uint8_t> data(256, 0x42);
    bool deleter_called = false;
    
    {
        memory_block block(data.data(), data.size(), 
            [&deleter_called](void*) { deleter_called = true; });
        
        EXPECT_TRUE(block.is_valid());
        EXPECT_EQ(block.data(), data.data());
        EXPECT_EQ(block.size(), data.size());
    } // block destroyed here
    
    EXPECT_TRUE(deleter_called);
}

TEST_F(MemoryBlockTest, MoveSemantics) {
    memory_block block1(*pool_);
    void* original_data = block1.data();
    std::size_t original_size = block1.size();
    
    // Move construction
    memory_block block2(std::move(block1));
    
    EXPECT_FALSE(block1.is_valid());
    EXPECT_TRUE(block2.is_valid());
    EXPECT_EQ(block2.data(), original_data);
    EXPECT_EQ(block2.size(), original_size);
    
    // Move assignment
    memory_block block3;
    EXPECT_FALSE(block3.is_valid());
    
    block3 = std::move(block2);
    EXPECT_FALSE(block2.is_valid());
    EXPECT_TRUE(block3.is_valid());
    EXPECT_EQ(block3.data(), original_data);
    EXPECT_EQ(block3.size(), original_size);
}

TEST_F(MemoryBlockTest, TypedAccess) {
    memory_block block(*pool_);
    
    // Test as<T>()
    auto* uint8_ptr = block.as<std::uint8_t>();
    auto* uint32_ptr = block.as<std::uint32_t>();
    
    EXPECT_NE(uint8_ptr, nullptr);
    EXPECT_NE(uint32_ptr, nullptr);
    EXPECT_EQ(static_cast<void*>(uint8_ptr), block.data());
    EXPECT_EQ(static_cast<void*>(uint32_ptr), block.data());
}

TEST_F(MemoryBlockTest, SpanAccess) {
    memory_block block(*pool_);
    
    auto span = block.span<std::uint8_t>();
    EXPECT_EQ(span.data(), block.as<std::uint8_t>());
    EXPECT_EQ(span.size(), block.size());
    
    auto const_span = const_cast<const memory_block&>(block).span<std::uint8_t>();
    EXPECT_EQ(const_span.data(), block.as<std::uint8_t>());
    EXPECT_EQ(const_span.size(), block.size());
}

/**
 * @brief Test fixture for SPSC ring buffer tests
 */
class SPSCRingBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        buffer_ = std::make_unique<spsc_ring_buffer<int>>(8);  // Power of 2
    }

    std::unique_ptr<spsc_ring_buffer<int>> buffer_;
};

TEST_F(SPSCRingBufferTest, BasicOperations) {
    EXPECT_TRUE(buffer_->empty());
    EXPECT_FALSE(buffer_->full());
    EXPECT_EQ(buffer_->size(), 0);
    EXPECT_EQ(buffer_->capacity(), 7);  // One slot reserved
    
    // Push elements
    EXPECT_TRUE(buffer_->push(1));
    EXPECT_TRUE(buffer_->push(2));
    EXPECT_TRUE(buffer_->push(3));
    
    EXPECT_FALSE(buffer_->empty());
    EXPECT_EQ(buffer_->size(), 3);
    
    // Pop elements
    int value;
    EXPECT_TRUE(buffer_->pop(value));
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(buffer_->pop(value));
    EXPECT_EQ(value, 2);
    EXPECT_TRUE(buffer_->pop(value));
    EXPECT_EQ(value, 3);
    
    EXPECT_TRUE(buffer_->empty());
    EXPECT_FALSE(buffer_->pop(value));
}

TEST_F(SPSCRingBufferTest, FillAndEmpty) {
    // Fill buffer to capacity
    for (int i = 0; i < static_cast<int>(buffer_->capacity()); ++i) {
        EXPECT_TRUE(buffer_->push(i)) << "Failed to push " << i;
    }
    
    EXPECT_TRUE(buffer_->full());
    EXPECT_FALSE(buffer_->push(999));  // Should fail when full
    
    // Empty buffer
    int value;
    for (int i = 0; i < static_cast<int>(buffer_->capacity()); ++i) {
        EXPECT_TRUE(buffer_->pop(value)) << "Failed to pop at " << i;
        EXPECT_EQ(value, i);
    }
    
    EXPECT_TRUE(buffer_->empty());
    EXPECT_FALSE(buffer_->pop(value));  // Should fail when empty
}

TEST_F(SPSCRingBufferTest, MoveSemantics) {
    // Test move in push
    std::string str = "test";
    EXPECT_TRUE(buffer_->push(42));
    
    // Test front/peek
    const int* front_ptr = buffer_->front();
    ASSERT_NE(front_ptr, nullptr);
    EXPECT_EQ(*front_ptr, 42);
}

TEST_F(SPSCRingBufferTest, Clear) {
    // Add some elements
    buffer_->push(1);
    buffer_->push(2);
    buffer_->push(3);
    
    EXPECT_EQ(buffer_->size(), 3);
    
    // Clear buffer
    buffer_->clear();
    
    EXPECT_TRUE(buffer_->empty());
    EXPECT_EQ(buffer_->size(), 0);
}

/**
 * @brief Test fixture for message buffer tests
 */
class MessageBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        message_buffer::config config;
        config.buffer_capacity = 16;
        config.pool_config.block_size = 1024;
        config.pool_config.initial_blocks = 10;
        buffer_ = std::make_unique<message_buffer>(config);
    }

    std::unique_ptr<message_buffer> buffer_;
};

TEST_F(MessageBufferTest, BasicPushPop) {
    std::vector<std::uint8_t> data = {1, 2, 3, 4, 5};
    
    // Test push
    auto result = buffer_->push(data);
    ASSERT_TRUE(result.is_ok());
    
    EXPECT_FALSE(buffer_->empty());
    EXPECT_EQ(buffer_->size(), 1);
    
    // Test pop
    auto pop_result = buffer_->pop();
    ASSERT_TRUE(pop_result.is_ok());
    
    auto message = std::move(pop_result.value());
    EXPECT_EQ(message.size, data.size());
    
    // Verify data
    auto span = message.data.span<std::uint8_t>();
    std::vector<std::uint8_t> received_data(span.begin(), span.begin() + message.size);
    EXPECT_EQ(received_data, data);
    
    EXPECT_TRUE(buffer_->empty());
}

TEST_F(MessageBufferTest, ZeroCopyPushPop) {
    auto& pool = buffer_->pool();
    memory_block block(pool);
    
    // Fill block with test data
    std::vector<std::uint8_t> test_data = {0xAA, 0xBB, 0xCC, 0xDD};
    std::memcpy(block.data(), test_data.data(), test_data.size());
    
    // Test zero-copy push
    auto result = buffer_->push_zero_copy(std::move(block), test_data.size());
    ASSERT_TRUE(result.is_ok());
    
    // Test pop
    auto pop_result = buffer_->pop();
    ASSERT_TRUE(pop_result.is_ok());
    
    auto message = std::move(pop_result.value());
    EXPECT_EQ(message.size, test_data.size());
    
    // Verify data
    auto span = message.data.span<std::uint8_t>();
    for (std::size_t i = 0; i < test_data.size(); ++i) {
        EXPECT_EQ(span[i], test_data[i]);
    }
}

TEST_F(MessageBufferTest, TryOperations) {
    std::vector<std::uint8_t> data = {1, 2, 3};
    
    // Try push (should succeed)
    auto push_result = buffer_->try_push(data);
    EXPECT_TRUE(push_result.is_ok());
    
    // Try pop (should succeed)
    auto pop_result = buffer_->try_pop();
    EXPECT_TRUE(pop_result.is_ok());
    
    // Try pop on empty buffer (should fail)
    auto empty_pop_result = buffer_->try_pop();
    EXPECT_FALSE(empty_pop_result.is_ok());
    EXPECT_EQ(empty_pop_result.error(), buffer_error::buffer_empty);
}

TEST_F(MessageBufferTest, TimeoutOperations) {
    // Test pop with timeout on empty buffer
    auto timeout_result = buffer_->pop_timeout(std::chrono::milliseconds{50});
    EXPECT_FALSE(timeout_result.is_ok());
    EXPECT_EQ(timeout_result.error(), buffer_error::timeout);
    
    // Push a message and test successful timeout pop
    std::vector<std::uint8_t> data = {1, 2, 3};
    buffer_->push(data);
    
    auto success_result = buffer_->pop_timeout(std::chrono::milliseconds{50});
    EXPECT_TRUE(success_result.is_ok());
}

TEST_F(MessageBufferTest, BufferCapacity) {
    std::vector<std::uint8_t> data = {1, 2, 3, 4, 5};
    
    // Fill buffer to capacity
    std::size_t pushed = 0;
    while (pushed < buffer_->capacity()) {
        auto result = buffer_->try_push(data);
        if (result.is_ok()) {
            pushed++;
        } else {
            break;
        }
    }
    
    EXPECT_TRUE(buffer_->full());
    
    // Try to push one more (should fail)
    auto overflow_result = buffer_->try_push(data);
    EXPECT_FALSE(overflow_result.is_ok());
    EXPECT_EQ(overflow_result.error(), buffer_error::buffer_full);
}

TEST_F(MessageBufferTest, MessagePriority) {
    std::vector<std::uint8_t> data1 = {1};
    std::vector<std::uint8_t> data2 = {2};
    std::vector<std::uint8_t> data3 = {3};
    
    // Push messages with different priorities
    buffer_->push(data1, 1);  // Low priority
    buffer_->push(data2, 3);  // High priority
    buffer_->push(data3, 2);  // Medium priority
    
    // Messages should be returned in FIFO order regardless of priority
    // (unless buffer implements priority queue, which this simple version doesn't)
    auto msg1 = buffer_->pop();
    ASSERT_TRUE(msg1.is_ok());
    EXPECT_EQ(msg1.value().priority, 1);
    
    auto msg2 = buffer_->pop();
    ASSERT_TRUE(msg2.is_ok());
    EXPECT_EQ(msg2.value().priority, 3);
    
    auto msg3 = buffer_->pop();
    ASSERT_TRUE(msg3.is_ok());
    EXPECT_EQ(msg3.value().priority, 2);
}

TEST_F(MessageBufferTest, Statistics) {
    auto stats = buffer_->get_stats();
    
    // Initial stats
    EXPECT_EQ(stats.messages_pushed, 0);
    EXPECT_EQ(stats.messages_popped, 0);
    EXPECT_EQ(stats.current_size, 0);
    
    // Push some messages
    std::vector<std::uint8_t> data = {1, 2, 3, 4, 5};
    buffer_->push(data);
    buffer_->push(data);
    
    stats = buffer_->get_stats();
    EXPECT_EQ(stats.messages_pushed, 2);
    EXPECT_EQ(stats.current_size, 2);
    EXPECT_GT(stats.total_bytes_buffered, 0);
    
    // Pop a message
    buffer_->pop();
    
    stats = buffer_->get_stats();
    EXPECT_EQ(stats.messages_popped, 1);
    EXPECT_EQ(stats.current_size, 1);
    
    // Reset stats
    buffer_->reset_stats();
    stats = buffer_->get_stats();
    EXPECT_EQ(stats.messages_pushed, 0);
    EXPECT_EQ(stats.messages_popped, 0);
}

TEST_F(MessageBufferTest, BackpressureHandling) {
    // This test assumes the buffer has backpressure support
    // Fill buffer beyond high water mark to trigger backpressure
    std::vector<std::uint8_t> data(100, 0x42);  // Large message
    
    bool backpressure_triggered = false;
    std::size_t push_count = 0;
    
    // Keep pushing until backpressure is active or buffer is full
    while (push_count < buffer_->capacity() && !buffer_->backpressure_active()) {
        auto result = buffer_->try_push(data);
        if (result.is_ok()) {
            push_count++;
        } else {
            break;
        }
    }
    
    // If backpressure is implemented, it might be active now
    // This depends on the specific implementation
}

TEST_F(MessageBufferTest, Clear) {
    std::vector<std::uint8_t> data = {1, 2, 3, 4, 5};
    
    // Add some messages
    buffer_->push(data);
    buffer_->push(data);
    buffer_->push(data);
    
    EXPECT_EQ(buffer_->size(), 3);
    EXPECT_FALSE(buffer_->empty());
    
    // Clear buffer
    buffer_->clear();
    
    EXPECT_EQ(buffer_->size(), 0);
    EXPECT_TRUE(buffer_->empty());
}

// Multi-threaded tests

TEST_F(MessageBufferTest, ConcurrentPushPop) {
    const int num_messages = 1000;
    std::atomic<int> messages_sent{0};
    std::atomic<int> messages_received{0};
    
    // Producer thread
    std::thread producer([this, num_messages, &messages_sent]() {
        for (int i = 0; i < num_messages; ++i) {
            std::vector<std::uint8_t> data = {
                static_cast<std::uint8_t>(i & 0xFF),
                static_cast<std::uint8_t>((i >> 8) & 0xFF),
                static_cast<std::uint8_t>((i >> 16) & 0xFF),
                static_cast<std::uint8_t>((i >> 24) & 0xFF)
            };
            
            // Keep trying until successful
            while (true) {
                auto result = buffer_->try_push(data);
                if (result.is_ok()) {
                    messages_sent.fetch_add(1);
                    break;
                }
                std::this_thread::sleep_for(std::chrono::microseconds{1});
            }
        }
    });
    
    // Consumer thread
    std::thread consumer([this, num_messages, &messages_received]() {
        for (int i = 0; i < num_messages; ++i) {
            // Keep trying until successful
            while (true) {
                auto result = buffer_->try_pop();
                if (result.is_ok()) {
                    messages_received.fetch_add(1);
                    break;
                }
                std::this_thread::sleep_for(std::chrono::microseconds{1});
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    EXPECT_EQ(messages_sent.load(), num_messages);
    EXPECT_EQ(messages_received.load(), num_messages);
    EXPECT_TRUE(buffer_->empty());
}

// Error handling tests

TEST_F(MessageBufferTest, ErrorStringConversion) {
    EXPECT_NE(to_string(buffer_error::none), "");
    EXPECT_NE(to_string(buffer_error::buffer_full), "");
    EXPECT_NE(to_string(buffer_error::buffer_empty), "");
    EXPECT_NE(to_string(buffer_error::timeout), "");
    EXPECT_NE(to_string(buffer_error::invalid_size), "");
}

} // namespace ss::network::test