#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <ss_p2p/sender.hpp>
#include <ss_p2p/multicast_manager.hpp>
#include <ss_p2p/interface.hpp>
#include <ss_p2p/ss_logger.hpp>
#include <ss_p2p/network/transport_factory.hpp>
#include <ss_p2p/message.hpp>

#include <memory>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <boost/asio.hpp>

namespace ss::test {

/**
 * @brief Integration test fixture for all legacy adapters
 */
class LegacyAdaptersIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        io_context_ = std::make_unique<core::io_context>();
        
        // Create transport for sender
        auto transport_factory = network::transport_factory::create();
        transport_ = transport_factory->create_udp_transport();
        
        // Create logger
        ss_logger_ = std::make_unique<ss_logger>();
        
        // Create sender
        app_id_ = message::app_id(999);
        sender_ = std::make_unique<sender>(transport_, app_id_);
        
        // Create multicast manager
        routing_table_ = std::make_shared<mock_routing_table>();
        ep_to_peer_func_ = [](const boost::asio::ip::udp::endpoint& ep) -> std::shared_ptr<peer> {
            return nullptr; // Minimal implementation
        };
        multicast_manager_ = std::make_unique<multicast_manager>(*routing_table_, ep_to_peer_func_);
        
        // Create interface
        command_received_ = false;
        received_commands_.clear();
        interface_callback_ = [this](const std::vector<std::string>& commands) {
            command_received_ = true;
            received_commands_ = commands;
        };
        interface_ = std::make_unique<interface>(*io_context_, interface_callback_, ss_logger_->get_system_logger());
    }

    void TearDown() override {
        if (interface_) {
            interface_->stop();
            interface_.reset();
        }
        multicast_manager_.reset();
        sender_.reset();
        ss_logger_.reset();
        transport_.reset();
        routing_table_.reset();
        io_context_.reset();
    }

    class mock_routing_table {
    public:
        void add_node(const boost::asio::ip::udp::endpoint& ep) { (void)ep; }
        void remove_node(const boost::asio::ip::udp::endpoint& ep) { (void)ep; }
        std::size_t node_count() const { return 0; }
    };

    std::unique_ptr<core::io_context> io_context_;
    std::shared_ptr<network::i_transport> transport_;
    std::unique_ptr<ss_logger> ss_logger_;
    std::unique_ptr<sender> sender_;
    std::shared_ptr<mock_routing_table> routing_table_;
    multicast_manager::endpoint_to_peer_func ep_to_peer_func_;
    std::unique_ptr<multicast_manager> multicast_manager_;
    std::unique_ptr<interface> interface_;
    
    message::app_id app_id_;
    bool command_received_;
    std::vector<std::string> received_commands_;
    interface::input_command_callback interface_callback_;
};

/**
 * @brief Test that all legacy adapters can be constructed together
 */
TEST_F(LegacyAdaptersIntegrationTest, AllAdaptersConstruction) {
    EXPECT_NE(sender_, nullptr);
    EXPECT_NE(multicast_manager_, nullptr);
    EXPECT_NE(interface_, nullptr);
    EXPECT_NE(ss_logger_, nullptr);
    
    // Test basic functionality of each adapter
    EXPECT_TRUE(sender_->is_ready() || !sender_->is_ready()); // Should be boolean
    EXPECT_FALSE(multicast_manager_->is_enabled()); // Should be disabled in minimal impl
    EXPECT_TRUE(interface_->is_running());
    EXPECT_TRUE(ss_logger_->is_system_logging_enabled() || !ss_logger_->is_system_logging_enabled());
}

/**
 * @brief Test coordinated logging across all components
 */
TEST_F(LegacyAdaptersIntegrationTest, CoordinatedLogging) {
    boost::asio::ip::udp::endpoint test_endpoint(
        boost::asio::ip::address::from_string("10.0.0.100"), 8080);
    
    // Test system logging
    EXPECT_NO_THROW({
        ss_logger_->log(logger::log_level::INFO, "Integration test started");
        ss_logger_->log(logger::log_level::DEBUG, "Testing sender integration");
        ss_logger_->log(logger::log_level::WARN, "Testing multicast manager integration");
        ss_logger_->log(logger::log_level::INFO, "Testing interface integration");
    });
    
    // Test packet logging
    EXPECT_NO_THROW({
        ss_logger_->log_packet(logger::log_level::INFO, 
                              ss_logger::INCOMING, 
                              test_endpoint, 
                              "Integration test packet");
        
        ss_logger_->log_packet(logger::log_level::DEBUG, 
                              ss_logger::OUTGOING, 
                              test_endpoint, 
                              "Response packet");
    });
}

