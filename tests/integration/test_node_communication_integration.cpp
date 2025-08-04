#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ss_p2p/node_controller.hpp"
#include "ss_p2p/core/types.hpp"
#include "ss_p2p/peer.hpp"

#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include <future>
#include <atomic>
#include <unordered_map>

using namespace ss;
using namespace ss::core;

namespace ss::integration::test {

class NodeCommunicationIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        io_context_ = std::make_shared<boost::asio::io_context>();
        work_guard_ = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
            io_context_->get_executor());
        
        // Start IO context in separate thread
        io_thread_ = std::thread([this]() {
            io_context_->run();
        });
        
        // Create test endpoints
        setupTestEndpoints();
        
        // Create node controllers
        setupNodeControllers();
    }
    
    void TearDown() override {
        // Cleanup nodes
        for (auto& node : nodes_) {
            if (node) {
                try {
                    boost::asio::co_spawn(*io_context_, node->stop(), boost::asio::detached);
                } catch (...) {
                    // Ignore cleanup errors
                }
            }
        }
        
        nodes_.clear();
        
        work_guard_.reset();
        if (io_thread_.joinable()) {
            io_context_->stop();
            io_thread_.join();
        }
    }
    
    void setupTestEndpoints() {
        // Create endpoints for test nodes
        for (int i = 0; i < NUM_TEST_NODES; ++i) {
            endpoints_.emplace_back("127.0.0.1", BASE_PORT + i);
        }
        
        // Bootstrap node endpoints
        bootstrap_endpoints_ = {endpoints_[0], endpoints_[1]};
    }
    
    void setupNodeControllers() {
        for (int i = 0; i < NUM_TEST_NODES; ++i) {
            auto node = std::make_unique<node_controller>(endpoints_[i], *io_context_);
            nodes_.push_back(std::move(node));
        }
    }
    
    // Helper function to wait for async operations
    template<typename T>
    T wait_for_result(boost::asio::awaitable<T> awaitable, std::chrono::milliseconds timeout = std::chrono::milliseconds(10000)) {
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
    
    // Wait for network to stabilize
    void waitForNetworkStabilization(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
        auto start_time = std::chrono::steady_clock::now();
        
        while (std::chrono::steady_clock::now() - start_time < timeout) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Check if all nodes have discovered each other
            bool all_connected = true;
            for (size_t i = 0; i < nodes_.size() && all_connected; ++i) {
                for (size_t j = 0; j < nodes_.size(); ++j) {
                    if (i != j) {
                        try {
                            auto peer = nodes_[i]->get_peer(endpoints_[j]);
                            if (!peer.is_connected()) {
                                all_connected = false;
                                break;
                            }
                        } catch (...) {
                            all_connected = false;
                            break;
                        }
                    }
                }
            }
            
            if (all_connected) {
                return;
            }
        }
        
        // Network didn't stabilize within timeout - continue with test anyway
    }
    
    static constexpr int NUM_TEST_NODES = 5;
    static constexpr int BASE_PORT = 18000;
    
    std::shared_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_;
    std::thread io_thread_;
    
    std::vector<std::unique_ptr<node_controller>> nodes_;
    std::vector<endpoint> endpoints_;
    std::vector<endpoint> bootstrap_endpoints_;
};

// Basic node startup and discovery tests
TEST_F(NodeCommunicationIntegrationTest, NodeStartupAndDiscovery) {
    // Start bootstrap nodes first
    for (int i = 0; i < 2; ++i) {
        EXPECT_NO_THROW({
            wait_for_result(nodes_[i]->start({})); // Bootstrap nodes start without peers
        });
        EXPECT_TRUE(nodes_[i]->is_running());
    }
    
    // Wait for bootstrap nodes to initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Start remaining nodes with bootstrap endpoints
    for (int i = 2; i < NUM_TEST_NODES; ++i) {
        EXPECT_NO_THROW({
            wait_for_result(nodes_[i]->start(bootstrap_endpoints_));
        });
        EXPECT_TRUE(nodes_[i]->is_running());
    }
    
    // Wait for network to stabilize
    waitForNetworkStabilization();
    
    // Verify all nodes can discover each other
    for (int i = 0; i < NUM_TEST_NODES; ++i) {
        for (int j = 0; j < NUM_TEST_NODES; ++j) {
            if (i != j) {
                EXPECT_NO_THROW({
                    auto peer = nodes_[i]->get_peer(endpoints_[j]);
                    // Note: Connection might not be established immediately
                    // but peer should be discoverable
                });
            }
        }
    }
}

