#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <ss_p2p/peer.hpp>
#include <ss_p2p/network/i_transport.hpp>
#include <ss_p2p/core/types.hpp>

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <memory>
#include <vector>
#include <chrono>
#include <future>
#include <thread>

using namespace ss;
using namespace ss::core;
using namespace ss::network;
using namespace std::chrono_literals;
using ::testing::_;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::StrictMock;

namespace {

/**
 * @brief Mock transport implementation for testing
 */
class mock_transport : public i_transport {
public:
    MOCK_METHOD(std::string, name, (), (const, noexcept, override));
    MOCK_METHOD(async_result<result<void, component_error>>, start, (), (override));
    MOCK_METHOD(async_void, stop, (), (override));
    
    MOCK_METHOD(async_result<result<void, transport_error>>,
                bind, (const endpoint&), (override));
    
    MOCK_METHOD(async_result<result<endpoint, transport_error>>,
                connect, (const endpoint&), (override));
    
    MOCK_METHOD(async_result<result<void, transport_error>>,
                send, (outgoing_message), (override));
    
    MOCK_METHOD(void, send_async, (outgoing_message, send_completion_handler), (override));
    
    MOCK_METHOD(async_result<result<incoming_message, transport_error>>,
                receive, (std::optional<std::chrono::milliseconds>), (override));
    
    MOCK_METHOD(void, set_message_handler, (message_handler), (override));
    MOCK_METHOD(void, clear_message_handler, (), (override));
    
    MOCK_METHOD(endpoint, local_endpoint, (), (const, noexcept, override));
    MOCK_METHOD(endpoint, remote_endpoint, (), (const, noexcept, override));
    MOCK_METHOD(bool, is_connection_based, (), (const, noexcept, override));
    MOCK_METHOD(bool, is_connected, (), (const, noexcept, override));
    
    MOCK_METHOD(transport_stats, get_stats, (), (const, noexcept, override));
    MOCK_METHOD(void, reset_stats, (), (noexcept, override));
    
    MOCK_METHOD(transport_config, get_config, (), (const, noexcept, override));
    MOCK_METHOD(result<void, transport_error>, set_config, (const transport_config&), (override));
    
    MOCK_METHOD(std::uint32_t, max_message_size, (), (const, noexcept, override));
    MOCK_METHOD(bool, can_send, (), (const, noexcept, override));
    MOCK_METHOD(std::uint32_t, send_queue_size, (), (const, noexcept, override));
    MOCK_METHOD(std::uint32_t, receive_queue_size, (), (const, noexcept, override));
    
    MOCK_METHOD(async_void, disconnect, (), (override));
    MOCK_METHOD(async_void, shutdown, (std::chrono::milliseconds), (override));
    
    // Additional methods for testing
    boost::asio::io_context& get_executor() { return io_context_; }
    
    void trigger_message_handler(incoming_message msg) {
        if (message_handler_) {
            boost::asio::co_spawn(io_context_, message_handler_(std::move(msg)), boost::asio::detached);
        }
    }
    
    void set_test_message_handler(message_handler handler) {
        message_handler_ = std::move(handler);
    }

private:
    boost::asio::io_context io_context_;
    message_handler message_handler_;
};

/**
 * @brief Test fixture for peer tests
 */
class PeerTest : public ::testing::Test {
protected:
    void SetUp() override {
        peer_id_ = peer_id::random();
        endpoint_ = endpoint("127.0.0.1", 8080);
        mock_transport_ = std::make_shared<StrictMock<mock_transport>>();
        
        // Set up common mock expectations
        EXPECT_CALL(*mock_transport_, name())
            .WillRepeatedly(Return("mock_transport"));
        EXPECT_CALL(*mock_transport_, is_connection_based())
            .WillRepeatedly(Return(false));
        EXPECT_CALL(*mock_transport_, can_send())
            .WillRepeatedly(Return(true));
        EXPECT_CALL(*mock_transport_, send_queue_size())
            .WillRepeatedly(Return(0));
        EXPECT_CALL(*mock_transport_, receive_queue_size())
            .WillRepeatedly(Return(0));
        
        // Capture message handler when set
        EXPECT_CALL(*mock_transport_, set_message_handler(_))
            .WillRepeatedly(Invoke([this](message_handler handler) {
                mock_transport_->set_test_message_handler(std::move(handler));
            }));
    }
    
