#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ss_p2p/network/impl/udp_transport.hpp"
#include "ss_p2p/core/types.hpp"

#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>

namespace ss::network::impl::test {

/**
 * @brief Test fixture for UDP transport tests
 */
class UDPTransportTest : public ::testing::Test {
protected:
    void SetUp() override {
        transport_config config;
        config.send_buffer_size = 8192;
        config.receive_buffer_size = 8192;
        config.max_concurrent_ops = 100;
        config.io_threads = 1;
        
        transport_ = std::make_unique<udp_transport>(io_context_, config);
    }

    void TearDown() override {
        if (transport_) {
            // Stop transport gracefully
            boost::asio::co_spawn(io_context_,
                transport_->stop(),
                boost::asio::detached
            );
            
            // Run briefly to process shutdown
            io_context_.run_for(std::chrono::milliseconds{100});
            transport_.reset();
        }
        
        // Reset io_context
        io_context_.restart();
    }

    void RunIOContext(std::chrono::milliseconds duration = std::chrono::milliseconds{100}) {
        io_context_.run_for(duration);
    }

    boost::asio::io_context io_context_;
    std::unique_ptr<udp_transport> transport_;
};

// Basic functionality tests

TEST_F(UDPTransportTest, ComponentInterface) {
    EXPECT_EQ(transport_->name(), "udp_transport");
    EXPECT_EQ(transport_->version(), "1.0.0");
    EXPECT_FALSE(transport_->is_running());
    EXPECT_FALSE(transport_->is_healthy());
    EXPECT_FALSE(transport_->is_connection_based());
}

TEST_F(UDPTransportTest, LifecycleManagement) {
    // Initialize
    boost::asio::co_spawn(io_context_,
        transport_->initialize(),
        boost::asio::detached
    );
    RunIOContext();

    // Start
    boost::asio::co_spawn(io_context_,
        transport_->start(),
        boost::asio::detached
    );
    RunIOContext();
    
    EXPECT_TRUE(transport_->is_running());
    EXPECT_FALSE(transport_->is_healthy());  // Not bound yet

    // Stop
    boost::asio::co_spawn(io_context_,
        transport_->stop(),
        boost::asio::detached
    );
    RunIOContext();
    
    EXPECT_FALSE(transport_->is_running());

    // Cleanup
    boost::asio::co_spawn(io_context_,
        transport_->cleanup(),
        boost::asio::detached
    );
    RunIOContext();
}

TEST_F(UDPTransportTest, Binding) {
    // Initialize and start
    boost::asio::co_spawn(io_context_,
        transport_->initialize(),
        boost::asio::detached
    );
    RunIOContext();
    
    boost::asio::co_spawn(io_context_,
        transport_->start(),
        boost::asio::detached
    );
    RunIOContext();

    // Test binding
    ss::core::endpoint bind_endpoint("127.0.0.1", 0);  // Let OS choose port
    bool bind_success = false;
    
    boost::asio::co_spawn(io_context_,
        [this, &bind_endpoint, &bind_success]() -> ss::core::async_void {
            auto result = co_await transport_->bind(bind_endpoint);
            bind_success = result.is_ok();
            
            if (bind_success) {
                EXPECT_TRUE(transport_->is_connected());
                EXPECT_TRUE(transport_->is_healthy());
                
                auto local_ep = transport_->local_endpoint();
                EXPECT_EQ(local_ep.address(), "127.0.0.1");
                EXPECT_GT(local_ep.port(), 0);
            }
        },
        boost::asio::detached
    );
    RunIOContext();
    
    EXPECT_TRUE(bind_success);
}

TEST_F(UDPTransportTest, PseudoConnection) {
    ss::core::endpoint remote_endpoint("127.0.0.1", 8080);
    bool connect_success = false;
    
    boost::asio::co_spawn(io_context_,
        [this, &remote_endpoint, &connect_success]() -> ss::core::async_void {
            auto result = co_await transport_->connect(remote_endpoint);
            connect_success = result.is_ok();
            
            if (connect_success) {
                EXPECT_EQ(transport_->remote_endpoint(), remote_endpoint);
            }
        },
        boost::asio::detached
    );
    RunIOContext();
    
    EXPECT_TRUE(connect_success);
}

TEST_F(UDPTransportTest, ConfigurationManagement) {
    auto config = transport_->get_config();
    EXPECT_EQ(config.send_buffer_size, 8192);
    EXPECT_EQ(config.receive_buffer_size, 8192);
    EXPECT_EQ(config.max_concurrent_ops, 100);

    // Update configuration
    transport_config new_config = config;
    new_config.send_buffer_size = 16384;
    new_config.receive_buffer_size = 16384;
    new_config.max_concurrent_ops = 200;

    auto result = transport_->set_config(new_config);
    EXPECT_TRUE(result.is_ok());

    auto updated_config = transport_->get_config();
    EXPECT_EQ(updated_config.send_buffer_size, 16384);
    EXPECT_EQ(updated_config.receive_buffer_size, 16384);
    EXPECT_EQ(updated_config.max_concurrent_ops, 200);
}

TEST_F(UDPTransportTest, Statistics) {
    auto stats = transport_->get_stats();
    
    // Initial stats should be zero
    EXPECT_EQ(stats.bytes_sent, 0);
    EXPECT_EQ(stats.bytes_received, 0);
    EXPECT_EQ(stats.messages_sent, 0);
    EXPECT_EQ(stats.messages_received, 0);
    EXPECT_EQ(stats.send_errors, 0);
    EXPECT_EQ(stats.receive_errors, 0);
    EXPECT_GT(stats.max_message_size, 0);

    // Reset stats
    transport_->reset_stats();
    stats = transport_->get_stats();
    EXPECT_EQ(stats.bytes_sent, 0);
    EXPECT_EQ(stats.bytes_received, 0);
}

TEST_F(UDPTransportTest, MessageSizeConstraints) {
    EXPECT_GT(transport_->max_message_size(), 0);
    EXPECT_TRUE(transport_->can_send());
    EXPECT_EQ(transport_->send_queue_size(), 0);
    EXPECT_EQ(transport_->receive_queue_size(), 0);
}

// Error handling tests

TEST_F(UDPTransportTest, SendWithoutBind) {
    std::vector<std::uint8_t> data = {1, 2, 3, 4, 5};
    ss::core::endpoint target("127.0.0.1", 8080);
    outgoing_message msg(data, target);
    
    bool send_failed = false;
    
    boost::asio::co_spawn(io_context_,
        [this, &msg, &send_failed]() -> ss::core::async_void {
            auto result = co_await transport_->send(std::move(msg));
            send_failed = !result.is_ok();
            
            if (send_failed) {
                EXPECT_EQ(result.error(), transport_error::not_connected);
            }
        },
        boost::asio::detached
    );
    RunIOContext();
    
    EXPECT_TRUE(send_failed);
}

TEST_F(UDPTransportTest, InvalidConfiguration) {
    transport_config invalid_config;
    invalid_config.send_buffer_size = 0;  // Invalid
    invalid_config.receive_buffer_size = 0;  // Invalid
    
    auto result = transport_->set_config(invalid_config);
    EXPECT_FALSE(result.is_ok());
    EXPECT_EQ(result.error(), transport_error::invalid_argument);
}

TEST_F(UDPTransportTest, MessageTooLarge) {
    // Create oversized message
    std::vector<std::uint8_t> large_data(transport_->max_message_size() + 1, 0x42);
    ss::core::endpoint target("127.0.0.1", 8080);
    outgoing_message msg(large_data, target);
    
    bool size_error = false;
    
    boost::asio::co_spawn(io_context_,
        [this, &msg, &size_error]() -> ss::core::async_void {
            auto result = co_await transport_->send(std::move(msg));
            size_error = !result.is_ok() && result.error() == transport_error::message_too_large;
        },
        boost::asio::detached
    );
    RunIOContext();
    
    EXPECT_TRUE(size_error);
}

// Message handling tests

TEST_F(UDPTransportTest, MessageHandler) {
    std::atomic<bool> handler_called{false};
    std::vector<std::uint8_t> received_data;
    ss::core::endpoint received_from;
    
    // Set message handler
    transport_->set_message_handler(
        [&handler_called, &received_data, &received_from](incoming_message msg) -> ss::core::async_void {
            handler_called.store(true);
            received_data = std::move(msg.data);
            received_from = msg.sender;
            co_return;
        }
    );
    
    // Clear handler
    transport_->clear_message_handler();
    
    // Handler should be cleared now
    // This test mainly verifies the API works without crashes
}

TEST_F(UDPTransportTest, ReceiveTimeout) {
    bool timeout_occurred = false;
    
    boost::asio::co_spawn(io_context_,
        [this, &timeout_occurred]() -> ss::core::async_void {
            auto result = co_await transport_->receive(std::chrono::milliseconds{50});
            timeout_occurred = !result.is_ok() && result.error() == transport_error::timeout;
        },
        boost::asio::detached
    );
    
    RunIOContext(std::chrono::milliseconds{100});  // Wait longer than timeout
    
    EXPECT_TRUE(timeout_occurred);
}

// Async operations tests

TEST_F(UDPTransportTest, AsyncSend) {
    std::vector<std::uint8_t> data = {1, 2, 3, 4, 5};
    ss::core::endpoint target("127.0.0.1", 8080);
    outgoing_message msg(data, target);
    
    std::atomic<bool> callback_called{false};
    std::atomic<bool> send_failed{false};
    
    transport_->send_async(std::move(msg), 
        [&callback_called, &send_failed](ss::core::result<void, transport_error> result) {
            callback_called.store(true);
            send_failed.store(!result.is_ok());
        }
    );
    
    RunIOContext();
    
    EXPECT_TRUE(callback_called.load());
    EXPECT_TRUE(send_failed.load());  // Should fail because not bound
}

// Shutdown tests

TEST_F(UDPTransportTest, GracefulShutdown) {
    // Initialize and start
    boost::asio::co_spawn(io_context_,
        transport_->initialize(),
        boost::asio::detached
    );
    RunIOContext();
    
    boost::asio::co_spawn(io_context_,
        transport_->start(),
        boost::asio::detached
    );
    RunIOContext();
    
    EXPECT_TRUE(transport_->is_running());
    
    // Test graceful shutdown
    boost::asio::co_spawn(io_context_,
        transport_->shutdown(std::chrono::milliseconds{1000}),
        boost::asio::detached
    );
    RunIOContext(std::chrono::milliseconds{200});
    
    EXPECT_FALSE(transport_->is_running());
}

TEST_F(UDPTransportTest, Disconnect) {
    // Set up pseudo-connection
    ss::core::endpoint remote_endpoint("127.0.0.1", 8080);
    
    boost::asio::co_spawn(io_context_,
        transport_->connect(remote_endpoint),
        boost::asio::detached
    );
    RunIOContext();
    
    // Test disconnect
    boost::asio::co_spawn(io_context_,
        transport_->disconnect(),
        boost::asio::detached
    );
    RunIOContext();
    
    // Remote endpoint should be cleared
    EXPECT_FALSE(transport_->remote_endpoint().is_valid());
}

// Integration test with actual networking

TEST_F(UDPTransportTest, LoopbackCommunication) {
    // This test requires two transport instances to communicate
    transport_config config;
    config.io_threads = 1;
    
    auto transport2 = std::make_unique<udp_transport>(io_context_, config);
    
    // Initialize both transports
    boost::asio::co_spawn(io_context_,
        transport_->initialize(),
        boost::asio::detached
    );
    boost::asio::co_spawn(io_context_,
        transport2->initialize(),
        boost::asio::detached
    );
    RunIOContext();
    
    // Start both transports
    boost::asio::co_spawn(io_context_,
        transport_->start(),
        boost::asio::detached
    );
    boost::asio::co_spawn(io_context_,
        transport2->start(),
        boost::asio::detached
    );
    RunIOContext();
    
    // Bind first transport
    ss::core::endpoint bind_ep1("127.0.0.1", 0);
    ss::core::endpoint actual_ep1;
    
    boost::asio::co_spawn(io_context_,
        [this, &bind_ep1, &actual_ep1]() -> ss::core::async_void {
            auto result = co_await transport_->bind(bind_ep1);
            if (result.is_ok()) {
                actual_ep1 = transport_->local_endpoint();
            }
        },
        boost::asio::detached
    );
    RunIOContext();
    
    // Bind second transport
    ss::core::endpoint bind_ep2("127.0.0.1", 0);
    ss::core::endpoint actual_ep2;
    
    boost::asio::co_spawn(io_context_,
        [&transport2, &bind_ep2, &actual_ep2]() -> ss::core::async_void {
            auto result = co_await transport2->bind(bind_ep2);
            if (result.is_ok()) {
                actual_ep2 = transport2->local_endpoint();
            }
        },
        boost::asio::detached
    );
    RunIOContext();
    
    // If both binds succeeded, test communication
    if (actual_ep1.is_valid() && actual_ep2.is_valid()) {
        std::vector<std::uint8_t> test_data = {0xDE, 0xAD, 0xBE, 0xEF};
        outgoing_message msg(test_data, actual_ep2);
        
        bool send_success = false;
        boost::asio::co_spawn(io_context_,
            [this, &msg, &send_success]() -> ss::core::async_void {
                auto result = co_await transport_->send(std::move(msg));
                send_success = result.is_ok();
            },
            boost::asio::detached
        );
        RunIOContext();
        
        // Note: In a real test, we'd need to set up proper message handlers
        // and synchronization to verify the message was received
    }
    
    // Cleanup second transport
    boost::asio::co_spawn(io_context_,
        transport2->stop(),
        boost::asio::detached
    );
    RunIOContext();
}

// Error code conversion tests

TEST_F(UDPTransportTest, ErrorCodeStrings) {
    EXPECT_NE(to_string(transport_error::none), "");
    EXPECT_NE(to_string(transport_error::network_failure), "");
    EXPECT_NE(to_string(transport_error::connection_refused), "");
    EXPECT_NE(to_string(transport_error::timeout), "");
    EXPECT_NE(to_string(transport_error::message_too_large), "");
}

// Performance/stress tests

TEST_F(UDPTransportTest, MultipleAsyncSends) {
    const int num_sends = 100;
    std::atomic<int> callbacks_received{0};
    
    for (int i = 0; i < num_sends; ++i) {
        std::vector<std::uint8_t> data = {
            static_cast<std::uint8_t>(i & 0xFF)
        };
        ss::core::endpoint target("127.0.0.1", 8080);
        outgoing_message msg(data, target);
        
        transport_->send_async(std::move(msg),
            [&callbacks_received](auto result) {
                callbacks_received.fetch_add(1);
            }
        );
    }
    
    // Run IO context to process all sends
    RunIOContext(std::chrono::milliseconds{500});
    
    EXPECT_EQ(callbacks_received.load(), num_sends);
}

} // namespace ss::network::impl::test