// Point-to-point communication tests
TEST_F(NodeCommunicationIntegrationTest, PointToPointCommunication) {
    // Start all nodes
    wait_for_result(nodes_[0]->start({}));
    wait_for_result(nodes_[1]->start({}));
    
    for (int i = 2; i < NUM_TEST_NODES; ++i) {
        wait_for_result(nodes_[i]->start(bootstrap_endpoints_));
    }
    
    waitForNetworkStabilization();
    
    // Test communication between nodes 0 and 1
    auto peer_0_to_1 = nodes_[0]->get_peer(endpoints_[1]);
    auto peer_1_to_0 = nodes_[1]->get_peer(endpoints_[0]);
    
    // Create test message
    json test_message;
    test_message["type"] = "test_message";
    test_message["content"] = "Hello from node 0";
    test_message["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Set up message reception
    std::promise<json> received_promise;
    auto received_future = received_promise.get_future();
    
    auto& message_hub_1 = nodes_[1]->get_message_hub();
    message_hub_1.start([&received_promise](const endpoint& sender, const json& message) {
        received_promise.set_value(message);
        return true; // Handled
    });
    
    // Send message
    EXPECT_TRUE(peer_0_to_1.send(test_message));
    
    // Wait for message reception
    auto status = received_future.wait_for(std::chrono::milliseconds(2000));
    EXPECT_EQ(status, std::future_status::ready);
    
    if (status == std::future_status::ready) {
        auto received_message = received_future.get();
        EXPECT_EQ(received_message["type"], "test_message");
        EXPECT_EQ(received_message["content"], "Hello from node 0");
    }
}

// Broadcast communication tests
TEST_F(NodeCommunicationIntegrationTest, BroadcastCommunication) {
    // Start all nodes
    wait_for_result(nodes_[0]->start({}));
    for (int i = 1; i < NUM_TEST_NODES; ++i) {
        wait_for_result(nodes_[i]->start(bootstrap_endpoints_));
    }
    
    waitForNetworkStabilization();
    
    // Set up message reception counters
    std::atomic<int> message_count{0};
    std::vector<std::promise<json>> receive_promises(NUM_TEST_NODES - 1);
    std::vector<std::future<json>> receive_futures;
    
    for (int i = 0; i < NUM_TEST_NODES - 1; ++i) {
        receive_futures.push_back(receive_promises[i].get_future());
    }
    
    // Set up message handlers for all nodes except sender (node 0)
    for (int i = 1; i < NUM_TEST_NODES; ++i) {
        auto& message_hub = nodes_[i]->get_message_hub();
        message_hub.start([&message_count, &receive_promises, i](const endpoint& sender, const json& message) {
            message_count.fetch_add(1);
            if (i - 1 < static_cast<int>(receive_promises.size())) {
                receive_promises[i - 1].set_value(message);
            }
            return true;
        });
    }
    
    // Create broadcast message
    json broadcast_message;
    broadcast_message["type"] = "broadcast_test";
    broadcast_message["content"] = "Hello everyone!";
    broadcast_message["sender_id"] = 0;
    
    // Get multicast manager from node 0
    auto& multicast_manager = nodes_[0]->get_multicast_manager();
    
    // Broadcast message to all peers
    std::vector<endpoint> target_endpoints;
    for (int i = 1; i < NUM_TEST_NODES; ++i) {
        target_endpoints.push_back(endpoints_[i]);
    }
    
    bool broadcast_sent = multicast_manager.multicast(target_endpoints, broadcast_message);
    EXPECT_TRUE(broadcast_sent);
    
    // Wait for all messages to be received
    int received_count = 0;
    for (auto& future : receive_futures) {
        auto status = future.wait_for(std::chrono::milliseconds(3000));
        if (status == std::future_status::ready) {
            auto received_message = future.get();
            EXPECT_EQ(received_message["type"], "broadcast_test");
            EXPECT_EQ(received_message["content"], "Hello everyone!");
            received_count++;
        }
    }
    
    EXPECT_EQ(received_count, NUM_TEST_NODES - 1);
    EXPECT_EQ(message_count.load(), NUM_TEST_NODES - 1);
}

// Request-response pattern tests
TEST_F(NodeCommunicationIntegrationTest, RequestResponsePattern) {
    // Start nodes
    wait_for_result(nodes_[0]->start({}));
    wait_for_result(nodes_[1]->start(bootstrap_endpoints_));
    
    waitForNetworkStabilization();
    
    // Set up request handler on node 1
    auto& message_hub_1 = nodes_[1]->get_message_hub();
    message_hub_1.start([this](const endpoint& sender, const json& message) {
        if (message["type"] == "ping_request") {
            // Send response back
            auto peer = nodes_[1]->get_peer(sender);
            json response;
            response["type"] = "ping_response";
            response["original_id"] = message["id"];
            response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            peer.send(response);
            return true;
        }
        return false;
    });
    
    // Set up response handler on node 0
    std::promise<json> response_promise;
    auto response_future = response_promise.get_future();
    
    auto& message_hub_0 = nodes_[0]->get_message_hub();
    message_hub_0.start([&response_promise](const endpoint& sender, const json& message) {
        if (message["type"] == "ping_response") {
            response_promise.set_value(message);
            return true;
        }
        return false;
    });
    
    // Send ping request from node 0 to node 1
    auto peer_0_to_1 = nodes_[0]->get_peer(endpoints_[1]);
    
    json ping_request;
    ping_request["type"] = "ping_request";
    ping_request["id"] = "test_ping_123";
    ping_request["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    EXPECT_TRUE(peer_0_to_1.send(ping_request));
    
    // Wait for response
    auto status = response_future.wait_for(std::chrono::milliseconds(3000));
    EXPECT_EQ(status, std::future_status::ready);
    
    if (status == std::future_status::ready) {
        auto response = response_future.get();
        EXPECT_EQ(response["type"], "ping_response");
        EXPECT_EQ(response["original_id"], "test_ping_123");
        EXPECT_TRUE(response.contains("timestamp"));
    }
}

// Network resilience and recovery tests
TEST_F(NodeCommunicationIntegrationTest, NetworkResilienceAndRecovery) {
    // Start all nodes
    wait_for_result(nodes_[0]->start({}));
    for (int i = 1; i < NUM_TEST_NODES; ++i) {
        wait_for_result(nodes_[i]->start(bootstrap_endpoints_));
    }
    
    waitForNetworkStabilization();
    
    // Verify initial connectivity
    auto initial_peer = nodes_[0]->get_peer(endpoints_[2]);
    EXPECT_NO_THROW(initial_peer.send(json{{"type", "test"}, {"content", "initial"}}));
    
    // Simulate node failure by stopping node 1 (bootstrap node)
    wait_for_result(nodes_[1]->stop());
    EXPECT_FALSE(nodes_[1]->is_running());
    
    // Wait for network to adjust
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // Test communication between remaining nodes
    json test_message;
    test_message["type"] = "resilience_test";
    test_message["content"] = "Communication after node failure";
    
    std::promise<json> recovery_promise;
    auto recovery_future = recovery_promise.get_future();
    
    auto& message_hub_2 = nodes_[2]->get_message_hub();
    message_hub_2.start([&recovery_promise](const endpoint& sender, const json& message) {
        if (message["type"] == "resilience_test") {
            recovery_promise.set_value(message);
            return true;
        }
        return false;
    });
    
    // Send message from node 0 to node 2
    auto peer_0_to_2 = nodes_[0]->get_peer(endpoints_[2]);
    EXPECT_TRUE(peer_0_to_2.send(test_message));
    
    // Verify message is received
    auto status = recovery_future.wait_for(std::chrono::milliseconds(3000));
    EXPECT_EQ(status, std::future_status::ready);
    
    // Restart the failed node
    wait_for_result(nodes_[1]->start(bootstrap_endpoints_));
    EXPECT_TRUE(nodes_[1]->is_running());
    
    // Wait for network to re-stabilize
    waitForNetworkStabilization();
    
    // Verify recovered node can communicate
    json recovery_test_message;
    recovery_test_message["type"] = "recovery_test";
    recovery_test_message["content"] = "Node recovered successfully";
    
    std::promise<json> final_promise;
    auto final_future = final_promise.get_future();
    
    auto& message_hub_1_recovered = nodes_[1]->get_message_hub();
    message_hub_1_recovered.start([&final_promise](const endpoint& sender, const json& message) {
        if (message["type"] == "recovery_test") {
            final_promise.set_value(message);
            return true;
        }
        return false;
    });
    
    auto peer_0_to_1_recovered = nodes_[0]->get_peer(endpoints_[1]);
    EXPECT_TRUE(peer_0_to_1_recovered.send(recovery_test_message));
    
    auto final_status = final_future.wait_for(std::chrono::milliseconds(3000));
    EXPECT_EQ(final_status, std::future_status::ready);
}

// Performance and load tests
TEST_F(NodeCommunicationIntegrationTest, MessageThroughputTest) {
    // Start nodes
    wait_for_result(nodes_[0]->start({}));
    wait_for_result(nodes_[1]->start(bootstrap_endpoints_));
    
    waitForNetworkStabilization();
    
    const int num_messages = 100;
    std::atomic<int> received_count{0};
    
    // Set up message counter on receiving node
    auto& message_hub_1 = nodes_[1]->get_message_hub();
    message_hub_1.start([&received_count](const endpoint& sender, const json& message) {
        if (message["type"] == "throughput_test") {
            received_count.fetch_add(1);
            return true;
        }
        return false;
    });
    
    auto peer_0_to_1 = nodes_[0]->get_peer(endpoints_[1]);
    
    // Send messages rapidly
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_messages; ++i) {
        json message;
        message["type"] = "throughput_test";
        message["sequence"] = i;
        message["payload"] = std::string(100, 'A' + (i % 26)); // 100-byte payload
        
        peer_0_to_1.send(message);
    }
    
    // Wait for all messages to be received
    auto timeout = std::chrono::milliseconds(10000);
    auto end_time = start_time + timeout;
    
    while (received_count.load() < num_messages && 
           std::chrono::high_resolution_clock::now() < end_time) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    auto actual_end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(actual_end_time - start_time);
    
    // Check results
    int final_received = received_count.load();
    EXPECT_GE(final_received, num_messages * 0.95); // Allow 5% message loss
    
    if (final_received > 0) {
        double throughput = static_cast<double>(final_received) / duration.count() * 1000.0; // messages per second
        std::cout << "Throughput: " << throughput << " messages/second" << std::endl;
        std::cout << "Received: " << final_received << "/" << num_messages << " messages" << std::endl;
        
        // Expect reasonable throughput
        EXPECT_GT(throughput, 10.0); // At least 10 messages per second
    }
}

// Concurrent access and thread safety tests
TEST_F(NodeCommunicationIntegrationTest, ConcurrentMessageHandling) {
    // Start nodes
    wait_for_result(nodes_[0]->start({}));
    for (int i = 1; i < NUM_TEST_NODES; ++i) {
        wait_for_result(nodes_[i]->start(bootstrap_endpoints_));
    }
    
    waitForNetworkStabilization();
    
    const int messages_per_thread = 20;
    const int num_sender_threads = 4;
    std::atomic<int> total_received{0};
    
    // Set up message handler on target node
    auto& message_hub_target = nodes_[NUM_TEST_NODES - 1]->get_message_hub();
    message_hub_target.start([&total_received](const endpoint& sender, const json& message) {
        if (message["type"] == "concurrent_test") {
            total_received.fetch_add(1);
            // Simulate some processing time
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            return true;
        }
        return false;
    });
    
    // Launch concurrent sender threads
    std::vector<std::thread> sender_threads;
    std::atomic<int> messages_sent{0};
    
    for (int thread_id = 0; thread_id < num_sender_threads; ++thread_id) {
        sender_threads.emplace_back([this, thread_id, messages_per_thread, &messages_sent]() {
            auto peer = nodes_[thread_id % (NUM_TEST_NODES - 1)]->get_peer(endpoints_[NUM_TEST_NODES - 1]);
            
            for (int i = 0; i < messages_per_thread; ++i) {
                json message;
                message["type"] = "concurrent_test";
                message["thread_id"] = thread_id;
                message["sequence"] = i;
                message["payload"] = "Thread " + std::to_string(thread_id) + " message " + std::to_string(i);
                
                if (peer.send(message)) {
                    messages_sent.fetch_add(1);
                }
                
                // Small delay to prevent overwhelming
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : sender_threads) {
        thread.join();
    }
    
    // Wait for all messages to be processed
    auto timeout = std::chrono::milliseconds(10000);
    auto start_wait = std::chrono::steady_clock::now();
    
    while (total_received.load() < messages_sent.load() && 
           std::chrono::steady_clock::now() - start_wait < timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // Verify results
    int final_sent = messages_sent.load();
    int final_received = total_received.load();
    
    std::cout << "Concurrent test: Sent " << final_sent << ", Received " << final_received << std::endl;
    
    EXPECT_GT(final_sent, 0);
    EXPECT_GE(final_received, final_sent * 0.9); // Allow 10% message loss in concurrent scenario
}

// Clean shutdown tests
TEST_F(NodeCommunicationIntegrationTest, CleanShutdown) {
    // Start all nodes
    for (int i = 0; i < NUM_TEST_NODES; ++i) {
        if (i < 2) {
            wait_for_result(nodes_[i]->start({}));
        } else {
            wait_for_result(nodes_[i]->start(bootstrap_endpoints_));
        }
        EXPECT_TRUE(nodes_[i]->is_running());
    }
    
    waitForNetworkStabilization();
    
    // Shutdown nodes gracefully
    for (int i = 0; i < NUM_TEST_NODES; ++i) {
        EXPECT_NO_THROW({
            wait_for_result(nodes_[i]->stop());
        });
        EXPECT_FALSE(nodes_[i]->is_running());
    }
    
    // Verify all nodes are properly stopped
    for (int i = 0; i < NUM_TEST_NODES; ++i) {
        EXPECT_FALSE(nodes_[i]->is_running());
    }
}

} // namespace ss::integration::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}