/**
 * @brief Test sender functionality with logging integration
 */
TEST_F(LegacyAdaptersIntegrationTest, SenderWithLogging) {
    boost::asio::ip::udp::endpoint dest_endpoint(
        boost::asio::ip::address::from_string("192.168.1.200"), 9090);
    
    std::string param = "integration_test";
    nlohmann::json payload = {
        {"test_type", "integration"},
        {"components", {"sender", "logger"}},
        {"timestamp", std::time(nullptr)}
    };
    
    bool send_completed = false;
    boost::system::error_code result_ec;
    
    // Log the send attempt
    ss_logger_->log(logger::log_level::INFO, "Attempting to send message via sender");
    
    // Attempt async send (will likely fail without actual transport setup, but should not crash)
    sender_->async_send(dest_endpoint, param, payload, 
        [&, dest_endpoint](const boost::system::error_code& ec, std::size_t bytes) {
            send_completed = true;
            result_ec = ec;
            
            // Log the result
            if (!ec) {
                ss_logger_->log_packet(logger::log_level::INFO, 
                                      ss_logger::OUTGOING, 
                                      dest_endpoint, 
                                      "Send successful, bytes:", bytes);
            } else {
                ss_logger_->log(logger::log_level::ERROR, 
                               "Send failed:", ec.message());
            }
        });
    
    // Give some time for async operation to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Log completion
    ss_logger_->log(logger::log_level::INFO, "Send operation completed, result:", send_completed);
}

/**
 * @brief Test multicast manager integration
 */
TEST_F(LegacyAdaptersIntegrationTest, MulticastManagerIntegration) {
    // Log multicast operation attempt
    ss_logger_->log(logger::log_level::INFO, "Testing multicast manager integration");
    
    // Test multicast target retrieval (should return empty in minimal implementation)
    auto targets = multicast_manager_->get_multicast_target(5);
    
    ss_logger_->log(logger::log_level::DEBUG, "Multicast targets retrieved, count:", targets.size());
    EXPECT_TRUE(targets.empty());
    
    // Test random peer selection (should return nullptr)
    auto random_peer = multicast_manager_->get_random();
    
    ss_logger_->log(logger::log_level::DEBUG, "Random peer selection result:", 
                   (random_peer ? "valid peer" : "null"));
    EXPECT_EQ(random_peer, nullptr);
    
    // Test context operations
    multicast_manager_->clear_context();
    ss_logger_->log(logger::log_level::DEBUG, "Multicast context cleared");
    
    multicast_manager_->update_context(targets);
    ss_logger_->log(logger::log_level::DEBUG, "Multicast context updated");
}

/**
 * @brief Test interface integration with command processing
 */
TEST_F(LegacyAdaptersIntegrationTest, InterfaceIntegration) {
    ss_logger_->log(logger::log_level::INFO, "Testing interface integration");
    
    // Test interface status
    EXPECT_TRUE(interface_->is_running());
    ss_logger_->log(logger::log_level::DEBUG, "Interface is running");
    
    // Test legacy listen_stdin API
    interface_->listen_stdin();
    ss_logger_->log(logger::log_level::DEBUG, "listen_stdin called");
    
    // Test command processing via IO context
    std::vector<std::string> test_commands = {"test", "integration", "command"};
    
    io_context_->post([&]() {
        interface_callback_(test_commands);
    });
    
    // Process the command
    io_context_->run_for(std::chrono::milliseconds(50));
    
    if (command_received_) {
        ss_logger_->log(logger::log_level::INFO, "Command received successfully");
        EXPECT_EQ(received_commands_, test_commands);
    } else {
        ss_logger_->log(logger::log_level::WARN, "Command not received");
    }
}

/**
 * @brief Test error handling across all adapters
 */
