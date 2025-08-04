#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ss_p2p/network/impl/udp_transport.hpp"
#include "ss_p2p/core/types.hpp"

#include <boost/asio.hpp>
#include <chrono>
#include <memory>
#include <vector>
#include <atomic>
#include <thread>
#include <future>

using namespace ss::network;
using namespace ss::core;

namespace ss::network::test {

class UDPTransportTest : public ::testing::Test {
protected:
    void SetUp() override {
        io_context_ = std::make_shared<boost::asio::io_context>();
        work_guard_ = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
            io_context_->get_executor());
        
        // Start IO context in separate thread
        io_thread_ = std::thread([this]() {
            io_context_->run();
        });
        
        // Create transport instances
        transport1_ = std::make_unique<udp_transport>(*io_context_);
        transport2_ = std::make_unique<udp_transport>(*io_context_);
        
        // Setup test endpoints
        local_endpoint1_ = endpoint("127.0.0.1", 0); // Port 0 for auto-assignment
        local_endpoint2_ = endpoint("127.0.0.1", 0);
        remote_endpoint_ = endpoint("127.0.0.1", 12345);
    }
    
    void TearDown() override {
        if (transport1_) {
            boost::asio::co_spawn(*io_context_, transport1_->stop(), boost::asio::detached);
            boost::asio::co_spawn(*io_context_, transport1_->cleanup(), boost::asio::detached);
        }
        if (transport2_) {
            boost::asio::co_spawn(*io_context_, transport2_->stop(), boost::asio::detached);
            boost::asio::co_spawn(*io_context_, transport2_->cleanup(), boost::asio::detached);
        }
        
        work_guard_.reset();
        if (io_thread_.joinable()) {
            io_context_->stop();
            io_thread_.join();
        }
    }
    
    // Helper function to wait for async operations
    template<typename T>
    T wait_for_result(boost::asio::awaitable<T> awaitable, std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
        std::promise<T> promise;
        auto future = promise.get_future();
        
        boost::asio::co_spawn(*io_context_, 
            [awaitable = std::move(awaitable), &promise]() mutable -> boost::asio::awaitable<void> {
                try {
                    if constexpr (std::is_void_v<T>) {
                        co_await awaitable;
                        promise.set_value();
                    } else {
                        auto result = co_await awaitable;
                        promise.set_value(std::move(result));
                    }
                } catch (...) {
                    promise.set_exception(std::current_exception());
                }
            }, boost::asio::detached);
        
        auto status = future.wait_for(timeout);
        if (status == std::future_status::timeout) {
            throw std::runtime_error("Operation timed out");
        }
        
        return future.get();
    }
    
    std::shared_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_;
    std::thread io_thread_;
    
    std::unique_ptr<udp_transport> transport1_;
    std::unique_ptr<udp_transport> transport2_;
    
    endpoint local_endpoint1_;
    endpoint local_endpoint2_;
    endpoint remote_endpoint_;
};

// Basic lifecycle tests
TEST_F(UDPTransportTest, LifecycleManagement) {
    // Test component interface
    EXPECT_EQ(transport1_->name(), "udp_transport");
    EXPECT_FALSE(transport1_->version().empty());
    EXPECT_FALSE(transport1_->is_running());
    EXPECT_FALSE(transport1_->is_healthy());
    
    // Test initialization
    EXPECT_NO_THROW({
        wait_for_result(transport1_->initialize());
    });
    
    // Test start
    EXPECT_NO_THROW({
        wait_for_result(transport1_->start());
    });
    EXPECT_TRUE(transport1_->is_running());
    EXPECT_TRUE(transport1_->is_healthy());
    
    // Test stop
    EXPECT_NO_THROW({
        wait_for_result(transport1_->stop());
    });
    EXPECT_FALSE(transport1_->is_running());
    
    // Test cleanup
    EXPECT_NO_THROW({
        wait_for_result(transport1_->cleanup());
    });
}

// Binding and endpoint management tests
TEST_F(UDPTransportTest, BindingOperations) {
    wait_for_result(transport1_->initialize());
    wait_for_result(transport1_->start());
    
    // Test binding to specific endpoint
    auto bind_result = wait_for_result(transport1_->bind(local_endpoint1_));
    EXPECT_TRUE(bind_result.is_ok());
    
    // Local endpoint should be updated with actual bound port
    auto bound_endpoint = transport1_->local_endpoint();
    EXPECT_TRUE(bound_endpoint.is_valid());
    EXPECT_EQ(bound_endpoint.address(), "127.0.0.1");
    EXPECT_GT(bound_endpoint.port(), 0); // Should have assigned port
    
    // Test binding to already bound port should fail
    transport2_->initialize();
    transport2_->start();
    
    endpoint same_endpoint("127.0.0.1", bound_endpoint.port());
    auto second_bind_result = wait_for_result(transport2_->bind(same_endpoint));
    EXPECT_TRUE(second_bind_result.is_err());
}

