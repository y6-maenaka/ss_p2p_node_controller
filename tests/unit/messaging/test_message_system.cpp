#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <future>
#include <memory>

#include <boost/asio.hpp>
#include <ss_p2p/message.hpp>
#include <ss_p2p/message_pool.hpp>

namespace ss::test {

// Mock peer class for testing
class mock_peer {
public:
    using id_type = std::string;
    
    explicit mock_peer(endpoint_t endpoint) : endpoint_(endpoint) {}
    
    endpoint_t get_endpoint() const { return endpoint_; }
    id_type get_id() const { return endpoint_.address().to_string() + ":" + std::to_string(endpoint_.port()); }
    
private:
    endpoint_t endpoint_;
};

using mock_peer_ptr = std::shared_ptr<mock_peer>;

// Test fixture for message system tests
class MessageSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        peer_resolver_ = [](const endpoint_t& ep) -> peer_ptr {
            // Create mock peer - need to cast to peer_ptr for interface compatibility
            auto mock = std::make_shared<mock_peer>(ep);
            return std::reinterpret_pointer_cast<peer>(mock);
        };
        
        pool_ = std::make_unique<message_pool>(io_context_, peer_resolver_);
        pool_->start();
    }
    
    void TearDown() override {
        if (pool_) {
            pool_->stop();
        }
        io_context_.stop();
    }
    
    boost::asio::io_context io_context_;
    endpoint_to_peer_func_t peer_resolver_;
    std::unique_ptr<message_pool> pool_;
    
    // Helper function to create test endpoint
    endpoint_t create_test_endpoint(const std::string& ip, uint16_t port) {
        return endpoint_t(boost::asio::ip::address::from_string(ip), port);
    }
};

// Message header tests
TEST(MessageHeaderTest, SerializationRoundTrip) {
    message_header original;
    original.magic_number = 0x53535032;
    original.version = message_version::current;
    original.type = message_type::ping;
    original.priority = message_priority::high;
    original.format = serialization_format::binary;
    original.payload_size = 1024;
    original.checksum = 0x12345678;
    original.sequence_id = 42;
    original.timestamp = std::chrono::steady_clock::now();
    
    auto serialized = original.serialize();
    EXPECT_EQ(serialized.size(), message_header::wire_size);
    
    auto deserialized = message_header::deserialize(serialized);
    ASSERT_TRUE(deserialized.has_value());
    
    const auto& header = *deserialized;
    EXPECT_EQ(header.magic_number, original.magic_number);
    EXPECT_EQ(header.version, original.version);
    EXPECT_EQ(header.type, original.type);
    EXPECT_EQ(header.priority, original.priority);
    EXPECT_EQ(header.format, original.format);
    EXPECT_EQ(header.payload_size, original.payload_size);
    EXPECT_EQ(header.checksum, original.checksum);
    EXPECT_EQ(header.sequence_id, original.sequence_id);
}

TEST(MessageHeaderTest, ValidationTests) {
    message_header header;
    header.magic_number = 0x53535032;
    header.version = message_version::current;
    header.type = message_type::ping;
    header.priority = message_priority::normal;
    header.format = serialization_format::binary;
    header.payload_size = 100;
    
    EXPECT_TRUE(header.is_valid());
    
    // Test invalid magic number
    header.magic_number = 0x12345678;
    EXPECT_FALSE(header.is_valid());
    header.magic_number = 0x53535032;
    
    // Test invalid payload size
    header.payload_size = constants::max_message_size + 1;
    EXPECT_FALSE(header.is_valid());
}

// Message payload tests
TEST(MessagePayloadTest, TypeSafeAccessors) {
    message_payload payload;
    
    // Test string payload
    payload.set(std::string("test string"));
    auto str_result = payload.get<std::string>();
    ASSERT_TRUE(str_result.has_value());
    EXPECT_EQ(*str_result, "test string");
    
    // Test JSON payload
    nlohmann::json json_data = {{"key", "value"}, {"number", 42}};
    payload.set(json_data);
    auto json_result = payload.get<nlohmann::json>();
    ASSERT_TRUE(json_result.has_value());
    EXPECT_EQ((*json_result)["key"], "value");
    EXPECT_EQ((*json_result)["number"], 42);
    
    // Test binary payload
    std::vector<uint8_t> binary_data = {0x01, 0x02, 0x03, 0x04};
    payload.set(binary_data);
    auto binary_result = payload.get<std::vector<uint8_t>>();
    ASSERT_TRUE(binary_result.has_value());
    EXPECT_EQ(*binary_result, binary_data);
}