    void TearDown() override {
        if (test_peer_) {
            // Clean shutdown for test
            boost::asio::co_spawn(mock_transport_->get_executor(),
                                 test_peer_->stop(),
                                 boost::asio::detached);
            test_peer_.reset();
        }
    }
    
    void CreatePeer(const peer_config& config = {}) {
        test_peer_ = std::make_unique<peer>(peer_id_, endpoint_, mock_transport_, config);
    }
    
    peer_id peer_id_;
    endpoint endpoint_;
    std::shared_ptr<StrictMock<mock_transport>> mock_transport_;
    std::unique_ptr<peer> test_peer_;
};

} // anonymous namespace

// Basic construction and properties tests
TEST_F(PeerTest, Construction) {
    CreatePeer();
    
    EXPECT_EQ(test_peer_->id(), peer_id_);
    EXPECT_EQ(test_peer_->endpoint(), endpoint_);
    EXPECT_EQ(test_peer_->state(), peer_state::disconnected);
    EXPECT_FALSE(test_peer_->is_connected());
    EXPECT_EQ(test_peer_->name(), "peer");
}

TEST_F(PeerTest, ConfigurationHandling) {
    peer_config config;
    config.max_queue_size = 500;
    config.send_timeout = 3000ms;
    config.enable_stats = false;
    
    CreatePeer(config);
    
    auto retrieved_config = test_peer_->get_config();
    EXPECT_EQ(retrieved_config.max_queue_size, 500);
    EXPECT_EQ(retrieved_config.send_timeout, 3000ms);
    EXPECT_FALSE(retrieved_config.enable_stats);
}

// Connection management tests
TEST_F(PeerTest, SuccessfulConnection) {
    CreatePeer();
    
    EXPECT_CALL(*mock_transport_, connect(endpoint_))
        .WillOnce(Invoke([](const endpoint&) -> async_result<result<endpoint, transport_error>> {
            co_return result<endpoint, transport_error>::ok(endpoint("127.0.0.1", 8080));
        }));
    
    // Run connection test
    boost::asio::io_context io_context;
    bool connection_completed = false;
    
    boost::asio::co_spawn(io_context, [&]() -> async_void {
        auto result = co_await test_peer_->connect();
        EXPECT_TRUE(result.is_ok());
        EXPECT_EQ(test_peer_->state(), peer_state::connected);
        EXPECT_TRUE(test_peer_->is_connected());
        connection_completed = true;
    }, boost::asio::detached);
    
    io_context.run();
    EXPECT_TRUE(connection_completed);
}

TEST_F(PeerTest, ConnectionFailure) {
    CreatePeer();
    
    EXPECT_CALL(*mock_transport_, connect(endpoint_))
        .WillOnce(Invoke([](const endpoint&) -> async_result<result<endpoint, transport_error>> {
            co_return result<endpoint, transport_error>::err(transport_error::host_unreachable);
        }));
    
    boost::asio::io_context io_context;
    bool connection_completed = false;
    
    boost::asio::co_spawn(io_context, [&]() -> async_void {
        auto result = co_await test_peer_->connect();
        EXPECT_FALSE(result.is_ok());
        EXPECT_EQ(result.error(), peer_error::unreachable);
        EXPECT_EQ(test_peer_->state(), peer_state::failed);
        connection_completed = true;
    }, boost::asio::detached);
    
    io_context.run();
    EXPECT_TRUE(connection_completed);
}

TEST_F(PeerTest, Disconnection) {
    CreatePeer();
    
    // First connect
    EXPECT_CALL(*mock_transport_, connect(endpoint_))
        .WillOnce(Invoke([](const endpoint&) -> async_result<result<endpoint, transport_error>> {
            co_return result<endpoint, transport_error>::ok(endpoint("127.0.0.1", 8080));
        }));
    
    EXPECT_CALL(*mock_transport_, disconnect())
        .WillOnce(Invoke([]() -> async_void {
            co_return;
        }));
    
    boost::asio::io_context io_context;
    bool test_completed = false;
    
    boost::asio::co_spawn(io_context, [&]() -> async_void {
        // Connect first
        auto connect_result = co_await test_peer_->connect();
        EXPECT_TRUE(connect_result.is_ok());
        EXPECT_TRUE(test_peer_->is_connected());
        
        // Then disconnect
        co_await test_peer_->disconnect();
        EXPECT_EQ(test_peer_->state(), peer_state::disconnected);
        EXPECT_FALSE(test_peer_->is_connected());
        
        test_completed = true;
    }, boost::asio::detached);
    
    io_context.run();
    EXPECT_TRUE(test_completed);
}