TEST_F(UDPTransportTest, BindingToZeroPort) {
    wait_for_result(transport1_->initialize());
    wait_for_result(transport1_->start());
    
    endpoint zero_port_endpoint("127.0.0.1", 0);
    auto bind_result = wait_for_result(transport1_->bind(zero_port_endpoint));
    EXPECT_TRUE(bind_result.is_ok());
    
    auto bound_endpoint = transport1_->local_endpoint();
    EXPECT_GT(bound_endpoint.port(), 0); // System should assign a port
}

// Message sending and receiving tests
TEST_F(UDPTransportTest, BasicMessageSending) {
    // Setup both transports
    wait_for_result(transport1_->initialize());
    wait_for_result(transport1_->start());
    wait_for_result(transport1_->bind(local_endpoint1_));
    
    wait_for_result(transport2_->initialize());
    wait_for_result(transport2_->start());
    wait_for_result(transport2_->bind(local_endpoint2_));
    
    auto endpoint1 = transport1_->local_endpoint();
    auto endpoint2 = transport2_->local_endpoint();
    
    // Setup message handler for transport2
    std::promise<incoming_message> received_promise;
    auto received_future = received_promise.get_future();
    
    transport2_->set_message_handler([&received_promise](incoming_message msg) {
        received_promise.set_value(std::move(msg));
    });
    
    // Send message from transport1 to transport2
    std::vector<std::uint8_t> test_data = {0x01, 0x02, 0x03, 0x04, 0x05};
    outgoing_message msg(test_data, endpoint2);
    
    auto send_result = wait_for_result(transport1_->send(std::move(msg)));
    EXPECT_TRUE(send_result.is_ok());
    
    // Verify message received
    auto status = received_future.wait_for(std::chrono::milliseconds(1000));
    EXPECT_EQ(status, std::future_status::ready);
    
    auto received_msg = received_future.get();
    EXPECT_EQ(received_msg.data, test_data);
    EXPECT_EQ(received_msg.sender, endpoint1);
}

TEST_F(UDPTransportTest, AsyncMessageSending) {
    wait_for_result(transport1_->initialize());
    wait_for_result(transport1_->start());
    wait_for_result(transport1_->bind(local_endpoint1_));
    
    wait_for_result(transport2_->initialize());
    wait_for_result(transport2_->start());
    wait_for_result(transport2_->bind(local_endpoint2_));
    
    auto endpoint2 = transport2_->local_endpoint();
    
    // Setup async send completion handler
    std::promise<result<void, transport_error>> send_promise;
    auto send_future = send_promise.get_future();
    
    std::vector<std::uint8_t> test_data = {0xAA, 0xBB, 0xCC};
    outgoing_message msg(test_data, endpoint2);
    
    transport1_->send_async(std::move(msg), [&send_promise](auto result) {
        send_promise.set_value(result);
    });
    
    auto status = send_future.wait_for(std::chrono::milliseconds(1000));
    EXPECT_EQ(status, std::future_status::ready);
    
    auto send_result = send_future.get();
    EXPECT_TRUE(send_result.is_ok());
}

TEST_F(UDPTransportTest, MessageReceiving) {
    wait_for_result(transport1_->initialize());
    wait_for_result(transport1_->start());
    wait_for_result(transport1_->bind(local_endpoint1_));
    
    auto endpoint1 = transport1_->local_endpoint();
    
    // Send message using raw socket for testing receive functionality
    boost::asio::ip::udp::socket sender_socket(*io_context_);
    sender_socket.open(boost::asio::ip::udp::v4());
    
    std::vector<std::uint8_t> test_data = {0x10, 0x20, 0x30};
    
    boost::asio::co_spawn(*io_context_, 
        [&]() -> boost::asio::awaitable<void> {
            co_await sender_socket.async_send_to(
                boost::asio::buffer(test_data),
                endpoint1.native(),
                boost::asio::use_awaitable);
        }, boost::asio::detached);
    
    // Receive message
    auto receive_result = wait_for_result(
        transport1_->receive(std::chrono::milliseconds(1000)));
    
    EXPECT_TRUE(receive_result.is_ok());
    auto received_msg = receive_result.value();
    EXPECT_EQ(received_msg.data, test_data);
}

