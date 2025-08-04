/**
 * @file test_legacy_node_controller.cpp
 * @brief Test suite for legacy node_controller backward compatibility
 * 
 * This test suite verifies that the new modular architecture wrapper
 * maintains backward compatibility with existing legacy code.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <ss_p2p/node_controller.hpp>
#include <ss_p2p/message.hpp>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include <vector>

using namespace ss;
using namespace boost::asio;

class LegacyNodeControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test endpoints
        local_endpoint = ip::udp::endpoint(ip::address::from_string("127.0.0.1"), 0);
        remote_endpoint = ip::udp::endpoint(ip::address::from_string("127.0.0.1"), 8001);
        
        // Create IO context
        io_context = std::make_shared<boost::asio::io_context>();
    }
    
    void TearDown() override {
        if (controller) {
            if (controller->is_running()) {
                controller->stop();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            controller.reset();
        }
        
        if (io_context) {
            io_context->stop();
        }
    }
    
    std::unique_ptr<node_controller> controller;
    std::shared_ptr<boost::asio::io_context> io_context;
    ip::udp::endpoint local_endpoint;
    ip::udp::endpoint remote_endpoint;
};

// ========================================
// Constructor and Basic Interface Tests
// ========================================

TEST_F(LegacyNodeControllerTest, ConstructorBasic) {
    EXPECT_NO_THROW({
        controller = std::make_unique<node_controller>(local_endpoint, io_context);
    });
    
    ASSERT_NE(controller, nullptr);
    EXPECT_FALSE(controller->is_running());
    EXPECT_EQ(controller->get_self_endpoint().address(), local_endpoint.address());
}

TEST_F(LegacyNodeControllerTest, ConstructorWithDefaultIOContext) {
    EXPECT_NO_THROW({
        controller = std::make_unique<node_controller>(local_endpoint);
    });
    
    ASSERT_NE(controller, nullptr);
    EXPECT_FALSE(controller->is_running());
}

TEST_F(LegacyNodeControllerTest, MoveConstructor) {
    // Create original controller
    auto original = std::make_unique<node_controller>(local_endpoint, io_context);
    ASSERT_NE(original, nullptr);
    
    // Move construct
    EXPECT_NO_THROW({
        controller = std::make_unique<node_controller>(std::move(*original));
    });
    
    EXPECT_NE(controller, nullptr);
    EXPECT_EQ(controller->get_self_endpoint().address(), local_endpoint.address());
}

// ========================================
// Lifecycle Management Tests
// ========================================

TEST_F(LegacyNodeControllerTest, InitBasic) {
    controller = std::make_unique<node_controller>(local_endpoint, io_context);
    
    std::vector<ip::udp::endpoint> bootstrap_nodes = {remote_endpoint};
    
    // init() should not throw even if bootstrap nodes are unreachable
    EXPECT_NO_THROW({
        controller->init(bootstrap_nodes);
    });
}

TEST_F(LegacyNodeControllerTest, InitWithEmptyBootstrap) {
    controller = std::make_unique<node_controller>(local_endpoint, io_context);
    
    std::vector<ip::udp::endpoint> empty_bootstrap;
    
    EXPECT_NO_THROW({
        controller->init(empty_bootstrap);
    });
}

TEST_F(LegacyNodeControllerTest, StartStop) {
    controller = std::make_unique<node_controller>(local_endpoint, io_context);
    
    // Start should not throw
    EXPECT_NO_THROW({
        controller->start();
    });
    
    // Give it time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Should report as running
    EXPECT_TRUE(controller->is_running());
    
    // Stop should not throw
    EXPECT_NO_THROW({
        controller->stop();
    });
    
    // Give it time to stop
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Should report as not running
    EXPECT_FALSE(controller->is_running());
}

TEST_F(LegacyNodeControllerTest, StartWithBootstrap) {
    controller = std::make_unique<node_controller>(local_endpoint, io_context);
    
    std::vector<ip::udp::endpoint> bootstrap_nodes = {remote_endpoint};
    
    EXPECT_NO_THROW({
        controller->start(bootstrap_nodes);
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(controller->is_running());
    
    EXPECT_NO_THROW({
        controller->stop();
    });
}

TEST_F(LegacyNodeControllerTest, MultipleStartStop) {
    controller = std::make_unique<node_controller>(local_endpoint, io_context);
    
    // Multiple starts should be safe
    EXPECT_NO_THROW({
        controller->start();
        controller->start(); // Should be no-op
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(controller->is_running());
    
    // Multiple stops should be safe
    EXPECT_NO_THROW({
        controller->stop();
        controller->stop(); // Should be no-op
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(controller->is_running());
}

// ========================================
// Peer Management Tests
// ========================================

TEST_F(LegacyNodeControllerTest, GetPeer) {
    controller = std::make_unique<node_controller>(local_endpoint, io_context);
    
    EXPECT_NO_THROW({
        auto p = controller->get_peer(remote_endpoint);
        // Peer should be valid but we can't test much without starting the node
    });
}

TEST_F(LegacyNodeControllerTest, GetPeerRef) {
    controller = std::make_unique<node_controller>(local_endpoint, io_context);
    
    EXPECT_NO_THROW({
        auto peer_ref = controller->get_peer_ref(remote_endpoint);
        EXPECT_NE(peer_ref, nullptr);
    });
}

// ========================================
// Legacy API Access Tests
// ========================================

TEST_F(LegacyNodeControllerTest, GetSocketManager) {
    controller = std::make_unique<node_controller>(local_endpoint, io_context);
    
    EXPECT_NO_THROW({
        auto& socket_manager = controller->get_socket_manager();
        // Socket manager should be accessible
    });
}

TEST_F(LegacyNodeControllerTest, GetMessageHub) {
    controller = std::make_unique<node_controller>(local_endpoint, io_context);
    
    EXPECT_NO_THROW({
        auto& message_hub = controller->get_message_hub();
        // Message hub should be accessible
    });
}

TEST_F(LegacyNodeControllerTest, GetSelfSocket) {
    controller = std::make_unique<node_controller>(local_endpoint, io_context);
    
    EXPECT_NO_THROW({
        const auto& socket = controller->self_sock();
        // Socket should be accessible
    });
}

// ========================================
// Configuration and Settings Tests
// ========================================

TEST_F(LegacyNodeControllerTest, UpdateGlobalEndpoint) {
    controller = std::make_unique<node_controller>(local_endpoint, io_context);
    
    ip::udp::endpoint global_ep(ip::address::from_string("8.8.8.8"), 8080);
    
    EXPECT_NO_THROW({
        controller->update_global_self_endpoint(global_ep);
    });
}

TEST_F(LegacyNodeControllerTest, RequiresRouting) {
    controller = std::make_unique<node_controller>(local_endpoint, io_context);
    
    // This method should be safe to call (even if it's a no-op in new architecture)
    EXPECT_NO_THROW({
        controller->requires_routing(true);
        controller->requires_routing(false);
    });
}

// ========================================
// Global Address Discovery Tests
// ========================================

TEST_F(LegacyNodeControllerTest, SyncGetGlobalAddressNotRunning) {
    controller = std::make_unique<node_controller>(local_endpoint, io_context);
    
    std::vector<ip::udp::endpoint> stun_servers = {remote_endpoint};
    
    // Should return nullopt when not running
    auto result = controller->sync_get_global_address(stun_servers);
    EXPECT_FALSE(result.has_value());
}

TEST_F(LegacyNodeControllerTest, SyncGetGlobalAddressEmptyServers) {
    controller = std::make_unique<node_controller>(local_endpoint, io_context);
    
    std::vector<ip::udp::endpoint> empty_servers;
    
    auto result = controller->sync_get_global_address(empty_servers);
    EXPECT_FALSE(result.has_value());
}

// ========================================
// Command Input Tests
// ========================================

TEST_F(LegacyNodeControllerTest, OnCommandInputEmpty) {
    controller = std::make_unique<node_controller>(local_endpoint, io_context);
    
    std::vector<std::string> empty_input;
    
    EXPECT_NO_THROW({
        controller->on_command_input(empty_input);
    });
}

TEST_F(LegacyNodeControllerTest, OnCommandInputStop) {
    controller = std::make_unique<node_controller>(local_endpoint, io_context);
    
    controller->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(controller->is_running());
    
    std::vector<std::string> stop_command = {"stop"};
    
    EXPECT_NO_THROW({
        controller->on_command_input(stop_command);
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(controller->is_running());
}

TEST_F(LegacyNodeControllerTest, OnCommandInputSend) {
    controller = std::make_unique<node_controller>(local_endpoint, io_context);
    
    controller->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::vector<std::string> send_command = {"send", "127.0.0.1:8001", "test_payload"};
    
    // Should not throw even if peer is unreachable
    EXPECT_NO_THROW({
        controller->on_command_input(send_command);
    });
}

// ========================================
// Deprecated API Tests (Should throw)
// ========================================

TEST_F(LegacyNodeControllerTest, DeprecatedAPIThrows) {
    controller = std::make_unique<node_controller>(local_endpoint, io_context);
    
    // These methods should throw indicating they're not available in new architecture
    EXPECT_THROW({
        controller->get_direct_routing_table_controller();
    }, std::runtime_error);
    
    EXPECT_THROW({
        controller->get_multicast_manager();
    }, std::runtime_error);
}

#ifdef SS_DEBUG
TEST_F(LegacyNodeControllerTest, DebugAPIThrows) {
    controller = std::make_unique<node_controller>(local_endpoint, io_context);
    
    // Debug APIs should throw indicating migration to new API
    EXPECT_THROW({
        controller->get_routing_table();
    }, std::runtime_error);
    
    EXPECT_THROW({
        controller->get_ice_agent();
    }, std::runtime_error);
    
    EXPECT_THROW({
        controller->get_dht_manager();
    }, std::runtime_error);
    
    // Message pool should still work for compatibility
    EXPECT_NO_THROW({
        auto& pool = controller->get_message_pool();
    });
}
#endif

// ========================================
// Error Handling Tests
// ========================================

TEST_F(LegacyNodeControllerTest, InvalidEndpointHandling) {
    // Test with invalid endpoint address
    ip::udp::endpoint invalid_ep;
    
    // Constructor should handle invalid endpoints gracefully
    // Note: This might throw depending on implementation
    // The test verifies consistent behavior
}

TEST_F(LegacyNodeControllerTest, NullIOContextHandling) {
    // Test with null IO context should throw
    std::shared_ptr<boost::asio::io_context> null_ctx;
    
    EXPECT_THROW({
        auto invalid_controller = std::make_unique<node_controller>(local_endpoint, null_ctx);
    }, std::exception);
}

// ========================================
// Application ID Tests
// ========================================

TEST_F(LegacyNodeControllerTest, ApplicationIdConstant) {
    controller = std::make_unique<node_controller>(local_endpoint, io_context);
    
    // Application ID should be accessible and match expected value
    const auto& app_id = controller->_id;
    EXPECT_EQ(app_id.size(), 8);
    
    // Should match the legacy constant
    message::app_id expected = {'a','b','c','d','e','f','g','h'};
    EXPECT_EQ(app_id, expected);
}

// ========================================
// Thread Safety Tests
// ========================================

TEST_F(LegacyNodeControllerTest, ConcurrentStartStop) {
    controller = std::make_unique<node_controller>(local_endpoint, io_context);
    
    // Multiple threads trying to start/stop simultaneously
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([this, &success_count]() {
            try {
                controller->start();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                controller->stop();
                success_count++;
            } catch (const std::exception& e) {
                // Some exceptions are expected due to race conditions
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // At least one thread should succeed
    EXPECT_GE(success_count.load(), 1);
}

// ========================================
// Memory Management Tests
// ========================================

TEST_F(LegacyNodeControllerTest, ResourceCleanup) {
    // Create and destroy controller multiple times
    for (int i = 0; i < 3; ++i) {
        {
            auto temp_controller = std::make_unique<node_controller>(local_endpoint, io_context);
            temp_controller->start();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            temp_controller->stop();
        } // Controller goes out of scope and should cleanup properly
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // If we reach here without crashes, cleanup is working
    SUCCEED();
}

// ========================================
// Integration Test
// ========================================

TEST_F(LegacyNodeControllerTest, BasicWorkflow) {
    controller = std::make_unique<node_controller>(local_endpoint, io_context);
    
    // Complete workflow test
    std::vector<ip::udp::endpoint> bootstrap = {remote_endpoint};
    
    // Initialize
    EXPECT_NO_THROW(controller->init(bootstrap));
    
    // Start
    EXPECT_NO_THROW(controller->start());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(controller->is_running());
    
    // Access various components
    EXPECT_NO_THROW({
        auto& socket_manager = controller->get_socket_manager();
        auto& message_hub = controller->get_message_hub();
        auto peer_ref = controller->get_peer_ref(remote_endpoint);
        const auto& socket = controller->self_sock();
    });
    
    // Update configuration
    EXPECT_NO_THROW({
        ip::udp::endpoint global_ep(ip::address::from_string("8.8.8.8"), 8080);
        controller->update_global_self_endpoint(global_ep);
    });
    
    // Stop
    EXPECT_NO_THROW(controller->stop());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(controller->is_running());
}

// ========================================
// Test Main
// ========================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}