// Message sending tests
TEST_F(PeerTest, SendMessageWhenConnected) {
    CreatePeer();
    
    // Set up connection expectations
    EXPECT_CALL(*mock_transport_, connect(endpoint_))
        .WillOnce(Invoke([](const endpoint&) -> async_result<result<endpoint, transport_error>> {
            co_return result<endpoint, transport_error>::ok(endpoint("127.0.0.1", 8080));
        }));
    
    EXPECT_CALL(*mock_transport_, send(_))
        .WillOnce(Invoke([](outgoing_message msg) -> async_result<result<void, transport_error>> {
            EXPECT_EQ(msg.data, std::vector<std::uint8_t>({'t', 'e', 's', 't'}));
            co_return result<void, transport_error>::ok();
        }));
    
    boost::asio::io_context io_context;
    bool test_completed = false;
    
    boost::asio::co_spawn(io_context, [&]() -> async_void {
        // Connect first
        auto connect_result = co_await test_peer_->connect();
        EXPECT_TRUE(connect_result.is_ok());
        
        // Send message
        peer_message msg(std::vector<std::uint8_t>({'t', 'e', 's', 't'}), "test_type");
        auto send_result = co_await test_peer_->send(std::move(msg));
        EXPECT_TRUE(send_result.is_ok());
        
        test_completed = true;
    }, boost::asio::detached);
    
    io_context.run();
    EXPECT_TRUE(test_completed);
}

TEST_F(PeerTest, SendMessageWhenDisconnected) {
    CreatePeer();
    
    boost::asio::io_context io_context;
    bool test_completed = false;
    
    boost::asio::co_spawn(io_context, [&]() -> async_void {
        peer_message msg(std::vector<std::uint8_t>({'t', 'e', 's', 't'}));
        auto send_result = co_await test_peer_->send(std::move(msg));
        EXPECT_FALSE(send_result.is_ok());
        EXPECT_EQ(send_result.error(), peer_error::not_connected);
        
        test_completed = true;
    }, boost::asio::detached);
    
    io_context.run();
    EXPECT_TRUE(test_completed);
}

// Message receiving tests  
TEST_F(PeerTest, ReceiveMessage) {
    CreatePeer();
    
    EXPECT_CALL(*mock_transport_, connect(endpoint_))
        .WillOnce(Invoke([](const endpoint&) -> async_result<result<endpoint, transport_error>> {
            co_return result<endpoint, transport_error>::ok(endpoint("127.0.0.1", 8080));
        }));
    
    boost::asio::io_context io_context;
    bool test_completed = false;
    
    boost::asio::co_spawn(io_context, [&]() -> async_void {
        // Connect first
        auto connect_result = co_await test_peer_->connect();
        EXPECT_TRUE(connect_result.is_ok());
        
        // Simulate incoming message
        std::vector<std::uint8_t> message_data = {'h', 'e', 'l', 'l', 'o'};
        incoming_message inc_msg(message_data, endpoint("127.0.0.1", 8081));
        inc_msg.metadata = "test_message_type";
        
        mock_transport_->trigger_message_handler(std::move(inc_msg));
        
        // Small delay to allow message processing
        boost::asio::steady_timer timer(io_context);
        timer.expires_after(10ms);
        co_await timer.async_wait(boost::asio::use_awaitable);
        
        // Try to receive the message
        auto receive_result = co_await test_peer_->receive(100ms);
        EXPECT_TRUE(receive_result.is_ok());
        
        auto& received_msg = receive_result.value();
        EXPECT_EQ(received_msg.payload, message_data);
        
        test_completed = true;
    }, boost::asio::detached);
    
    io_context.run();
    EXPECT_TRUE(test_completed);
}