// Configuration and statistics tests
TEST_F(UDPTransportTest, ConfigurationManagement) {
    transport_config config;
    config.send_buffer_size = 32768;
    config.receive_buffer_size = 32768;
    config.max_concurrent_ops = 1000;
    config.default_timeout = std::chrono::milliseconds(2000);
    
    auto set_result = transport1_->set_config(config);
    EXPECT_TRUE(set_result.is_ok());
    
    auto retrieved_config = transport1_->get_config();
    EXPECT_EQ(retrieved_config.send_buffer_size, config.send_buffer_size);
    EXPECT_EQ(retrieved_config.receive_buffer_size, config.receive_buffer_size);
    EXPECT_EQ(retrieved_config.max_concurrent_ops, config.max_concurrent_ops);
    EXPECT_EQ(retrieved_config.default_timeout, config.default_timeout);
}

TEST_F(UDPTransportTest, StatisticsTracking) {
    wait_for_result(transport1_->initialize());
    wait_for_result(transport1_->start());
    wait_for_result(transport1_->bind(local_endpoint1_));
    
    wait_for_result(transport2_->initialize());
    wait_for_result(transport2_->start());
    wait_for_result(transport2_->bind(local_endpoint2_));
    
    auto endpoint2 = transport2_->local_endpoint();
    
    // Get initial stats
    auto initial_stats = transport1_->get_stats();
    EXPECT_EQ(initial_stats.messages_sent, 0);
    EXPECT_EQ(initial_stats.bytes_sent, 0);
    
    // Send a message
    std::vector<std::uint8_t> test_data(100, 0xFF);
    outgoing_message msg(test_data, endpoint2);
    wait_for_result(transport1_->send(std::move(msg)));
    
    // Check updated stats
    auto updated_stats = transport1_->get_stats();
    EXPECT_GT(updated_stats.messages_sent, initial_stats.messages_sent);
    EXPECT_GT(updated_stats.bytes_sent, initial_stats.bytes_sent);
    
    // Test stats reset
    transport1_->reset_stats();
    auto reset_stats = transport1_->get_stats();
    EXPECT_EQ(reset_stats.messages_sent, 0);
    EXPECT_EQ(reset_stats.bytes_sent, 0);
}

// Error handling tests
TEST_F(UDPTransportTest, InvalidEndpointHandling) {
    wait_for_result(transport1_->initialize());
    wait_for_result(transport1_->start());
    
    // Test binding to invalid endpoint
    endpoint invalid_endpoint("999.999.999.999", 8080);
    auto bind_result = wait_for_result(transport1_->bind(invalid_endpoint));
    EXPECT_TRUE(bind_result.is_err());
    EXPECT_EQ(bind_result.error(), transport_error::network_failure);
}

TEST_F(UDPTransportTest, SendToUnreachableEndpoint) {
    wait_for_result(transport1_->initialize());
    wait_for_result(transport1_->start());
    wait_for_result(transport1_->bind(local_endpoint1_));
    
    // Send to unreachable endpoint
    endpoint unreachable("10.255.255.1", 9999);
    std::vector<std::uint8_t> test_data = {0x01, 0x02};
    outgoing_message msg(test_data, unreachable);
    
    // Note: UDP is connectionless, so this might not fail immediately
    auto send_result = wait_for_result(transport1_->send(std::move(msg)));
    // UDP send typically succeeds even if destination is unreachable
    EXPECT_TRUE(send_result.is_ok());
}

TEST_F(UDPTransportTest, ReceiveTimeout) {
    wait_for_result(transport1_->initialize());
    wait_for_result(transport1_->start());
    wait_for_result(transport1_->bind(local_endpoint1_));
    
    // Try to receive with short timeout when no messages are available
    auto receive_result = wait_for_result(
        transport1_->receive(std::chrono::milliseconds(100)));
    
    EXPECT_TRUE(receive_result.is_err());
    EXPECT_EQ(receive_result.error(), transport_error::timeout);
}