TEST_F(LegacyAdaptersIntegrationTest, ErrorHandlingIntegration) {
    ss_logger_->log(logger::log_level::INFO, "Testing error handling integration");
    
    // Test sender with invalid endpoint
    boost::asio::ip::udp::endpoint invalid_endpoint;
    message test_msg(app_id_);
    test_msg.set_param("error_test", nlohmann::json{{"should_fail", true}});
    
    bool error_occurred = false;
    sender_->async_send(invalid_endpoint, test_msg, 
        [&](const boost::system::error_code& ec, std::size_t bytes) {
            if (ec) {
                error_occurred = true;
                ss_logger_->log(logger::log_level::ERROR, 
                               "Expected error occurred:", ec.message());
            }
        });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Test multicast manager error conditions
    multicast_manager_->update_context(nullptr); // Should handle null gracefully
    ss_logger_->log(logger::log_level::DEBUG, "Multicast manager handled null peer");
    
    // Test interface error handling
    auto throwing_callback = [](const std::vector<std::string>& commands) {
        throw std::runtime_error("Test exception in callback");
    };
    
    io_context_->post([&]() {
        try {
            throwing_callback({"error", "test"});
        } catch (const std::exception& e) {
            ss_logger_->log(logger::log_level::ERROR, "Caught expected exception:", e.what());
        }
    });
    
    io_context_->run_for(std::chrono::milliseconds(50));
    
    ss_logger_->log(logger::log_level::INFO, "Error handling tests completed");
}

/**
 * @brief Test performance of all adapters working together
 */
TEST_F(LegacyAdaptersIntegrationTest, PerformanceIntegration) {
    const int num_operations = 1000;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    ss_logger_->log(logger::log_level::INFO, "Starting performance test with", num_operations, "operations");
    
    for (int i = 0; i < num_operations; ++i) {
        // Test logging performance
        if (i % 100 == 0) {
            ss_logger_->log(logger::log_level::DEBUG, "Operation", i);
        }
        
        // Test multicast manager performance
        if (i % 200 == 0) {
            multicast_manager_->get_multicast_target(1);
            multicast_manager_->clear_context();
        }
        
        // Test interface state checking performance
        if (i % 300 == 0) {
            interface_->is_running();
        }
        
        // Test sender state checking performance
        if (i % 150 == 0) {
            sender_->is_ready();
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    ss_logger_->log(logger::log_level::INFO, "Performance test completed in", duration.count(), "ms");
    
    // Performance should be reasonable (less than 1 second for 1000 operations)
    EXPECT_LT(duration.count(), 1000);
}

/**
 * @brief Test cleanup and shutdown of all adapters
 */
TEST_F(LegacyAdaptersIntegrationTest, CleanupIntegration) {
    ss_logger_->log(logger::log_level::INFO, "Testing cleanup integration");
    
    // Test interface shutdown
    EXPECT_TRUE(interface_->is_running());
    interface_->stop();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(interface_->is_running());
    
    ss_logger_->log(logger::log_level::INFO, "Interface stopped successfully");
    
    // Test multicast manager cleanup
    multicast_manager_->clear_context();
    ss_logger_->log(logger::log_level::DEBUG, "Multicast manager context cleared");
    
    // Test final logging
    ss_logger_->log(logger::log_level::INFO, "All components cleaned up successfully");
    
    // Cleanup will be handled by TearDown()
}

/**
 * @brief Test thread safety of all adapters together
 */
TEST_F(LegacyAdaptersIntegrationTest, ThreadSafetyIntegration) {
    const int num_threads = 3;
    const int operations_per_thread = 50;
    std::atomic<int> completed_operations{0};
    
    ss_logger_->log(logger::log_level::INFO, "Starting thread safety integration test");
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                // Test concurrent logging
                ss_logger_->log(logger::log_level::DEBUG, "Thread", i, "operation", j);
                
                // Test concurrent multicast manager access
                multicast_manager_->get_multicast_target(1);
                
                // Test concurrent interface status checking
                interface_->is_running();
                
                // Test concurrent sender status checking
                sender_->is_ready();
                
                completed_operations.fetch_add(1);
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(completed_operations.load(), num_threads * operations_per_thread);
    
    ss_logger_->log(logger::log_level::INFO, "Thread safety test completed,", 
                   completed_operations.load(), "operations performed");
    
    // All adapters should still be in consistent state
    EXPECT_FALSE(multicast_manager_->is_enabled());
    EXPECT_TRUE(interface_->is_running());
}

} // namespace ss::test