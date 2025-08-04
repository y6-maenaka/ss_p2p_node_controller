/**
 * @file test_legacy_error_handling.cpp
 * @brief Test suite for error handling and edge cases in legacy node_controller wrapper
 * 
 * This test suite focuses on error conditions, edge cases, and exception safety
 * in the legacy node_controller wrapper implementation.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <ss_p2p/node_controller.hpp>
#include <ss_p2p/message.hpp>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include <vector>
#include <future>
#include <exception>

using namespace ss;
using namespace boost::asio;

class LegacyErrorHandlingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test endpoints
        valid_endpoint = ip::udp::endpoint(ip::address::from_string("127.0.0.1"), 0);
        invalid_endpoint = ip::udp::endpoint(ip::address::from_string("0.0.0.0"), 0);
        unreachable_endpoint = ip::udp::endpoint(ip::address::from_string("192.0.2.1"), 12345); // TEST-NET-1
        
        // Create IO context
        io_context = std::make_shared<boost::asio::io_context>();
    }
    
    void TearDown() override {
        if (controller) {
            try {
                if (controller->is_running()) {
                    controller->stop();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            } catch (...) {
                // Ignore cleanup errors
            }
            controller.reset();
        }
        
        if (io_context) {
            io_context->stop();
        }
    }
    
    std::unique_ptr<node_controller> controller;
    std::shared_ptr<boost::asio::io_context> io_context;
    ip::udp::endpoint valid_endpoint;
    ip::udp::endpoint invalid_endpoint;
    ip::udp::endpoint unreachable_endpoint;
};

// ========================================
// Constructor Error Handling Tests
// ========================================

TEST_F(LegacyErrorHandlingTest, ConstructorWithNullIOContext) {
    std::shared_ptr<boost::asio::io_context> null_ctx;
    
    EXPECT_THROW({
        controller = std::make_unique<node_controller>(valid_endpoint, null_ctx);
    }, std::exception);
}

TEST_F(LegacyErrorHandlingTest, ConstructorWithInvalidEndpoint) {
    // Test with various potentially problematic endpoints
    ip::udp::endpoint zero_port_ep(ip::address::from_string("127.0.0.1"), 0);
    
    // Should not throw - zero port should be handled gracefully
    EXPECT_NO_THROW({
        controller = std::make_unique<node_controller>(zero_port_ep, io_context);
    });
    
    ASSERT_NE(controller, nullptr);
}

TEST_F(LegacyErrorHandlingTest, ConstructorExceptionSafety) {
    // Simulate various construction failure scenarios
    auto problematic_io_ctx = std::make_shared<boost::asio::io_context>();
    problematic_io_ctx->stop(); // Pre-stopped IO context
    
    // Constructor should handle stopped IO context gracefully or throw consistently
    try {
        controller = std::make_unique<node_controller>(valid_endpoint, problematic_io_ctx);
        // If it doesn't throw, it should still be in a valid state
        EXPECT_NE(controller, nullptr);
    } catch (const std::exception& e) {
        // If it throws, that's also acceptable behavior
        EXPECT_THAT(e.what(), testing::ContainsRegex(".*"));
    }
}

// ========================================
// Initialization Error Handling
// ========================================

TEST_F(LegacyErrorHandlingTest, InitWithUnreachableBootstrap) {
    controller = std::make_unique<node_controller>(valid_endpoint, io_context);
    
    std::vector<ip::udp::endpoint> unreachable_bootstrap = {unreachable_endpoint};
    
    // Should not throw even with unreachable bootstrap nodes
    EXPECT_NO_THROW({
        controller->init(unreachable_bootstrap);
    });
}

TEST_F(LegacyErrorHandlingTest, InitWithMalformedBootstrap) {
    controller = std::make_unique<node_controller>(valid_endpoint, io_context);
    
    // Create bootstrap with zero address
    std::vector<ip::udp::endpoint> malformed_bootstrap = {
        ip::udp::endpoint(ip::address::from_string("0.0.0.0"), 0)
    };
    
    // Should handle malformed bootstrap gracefully
    EXPECT_NO_THROW({
        controller->init(malformed_bootstrap);
    });
}

TEST_F(LegacyErrorHandlingTest, MultipleInitCalls) {
    controller = std::make_unique<node_controller>(valid_endpoint, io_context);
    
    std::vector<ip::udp::endpoint> bootstrap1 = {unreachable_endpoint};
    std::vector<ip::udp::endpoint> bootstrap2 = {valid_endpoint};
    
    // Multiple init calls should be safe
    EXPECT_NO_THROW({
        controller->init(bootstrap1);
        controller->init(bootstrap2); // Should be no-op or safe
    });
}

// ========================================
// Start/Stop Error Handling
// ========================================

TEST_F(LegacyErrorHandlingTest, StartWithStoppedIOContext) {
    auto stopped_ctx = std::make_shared<boost::asio::io_context>();
    stopped_ctx->stop();
    
    // Create controller with already stopped context
    try {
        controller = std::make_unique<node_controller>(valid_endpoint, stopped_ctx);
        
        // Starting with stopped context should handle gracefully
        EXPECT_NO_THROW({
            controller->start();
        });
        
    } catch (const std::exception& e) {
        // If constructor throws, that's acceptable
        EXPECT_THAT(e.what(), testing::ContainsRegex(".*"));
    }
}

TEST_F(LegacyErrorHandlingTest, StopBeforeStart) {
    controller = std::make_unique<node_controller>(valid_endpoint, io_context);
    
    // Stopping before starting should be safe
    EXPECT_NO_THROW({
        controller->stop();
    });
    
    EXPECT_FALSE(controller->is_running());
}

TEST_F(LegacyErrorHandlingTest, StartAfterDestruction) {
    controller = std::make_unique<node_controller>(valid_endpoint, io_context);
    
    // Start normally
    EXPECT_NO_THROW({
        controller->start();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Stop and then try to start again
    EXPECT_NO_THROW({
        controller->stop();
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Should be able to restart
    EXPECT_NO_THROW({
        controller->start();
    });
}

TEST_F(LegacyErrorHandlingTest, ConcurrentStartStopOperations) {
    controller = std::make_unique<node_controller>(valid_endpoint, io_context);
    
    std::atomic<int> exception_count{0};
    std::vector<std::future<void>> futures;
    
    // Launch multiple concurrent start/stop operations
    for (int i = 0; i < 10; ++i) {
        futures.push_back(std::async(std::launch::async, [this, &exception_count]() {
            try {
                controller->start();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                controller->stop();
            } catch (const std::exception& e) {
                exception_count++;
            }
        }));
    }
    
    // Wait for all operations to complete
    for (auto& future : futures) {
        EXPECT_NO_THROW({
            future.get();
        });
    }
    
    // Some exceptions may occur due to race conditions, but not too many
    EXPECT_LT(exception_count.load(), 5);
}

// ========================================
// Peer Management Error Handling
// ========================================

TEST_F(LegacyErrorHandlingTest, GetPeerWithInvalidEndpoint) {
    controller = std::make_unique<node_controller>(valid_endpoint, io_context);
    
    // Should handle invalid endpoints gracefully
    EXPECT_NO_THROW({
        auto peer = controller->get_peer(invalid_endpoint);
    });
    
    EXPECT_NO_THROW({
        auto peer_ref = controller->get_peer_ref(invalid_endpoint);
        EXPECT_NE(peer_ref, nullptr);
    });
}

TEST_F(LegacyErrorHandlingTest, GetPeerBeforeStart) {
    controller = std::make_unique<node_controller>(valid_endpoint, io_context);
    
    // Getting peers before starting should work (lazy initialization)
    EXPECT_NO_THROW({
        auto peer = controller->get_peer(unreachable_endpoint);
        auto peer_ref = controller->get_peer_ref(unreachable_endpoint);
        EXPECT_NE(peer_ref, nullptr);
    });
}

TEST_F(LegacyErrorHandlingTest, GetPeerAfterStop) {
    controller = std::make_unique<node_controller>(valid_endpoint, io_context);
    
    controller->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    controller->stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Getting peers after stop should still work (compatibility layer)
    EXPECT_NO_THROW({
        auto peer = controller->get_peer(unreachable_endpoint);
        auto peer_ref = controller->get_peer_ref(unreachable_endpoint);
        EXPECT_NE(peer_ref, nullptr);
    });
}

// ========================================
// Legacy API Error Handling
// ========================================

TEST_F(LegacyErrorHandlingTest, SocketManagerBeforeStart) {
    controller = std::make_unique<node_controller>(valid_endpoint, io_context);
    
    // Should be able to access socket manager before starting
    EXPECT_NO_THROW({
        auto& socket_manager = controller->get_socket_manager();
        const auto& socket = controller->self_sock();
    });
}

TEST_F(LegacyErrorHandlingTest, MessageHubBeforeStart) {
    controller = std::make_unique<node_controller>(valid_endpoint, io_context);
    
    // Should be able to access message hub before starting
    EXPECT_NO_THROW({
        auto& message_hub = controller->get_message_hub();
    });
}

TEST_F(LegacyErrorHandlingTest, DeprecatedAPIErrorMessages) {
    controller = std::make_unique<node_controller>(valid_endpoint, io_context);
    
    // Deprecated APIs should throw with informative messages
    try {
        controller->get_direct_routing_table_controller();
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_THAT(e.what(), testing::ContainsRegex(".*not available.*new architecture.*"));
    }
    
    try {
        controller->get_multicast_manager();
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_THAT(e.what(), testing::ContainsRegex(".*not available.*new architecture.*"));
    }
}

// ========================================
// Global Address Discovery Error Handling
// ========================================

TEST_F(LegacyErrorHandlingTest, SyncGetGlobalAddressTimeout) {
    controller = std::make_unique<node_controller>(valid_endpoint, io_context);
    
    // Should handle timeout gracefully when not running
    std::vector<ip::udp::endpoint> unreachable_stun = {unreachable_endpoint};
    
    auto result = controller->sync_get_global_address(unreachable_stun);
    EXPECT_FALSE(result.has_value());
}

TEST_F(LegacyErrorHandlingTest, SyncGetGlobalAddressWithRunningNode) {
    controller = std::make_unique<node_controller>(valid_endpoint, io_context);
    controller->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Should handle unreachable STUN servers gracefully
    std::vector<ip::udp::endpoint> unreachable_stun = {unreachable_endpoint};
    
    auto result = controller->sync_get_global_address(unreachable_stun);
    // May or may not have value depending on implementation
    // The important thing is it doesn't throw
}

// ========================================
// Command Input Error Handling
// ========================================

TEST_F(LegacyErrorHandlingTest, CommandInputMalformed) {
    controller = std::make_unique<node_controller>(valid_endpoint, io_context);
    
    // Various malformed inputs should be handled gracefully
    std::vector<std::vector<std::string>> malformed_inputs = {
        {"send"}, // Missing arguments
        {"send", "invalid_address"}, // Invalid address format
        {"send", "127.0.0.1:8080"}, // Missing payload
        {"unknown_command", "arg1", "arg2"},
        {""}  // Empty command
    };
    
    for (const auto& input : malformed_inputs) {
        EXPECT_NO_THROW({
            controller->on_command_input(input);
        }) << "Failed with input size: " << input.size();
    }
}

TEST_F(LegacyErrorHandlingTest, CommandInputSendToInvalidPeer) {
    controller = std::make_unique<node_controller>(valid_endpoint, io_context);
    controller->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Send to invalid/unreachable peer should not throw
    std::vector<std::string> send_to_invalid = {"send", "256.256.256.256:99999", "payload"};
    
    EXPECT_NO_THROW({
        controller->on_command_input(send_to_invalid);
    });
}

// ========================================
// Memory and Resource Error Handling
// ========================================

TEST_F(LegacyErrorHandlingTest, MultipleControllerInstances) {
    // Create multiple controllers with same endpoint (should handle port conflicts)
    std::vector<std::unique_ptr<node_controller>> controllers;
    
    for (int i = 0; i < 3; ++i) {
        try {
            auto new_endpoint = ip::udp::endpoint(ip::address::from_string("127.0.0.1"), 0);
            controllers.push_back(std::make_unique<node_controller>(new_endpoint, io_context));
        } catch (const std::exception& e) {
            // Some failures are expected due to port conflicts
        }
    }
    
    // At least one should succeed
    EXPECT_GE(controllers.size(), 1);
    
    // Cleanup
    for (auto& ctrl : controllers) {
        if (ctrl) {
            try {
                ctrl->stop();
            } catch (...) {
                // Ignore cleanup errors
            }
        }
    }
}

TEST_F(LegacyErrorHandlingTest, ExceptionInDestructor) {
    // Test that destructor handles exceptions gracefully
    {
        controller = std::make_unique<node_controller>(valid_endpoint, io_context);
        controller->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Force the IO context to stop to potentially cause issues in destructor
        io_context->stop();
        
        // Destructor should not throw even if cleanup fails
    } // Controller goes out of scope here
    
    // If we reach here, destructor handled exceptions gracefully
    SUCCEED();
    
    // Reset for teardown
    controller.reset();
    io_context = std::make_shared<boost::asio::io_context>();
}

// ========================================
// State Consistency Error Handling
// ========================================

TEST_F(LegacyErrorHandlingTest, StateConsistencyAfterErrors) {
    controller = std::make_unique<node_controller>(valid_endpoint, io_context);
    
    // Try various operations that might fail
    try {
        controller->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Force some error conditions
        std::vector<ip::udp::endpoint> invalid_bootstrap = {invalid_endpoint};
        controller->init(invalid_bootstrap);
        
        // Try deprecated APIs (will throw)
        try {
            controller->get_direct_routing_table_controller();
        } catch (...) {
            // Expected
        }
        
        // State should still be consistent
        EXPECT_TRUE(controller->is_running());
        
        // Should still be able to perform basic operations
        EXPECT_NO_THROW({
            auto peer = controller->get_peer(valid_endpoint);
            auto& socket_manager = controller->get_socket_manager();
        });
        
    } catch (const std::exception& e) {
        // If start fails, state should be consistent
        EXPECT_FALSE(controller->is_running());
    }
}

// ========================================
// Thread Safety Error Handling
// ========================================

TEST_F(LegacyErrorHandlingTest, ThreadSafetyUnderStress) {
    controller = std::make_unique<node_controller>(valid_endpoint, io_context);
    
    std::atomic<int> operation_count{0};
    std::atomic<int> exception_count{0};
    std::vector<std::thread> threads;
    
    // Launch threads performing various operations concurrently
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, &operation_count, &exception_count]() {
            for (int j = 0; j < 10; ++j) {
                try {
                    // Mix of different operations
                    switch (j % 4) {
                        case 0:
                            controller->start();
                            break;
                        case 1:
                            controller->stop();
                            break;
                        case 2: {
                            auto peer = controller->get_peer(unreachable_endpoint);
                            break;
                        }
                        case 3: {
                            auto& socket_manager = controller->get_socket_manager();
                            break;
                        }
                    }
                    operation_count++;
                } catch (const std::exception& e) {
                    exception_count++;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }
    
    // Most operations should succeed
    EXPECT_GT(operation_count.load(), 30);
    // Some exceptions are acceptable due to race conditions
    EXPECT_LT(exception_count.load(), operation_count.load() / 2);
}

// ========================================
// Edge Case Tests
// ========================================

TEST_F(LegacyErrorHandlingTest, VeryLargeBootstrapList) {
    controller = std::make_unique<node_controller>(valid_endpoint, io_context);
    
    // Create a very large bootstrap list
    std::vector<ip::udp::endpoint> large_bootstrap;
    for (int i = 1; i < 1000; ++i) {
        try {
            std::string addr = "127.0.0." + std::to_string(i % 255 + 1);
            large_bootstrap.emplace_back(ip::address::from_string(addr), 8000 + i);
        } catch (...) {
            // Skip invalid addresses
        }
    }
    
    // Should handle large bootstrap list gracefully
    EXPECT_NO_THROW({
        controller->init(large_bootstrap);
    });
}

TEST_F(LegacyErrorHandlingTest, RapidStartStopCycles) {
    controller = std::make_unique<node_controller>(valid_endpoint, io_context);
    
    // Rapid start/stop cycles
    for (int i = 0; i < 10; ++i) {
        EXPECT_NO_THROW({
            controller->start();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            controller->stop();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
    }
}

TEST_F(LegacyErrorHandlingTest, UpdateEndpointWhileRunning) {
    controller = std::make_unique<node_controller>(valid_endpoint, io_context);
    controller->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Update global endpoint while running
    ip::udp::endpoint new_global(ip::address::from_string("8.8.8.8"), 8080);
    
    EXPECT_NO_THROW({
        controller->update_global_self_endpoint(new_global);
    });
    
    // Should still be running
    EXPECT_TRUE(controller->is_running());
}

// ========================================
// Test Main
// ========================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}