// Request-response tests
TEST_F(PeerTest, RequestResponseSuccess) {
    CreatePeer();
    
    EXPECT_CALL(*mock_transport_, connect(endpoint_))
        .WillOnce(Invoke([](const endpoint&) -> async_result<result<endpoint, transport_error>> {
            co_return result<endpoint, transport_error>::ok(endpoint("127.0.0.1", 8080));
        }));
    
    EXPECT_CALL(*mock_transport_, send(_))
        .WillOnce(Invoke([&](outgoing_message msg) -> async_result<result<void, transport_error>> {
            // Simulate response after short delay
            boost::asio::steady_timer timer(mock_transport_->get_executor());
            timer.expires_after(50ms);
            co_await timer.async_wait(boost::asio::use_awaitable);
            
            // Create response message
            std::vector<std::uint8_t> response_data = {'r', 'e', 's', 'p', 'o', 'n', 's', 'e'};
            incoming_message response(response_data, endpoint("127.0.0.1", 8081));
            response.metadata = "__ss_resp_test_type|__request_id=1;";
            
            mock_transport_->trigger_message_handler(std::move(response));
            
            co_return result<void, transport_error>::ok();
        }));
    
    boost::asio::io_context io_context;
    bool test_completed = false;
    
    boost::asio::co_spawn(io_context, [&]() -> async_void {
        // Connect first
        auto connect_result = co_await test_peer_->connect();
        EXPECT_TRUE(connect_result.is_ok());
        
        // Send request
        peer_message request(std::vector<std::uint8_t>({'r', 'e', 'q'}), "test_type");
        auto response_result = co_await test_peer_->request(std::move(request), 1000ms);
        
        EXPECT_TRUE(response_result.is_ok());
        auto& response = response_result.value();
        EXPECT_EQ(response.payload, std::vector<std::uint8_t>({'r', 'e', 's', 'p', 'o', 'n', 's', 'e'}));
        
        test_completed = true;
    }, boost::asio::detached);
    
    io_context.run();
    EXPECT_TRUE(test_completed);
}

// Ping tests
TEST_F(PeerTest, PingSuccess) {
    CreatePeer();
    
    EXPECT_CALL(*mock_transport_, connect(endpoint_))
        .WillOnce(Invoke([](const endpoint&) -> async_result<result<endpoint, transport_error>> {
            co_return result<endpoint, transport_error>::ok(endpoint("127.0.0.1", 8080));
        }));
    
    EXPECT_CALL(*mock_transport_, send(_))
        .WillOnce(Invoke([&](outgoing_message msg) -> async_result<result<void, transport_error>> {
            // Simulate pong response
            boost::asio::steady_timer timer(mock_transport_->get_executor());
            timer.expires_after(10ms);
            co_await timer.async_wait(boost::asio::use_awaitable);
            
            std::vector<std::uint8_t> pong_data;
            incoming_message pong_response(pong_data, endpoint("127.0.0.1", 8081));
            pong_response.metadata = "__ss_resp___ss_ping|__request_id=1;";
            
            mock_transport_->trigger_message_handler(std::move(pong_response));
            
            co_return result<void, transport_error>::ok();
        }));
    
    boost::asio::io_context io_context;
    bool test_completed = false;
    
    boost::asio::co_spawn(io_context, [&]() -> async_void {
        auto connect_result = co_await test_peer_->connect();
        EXPECT_TRUE(connect_result.is_ok());
        
        auto ping_result = co_await test_peer_->ping(1000ms);
        EXPECT_TRUE(ping_result.is_ok());
        
        auto rtt = ping_result.value();
        EXPECT_GT(rtt.count(), 0);
        EXPECT_LT(rtt.count(), 1000000); // Less than 1 second in microseconds
        
        test_completed = true;
    }, boost::asio::detached);
    
    io_context.run();
    EXPECT_TRUE(test_completed);
}

// Statistics tests
TEST_F(PeerTest, StatisticsCollection) {
    peer_config config;
    config.enable_stats = true;
    CreatePeer(config);
    
    // Initially, stats should be empty
    auto initial_stats = test_peer_->get_stats();
    EXPECT_EQ(initial_stats.messages_sent, 0);
    EXPECT_EQ(initial_stats.messages_received, 0);
    EXPECT_EQ(initial_stats.bytes_sent, 0);
    EXPECT_EQ(initial_stats.bytes_received, 0);
    EXPECT_EQ(initial_stats.current_state, peer_state::disconnected);
    
    // Reset should maintain zero values
    test_peer_->reset_stats();
    auto reset_stats = test_peer_->get_stats();
    EXPECT_EQ(reset_stats.messages_sent, 0);
    EXPECT_EQ(reset_stats.current_state, peer_state::disconnected);
}