TEST(MessagePayloadTest, SerializationFormats) {
    nlohmann::json test_data = {
        {"message", "Hello, World!"},
        {"timestamp", 1234567890},
        {"data", {1, 2, 3, 4, 5}}
    };
    
    message_payload payload(test_data);
    
    // Test JSON serialization
    auto json_serialized = payload.serialize(serialization_format::json);
    auto json_deserialized = message_payload::deserialize(json_serialized, serialization_format::json);
    ASSERT_TRUE(json_deserialized.has_value());
    
    auto recovered_json = json_deserialized->get<nlohmann::json>();
    ASSERT_TRUE(recovered_json.has_value());
    EXPECT_EQ(*recovered_json, test_data);
    
    // Test binary serialization
    auto binary_serialized = payload.serialize(serialization_format::binary);
    auto binary_deserialized = message_payload::deserialize(binary_serialized, serialization_format::binary);
    ASSERT_TRUE(binary_deserialized.has_value());
    
    auto recovered_binary = binary_deserialized->get<nlohmann::json>();
    ASSERT_TRUE(recovered_binary.has_value());
    EXPECT_EQ(*recovered_binary, test_data);
}

// Message tests
TEST(MessageTest, BasicConstruction) {
    message msg(message_type::ping, message_priority::high);
    
    EXPECT_EQ(msg.type(), message_type::ping);
    EXPECT_EQ(msg.priority(), message_priority::high);
    EXPECT_GT(msg.id(), 0);
    EXPECT_TRUE(msg.is_valid());
}

TEST(MessageTest, PayloadMessage) {
    nlohmann::json data = {{"target", "node123"}, {"hops", 3}};
    message_payload payload(data);
    
    message msg(message_type::find_node, std::move(payload), message_priority::normal);
    
    EXPECT_EQ(msg.type(), message_type::find_node);
    EXPECT_EQ(msg.priority(), message_priority::normal);
    
    auto recovered_json = msg.payload().get<nlohmann::json>();
    ASSERT_TRUE(recovered_json.has_value());
    EXPECT_EQ((*recovered_json)["target"], "node123");
    EXPECT_EQ((*recovered_json)["hops"], 3);
}

TEST(MessageTest, SerializationRoundTrip) {
    nlohmann::json test_data = {{"test", "data"}, {"value", 42}};
    message_payload payload(test_data);
    message original(message_type::user_data, std::move(payload));
    
    auto serialized = original.serialize();
    EXPECT_GT(serialized.size(), message_header::wire_size);
    
    auto deserialized = message::deserialize(serialized);
    ASSERT_TRUE(deserialized.has_value());
    
    const auto& msg = *deserialized;
    EXPECT_EQ(msg.type(), original.type());
    EXPECT_EQ(msg.priority(), original.priority());
    EXPECT_EQ(msg.id(), original.id());
    
    auto recovered_json = msg.payload().get<nlohmann::json>();
    ASSERT_TRUE(recovered_json.has_value());
    EXPECT_EQ(*recovered_json, test_data);
}

// Message buffer tests
TEST(MessageBufferTest, BasicOperations) {
    auto endpoint = endpoint_t(boost::asio::ip::address::from_string("127.0.0.1"), 8080);
    message_buffer buffer(endpoint);
    
    EXPECT_EQ(buffer.message_count(), 0);
    EXPECT_EQ(buffer.get_endpoint(), endpoint);
    
    auto msg1 = std::make_shared<message>(message_type::ping);
    auto msg2 = std::make_shared<message>(message_type::pong);
    
    EXPECT_TRUE(buffer.push_message(msg1));
    EXPECT_TRUE(buffer.push_message(msg2));
    EXPECT_EQ(buffer.message_count(), 2);
    
    auto popped1 = buffer.pop_message();
    ASSERT_TRUE(popped1.has_value());
    EXPECT_EQ((*popped1)->type(), message_type::ping);
    
    auto popped2 = buffer.pop_message();
    ASSERT_TRUE(popped2.has_value());
    EXPECT_EQ((*popped2)->type(), message_type::pong);
    
    EXPECT_EQ(buffer.message_count(), 0);
}