// Concurrent operations tests
TEST_F(UDPTransportTest, ConcurrentSending) {
    wait_for_result(transport1_->initialize());
    wait_for_result(transport1_->start());
    wait_for_result(transport1_->bind(local_endpoint1_));
    
    wait_for_result(transport2_->initialize());
    wait_for_result(transport2_->start());
    wait_for_result(transport2_->bind(local_endpoint2_));
    
    auto endpoint2 = transport2_->local_endpoint();
    
    // Setup message counter
    std::atomic<int> received_count{0};
    transport2_->set_message_handler([&received_count](incoming_message) {
        received_count.fetch_add(1);
    });
    
    // Send multiple messages concurrently
    const int num_messages = 100;
    std::vector<std::future<result<void, transport_error>>> send_futures;
    
    for (int i = 0; i < num_messages; ++i) {
        std::vector<std::uint8_t> data = {static_cast<std::uint8_t>(i)};
        outgoing_message msg(data, endpoint2);
        
        auto send_promise = std::make_shared<std::promise<result<void, transport_error>>>();
        send_futures.push_back(send_promise->get_future());
        
        transport1_->send_async(std::move(msg), [send_promise](auto result) {
            send_promise->set_value(result);
        });
    }
    
    // Wait for all sends to complete
    int successful_sends = 0;
    for (auto& future : send_futures) {
        auto status = future.wait_for(std::chrono::milliseconds(1000));
        if (status == std::future_status::ready) {
            auto result = future.get();
            if (result.is_ok()) {
                successful_sends++;
            }
        }
    }
    
    EXPECT_EQ(successful_sends, num_messages);
    
    // Wait a bit for all messages to be received
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(received_count.load(), num_messages);
}

// Message size and fragmentation tests
TEST_F(UDPTransportTest, MaxMessageSize) {
    EXPECT_GT(transport1_->max_message_size(), 0);
    EXPECT_LE(transport1_->max_message_size(), 65507); // UDP max payload size
}

TEST_F(UDPTransportTest, LargeMessageHandling) {
    wait_for_result(transport1_->initialize());
    wait_for_result(transport1_->start());
    wait_for_result(transport1_->bind(local_endpoint1_));
    
    wait_for_result(transport2_->initialize());
    wait_for_result(transport2_->start());
    wait_for_result(transport2_->bind(local_endpoint2_));
    
    auto endpoint2 = transport2_->local_endpoint();
    
    // Create large message close to UDP limit
    std::vector<std::uint8_t> large_data(transport1_->max_message_size() - 100);
    std::iota(large_data.begin(), large_data.end(), 0);
    
    std::promise<incoming_message> received_promise;
    auto received_future = received_promise.get_future();
    
    transport2_->set_message_handler([&received_promise](incoming_message msg) {
        received_promise.set_value(std::move(msg));
    });
    
    outgoing_message msg(large_data, endpoint2);
    auto send_result = wait_for_result(transport1_->send(std::move(msg)));
    EXPECT_TRUE(send_result.is_ok());
    
    auto status = received_future.wait_for(std::chrono::milliseconds(2000));
    EXPECT_EQ(status, std::future_status::ready);
    
    auto received_msg = received_future.get();
    EXPECT_EQ(received_msg.data.size(), large_data.size());
    EXPECT_EQ(received_msg.data, large_data);
}

// Connection state tests
TEST_F(UDPTransportTest, ConnectionProperties) {
    // UDP is connectionless
    EXPECT_FALSE(transport1_->is_connection_based());
    EXPECT_FALSE(transport1_->is_connected());
    
    // Connect operation should return appropriate result for UDP
    auto connect_result = wait_for_result(transport1_->connect(remote_endpoint_));
    // UDP might handle connect differently - check implementation
}

TEST_F(UDPTransportTest, QueueSizes) {
    EXPECT_EQ(transport1_->send_queue_size(), 0);
    EXPECT_EQ(transport1_->receive_queue_size(), 0);
    EXPECT_TRUE(transport1_->can_send());
}

// Shutdown and cleanup tests
TEST_F(UDPTransportTest, GracefulShutdown) {
    wait_for_result(transport1_->initialize());
    wait_for_result(transport1_->start());
    wait_for_result(transport1_->bind(local_endpoint1_));
    
    EXPECT_TRUE(transport1_->is_running());
    
    // Test graceful shutdown with timeout
    auto shutdown_result = wait_for_result(
        transport1_->shutdown(std::chrono::milliseconds(1000)));
    
    EXPECT_FALSE(transport1_->is_running());
}

TEST_F(UDPTransportTest, MultipleStartStop) {
    // Test multiple start/stop cycles
    for (int i = 0; i < 3; ++i) {
        wait_for_result(transport1_->initialize());
        wait_for_result(transport1_->start());
        EXPECT_TRUE(transport1_->is_running());
        
        wait_for_result(transport1_->stop());
        EXPECT_FALSE(transport1_->is_running());
        
        wait_for_result(transport1_->cleanup());
    }
}

} // namespace ss::network::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}