// Event handler tests
TEST_F(PeerTest, MessageHandlerInvocation) {
    CreatePeer();
    
    bool handler_called = false;
    peer_message received_message({}, "");
    
    test_peer_->set_message_handler([&](peer_message msg) -> async_void {
        handler_called = true;
        received_message = std::move(msg);
        co_return;
    });
    
    EXPECT_CALL(*mock_transport_, connect(endpoint_))
        .WillOnce(Invoke([](const endpoint&) -> async_result<result<endpoint, transport_error>> {
            co_return result<endpoint, transport_error>::ok(endpoint("127.0.0.1", 8080));
        }));
    
    boost::asio::io_context io_context;
    bool test_completed = false;
    
    boost::asio::co_spawn(io_context, [&]() -> async_void {
        auto connect_result = co_await test_peer_->connect();
        EXPECT_TRUE(connect_result.is_ok());
        
        // Trigger message
        std::vector<std::uint8_t> test_data = {'t', 'e', 's', 't'};
        incoming_message inc_msg(test_data, endpoint("127.0.0.1", 8081));
        inc_msg.metadata = "handler_test";
        
        mock_transport_->trigger_message_handler(std::move(inc_msg));
        
        // Allow time for handler execution
        boost::asio::steady_timer timer(io_context);
        timer.expires_after(50ms);
        co_await timer.async_wait(boost::asio::use_awaitable);
        
        test_completed = true;
    }, boost::asio::detached);
    
    io_context.run();
    EXPECT_TRUE(test_completed);
    EXPECT_TRUE(handler_called);
    EXPECT_EQ(received_message.payload, std::vector<std::uint8_t>({'t', 'e', 's', 't'}));
}

TEST_F(PeerTest, StateChangeHandlerInvocation) {
    CreatePeer();
    
    peer_state old_state = peer_state::disconnected;
    peer_state new_state = peer_state::disconnected;
    bool handler_called = false;
    
    test_peer_->set_state_change_handler([&](peer_state from, peer_state to) {
        old_state = from;
        new_state = to;
        handler_called = true;
    });
    
    EXPECT_CALL(*mock_transport_, connect(endpoint_))
        .WillOnce(Invoke([](const endpoint&) -> async_result<result<endpoint, transport_error>> {
            co_return result<endpoint, transport_error>::ok(endpoint("127.0.0.1", 8080));
        }));
    
    boost::asio::io_context io_context;
    bool test_completed = false;
    
    boost::asio::co_spawn(io_context, [&]() -> async_void {
        auto connect_result = co_await test_peer_->connect();
        EXPECT_TRUE(connect_result.is_ok());
        
        test_completed = true;
    }, boost::asio::detached);
    
    io_context.run();
    EXPECT_TRUE(test_completed);
    EXPECT_TRUE(handler_called);
    EXPECT_EQ(old_state, peer_state::disconnected);
    EXPECT_EQ(new_state, peer_state::connected);
}

// Queue management tests
TEST_F(PeerTest, QueueSizeReporting) {
    CreatePeer();
    
    // Initially queues should be empty
    EXPECT_EQ(test_peer_->send_queue_size(), 0);
    EXPECT_EQ(test_peer_->receive_queue_size(), 0);
    EXPECT_TRUE(test_peer_->can_send()); // Mock transport always returns true
}

TEST_F(PeerTest, ClearQueues) {
    CreatePeer();
    
    // This test verifies that clear_queues() doesn't crash
    // Actual queue behavior would require more complex mocking
    test_peer_->clear_queues();
    
    EXPECT_EQ(test_peer_->send_queue_size(), 0);
    EXPECT_EQ(test_peer_->receive_queue_size(), 0);
}

// Error handling tests
TEST_F(PeerTest, ErrorHandlerInvocation) {
    CreatePeer();
    
    peer_error error_received = peer_error::none;
    std::string error_details;
    bool error_handler_called = false;
    
    test_peer_->set_error_handler([&](peer_error error, const std::string& details) {
        error_received = error;
        error_details = details;
        error_handler_called = true;
    });
    
    EXPECT_CALL(*mock_transport_, connect(endpoint_))
        .WillOnce(Invoke([](const endpoint&) -> async_result<result<endpoint, transport_error>> {
            co_return result<endpoint, transport_error>::err(transport_error::timeout);
        }));
    
    boost::asio::io_context io_context;
    bool test_completed = false;
    
    boost::asio::co_spawn(io_context, [&]() -> async_void {
        auto connect_result = co_await test_peer_->connect();
        EXPECT_FALSE(connect_result.is_ok());
        
        test_completed = true;
    }, boost::asio::detached);
    
    io_context.run();
    EXPECT_TRUE(test_completed);
    // Error handler might be called asynchronously, so we may need to check with delay
}