TEST(MessageBufferTest, TimeoutOperations) {
    auto endpoint = endpoint_t(boost::asio::ip::address::from_string("127.0.0.1"), 8080);
    message_buffer buffer(endpoint);
    
    // Test timeout on empty buffer
    auto start = std::chrono::steady_clock::now();
    auto result = buffer.pop_message(std::chrono::milliseconds{50});
    auto elapsed = std::chrono::steady_clock::now() - start;
    
    EXPECT_FALSE(result.has_value());
    EXPECT_GE(elapsed, std::chrono::milliseconds{40}); // Allow some variance
    
    // Test immediate return when message is available
    auto msg = std::make_shared<message>(message_type::ping);
    EXPECT_TRUE(buffer.push_message(msg));
    
    start = std::chrono::steady_clock::now();
    result = buffer.pop_message(std::chrono::milliseconds{100});
    elapsed = std::chrono::steady_clock::now() - start;
    
    EXPECT_TRUE(result.has_value());
    EXPECT_LT(elapsed, std::chrono::milliseconds{10});
}

// Memory pool tests
TEST(MessageMemoryPoolTest, AllocationDeallocation) {
    message_memory_pool pool(10);
    
    auto buffer1 = pool.allocate(1024);
    auto buffer2 = pool.allocate(2048);
    
    EXPECT_NE(buffer1, nullptr);
    EXPECT_NE(buffer2, nullptr);
    
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.active_allocations, 2);
    EXPECT_GE(stats.total_allocations, 2);
    
    pool.deallocate(std::move(buffer1), 1024);
    pool.deallocate(std::move(buffer2), 2048);
    
    stats = pool.get_stats();
    EXPECT_EQ(stats.active_allocations, 0);
}

// Message pool integration tests
TEST_F(MessageSystemTest, BasicStoreRetrieve) {
    auto endpoint = create_test_endpoint("192.168.1.100", 8080);
    auto msg = std::make_shared<message>(message_type::ping);
    
    pool_->store_message(msg, endpoint);
    
    auto retrieved = pool_->retrieve_message(endpoint, std::chrono::milliseconds{100});
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ((*retrieved)->type(), message_type::ping);
    EXPECT_EQ((*retrieved)->id(), msg->id());
}

TEST_F(MessageSystemTest, MultipleEndpoints) {
    auto endpoint1 = create_test_endpoint("192.168.1.100", 8080);
    auto endpoint2 = create_test_endpoint("192.168.1.101", 8081);
    
    auto msg1 = std::make_shared<message>(message_type::ping);
    auto msg2 = std::make_shared<message>(message_type::pong);
    
    pool_->store_message(msg1, endpoint1);
    pool_->store_message(msg2, endpoint2);
    
    // Retrieve from endpoint1
    auto retrieved1 = pool_->retrieve_message(endpoint1, std::chrono::milliseconds{100});
    ASSERT_TRUE(retrieved1.has_value());
    EXPECT_EQ((*retrieved1)->id(), msg1->id());
    
    // Retrieve from endpoint2
    auto retrieved2 = pool_->retrieve_message(endpoint2, std::chrono::milliseconds{100});
    ASSERT_TRUE(retrieved2.has_value());
    EXPECT_EQ((*retrieved2)->id(), msg2->id());
}

// Concurrency tests
TEST_F(MessageSystemTest, ConcurrentAccess) {
    const int num_threads = 4;
    const int messages_per_thread = 100;
    std::atomic<int> messages_sent{0};
    std::atomic<int> messages_received{0};
    
    auto endpoint = create_test_endpoint("10.0.0.1", 9000);
    
    std::vector<std::future<void>> futures;
    
    // Producer threads
    for (int i = 0; i < num_threads; ++i) {
        futures.emplace_back(std::async(std::launch::async, [&, i]() {
            for (int j = 0; j < messages_per_thread; ++j) {
                nlohmann::json data = {{"thread", i}, {"message", j}};
                message_payload payload(data);
                auto msg = std::make_shared<message>(message_type::user_data, std::move(payload));
                
                pool_->store_message(msg, endpoint);
                messages_sent.fetch_add(1);
            }
        }));
    }
    
    // Consumer thread
    futures.emplace_back(std::async(std::launch::async, [&]() {
        int total_expected = num_threads * messages_per_thread;
        while (messages_received.load() < total_expected) {
            auto msg = pool_->retrieve_message(endpoint, std::chrono::milliseconds{10});
            if (msg.has_value()) {
                messages_received.fetch_add(1);
            }
        }
    }));
    
    // Wait for all threads to complete
    for (auto& future : futures) {
        future.wait();
    }
    
    EXPECT_EQ(messages_sent.load(), num_threads * messages_per_thread);
    EXPECT_EQ(messages_received.load(), num_threads * messages_per_thread);
}

// Performance tests
TEST_F(MessageSystemTest, PerformanceBenchmark) {
    const int num_messages = 10000;
    auto endpoint = create_test_endpoint("127.0.0.1", 7777);
    
    // Create messages
    std::vector<message_ptr> messages;
    messages.reserve(num_messages);
    
    for (int i = 0; i < num_messages; ++i) {
        nlohmann::json data = {{"id", i}, {"payload", "benchmark data"}};
        message_payload payload(data);
        messages.emplace_back(std::make_shared<message>(message_type::user_data, std::move(payload)));
    }
    
    // Benchmark storage
    auto start = std::chrono::high_resolution_clock::now();
    
    for (const auto& msg : messages) {
        pool_->store_message(msg, endpoint);
    }
    
    auto store_end = std::chrono::high_resolution_clock::now();
    
    // Benchmark retrieval
    for (int i = 0; i < num_messages; ++i) {
        auto msg = pool_->retrieve_message(endpoint, std::chrono::milliseconds{1});
        ASSERT_TRUE(msg.has_value());
    }
    
    auto retrieve_end = std::chrono::high_resolution_clock::now();
    
    auto store_duration = std::chrono::duration_cast<std::chrono::microseconds>(store_end - start);
    auto retrieve_duration = std::chrono::duration_cast<std::chrono::microseconds>(retrieve_end - store_end);
    
    std::cout << "Performance Results:\n";
    std::cout << "  Messages: " << num_messages << "\n";
    std::cout << "  Store time: " << store_duration.count() << " μs\n";
    std::cout << "  Store rate: " << (num_messages * 1000000.0 / store_duration.count()) << " msg/s\n";
    std::cout << "  Retrieve time: " << retrieve_duration.count() << " μs\n";
    std::cout << "  Retrieve rate: " << (num_messages * 1000000.0 / retrieve_duration.count()) << " msg/s\n";
    
    // Basic performance expectations (adjust based on hardware)
    EXPECT_LT(store_duration.count(), 100000); // Less than 100ms for 10k messages
    EXPECT_LT(retrieve_duration.count(), 100000);
}

// Memory leak tests
TEST_F(MessageSystemTest, MemoryLeakTest) {
    const int iterations = 1000;
    auto endpoint = create_test_endpoint("127.0.0.1", 6666);
    
    auto initial_stats = pool_->get_memory_pool().get_stats();
    
    for (int i = 0; i < iterations; ++i) {
        nlohmann::json data = {{"iteration", i}};
        message_payload payload(data);
        auto msg = std::make_shared<message>(message_type::user_data, std::move(payload));
        
        pool_->store_message(msg, endpoint);
        
        auto retrieved = pool_->retrieve_message(endpoint, std::chrono::milliseconds{10});
        ASSERT_TRUE(retrieved.has_value());
    }
    
    // Force cleanup
    pool_->cleanup_inactive_buffers(std::chrono::milliseconds{0});
    
    auto final_stats = pool_->get_memory_pool().get_stats();
    
    // Should not have memory leaks (active allocations should be the same or less)
    EXPECT_LE(final_stats.active_allocations, initial_stats.active_allocations + 5); // Allow small variance
}

// Message hub tests
TEST_F(MessageSystemTest, MessageHubDistribution) {
    std::atomic<int> messages_handled{0};
    std::vector<message_ptr> received_messages;
    std::mutex received_mutex;
    
    auto& hub = pool_->get_message_hub();
    
    hub.start([&](peer_ptr peer, message_ptr msg) {
        std::lock_guard lock(received_mutex);
        received_messages.push_back(msg);
        messages_handled.fetch_add(1);
    });
    
    auto endpoint = create_test_endpoint("172.16.0.1", 5555);
    
    // Send some messages
    const int num_messages = 50;
    for (int i = 0; i < num_messages; ++i) {
        nlohmann::json data = {{"hub_test", i}};
        message_payload payload(data);
        auto msg = std::make_shared<message>(message_type::user_data, std::move(payload));
        
        pool_->store_message(msg, endpoint);
    }
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    
    EXPECT_EQ(messages_handled.load(), num_messages);
    
    std::lock_guard lock(received_mutex);
    EXPECT_EQ(received_messages.size(), num_messages);
}

} // namespace ss::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}