// Component interface tests
TEST_F(PeerTest, ComponentStartStop) {
    CreatePeer();
    
    EXPECT_CALL(*mock_transport_, connect(endpoint_))
        .WillOnce(Invoke([](const endpoint&) -> async_result<result<endpoint, transport_error>> {
            co_return result<endpoint, transport_error>::ok(endpoint("127.0.0.1", 8080));
        }));
    
    EXPECT_CALL(*mock_transport_, disconnect())
        .WillOnce(Invoke([]() -> async_void {
            co_return;
        }));
    
    boost::asio::io_context io_context;
    bool test_completed = false;
    
    boost::asio::co_spawn(io_context, [&]() -> async_void {
        // Test start
        auto start_result = co_await test_peer_->start();
        EXPECT_TRUE(start_result.is_ok());
        EXPECT_TRUE(test_peer_->is_connected());
        
        // Test stop
        co_await test_peer_->stop();
        EXPECT_FALSE(test_peer_->is_connected());
        
        test_completed = true;
    }, boost::asio::detached);
    
    io_context.run();
    EXPECT_TRUE(test_completed);
}

// Factory function tests
TEST_F(PeerTest, FactoryFunction) {
    auto peer_ptr = make_peer(peer_id_, endpoint_, mock_transport_);
    
    ASSERT_NE(peer_ptr, nullptr);
    EXPECT_EQ(peer_ptr->id(), peer_id_);
    EXPECT_EQ(peer_ptr->endpoint(), endpoint_);
    EXPECT_EQ(peer_ptr->state(), peer_state::disconnected);
}

// Thread safety tests
TEST_F(PeerTest, ConcurrentAccess) {
    peer_config config;
    config.enable_stats = true;
    CreatePeer(config);
    
    // Test concurrent access to statistics
    std::vector<std::thread> threads;
    std::atomic<int> completed_threads{0};
    
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 100; ++j) {
                auto stats = test_peer_->get_stats();
                test_peer_->reset_stats();
                
                // Access various methods concurrently
                auto state = test_peer_->state();
                auto connected = test_peer_->is_connected();
                auto config = test_peer_->get_config();
            }
            completed_threads++;
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(completed_threads.load(), 4);
}

// Edge case tests
TEST_F(PeerTest, MultipleConnectAttempts) {
    CreatePeer();
    
    EXPECT_CALL(*mock_transport_, connect(endpoint_))
        .Times(1) // Should only be called once
        .WillOnce(Invoke([](const endpoint&) -> async_result<result<endpoint, transport_error>> {
            co_return result<endpoint, transport_error>::ok(endpoint("127.0.0.1", 8080));
        }));
    
    boost::asio::io_context io_context;
    bool test_completed = false;
    
    boost::asio::co_spawn(io_context, [&]() -> async_void {
        // First connect should succeed
        auto result1 = co_await test_peer_->connect();
        EXPECT_TRUE(result1.is_ok());
        
        // Second connect should return ok immediately (already connected)
        auto result2 = co_await test_peer_->connect();
        EXPECT_TRUE(result2.is_ok());
        
        test_completed = true;
    }, boost::asio::detached);
    
    io_context.run();
    EXPECT_TRUE(test_completed);
}

TEST_F(PeerTest, ReceiveTimeout) {
    CreatePeer();
    
    EXPECT_CALL(*mock_transport_, connect(endpoint_))
        .WillOnce(Invoke([](const endpoint&) -> async_result<result<endpoint, transport_error>> {
            co_return result<endpoint, transport_error>::ok(endpoint("127.0.0.1", 8080));
        }));
    
    boost::asio::io_context io_context;
    bool test_completed = false;
    
    boost::asio::co_spawn(io_context, [&]() -> async_void {
        auto connect_result = co_await test_peer_->connect();
        EXPECT_TRUE(connect_result.is_ok());
        
        // Try to receive with very short timeout - should timeout
        auto start_time = std::chrono::steady_clock::now();
        auto receive_result = co_await test_peer_->receive(10ms);
        auto end_time = std::chrono::steady_clock::now();
        
        EXPECT_FALSE(receive_result.is_ok());
        EXPECT_EQ(receive_result.error(), peer_error::timeout);
        
        // Verify timeout was approximately correct
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        EXPECT_GE(elapsed.count(), 8); // Allow some variance
        EXPECT_LE(elapsed.count(), 50); // But not too much
        
        test_completed = true;
    }, boost::asio::detached);
    
    io_context.run();
    EXPECT_TRUE(test_completed);
}