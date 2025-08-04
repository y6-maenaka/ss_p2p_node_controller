/**
 * @file test_p2p_node.cpp
 * @brief Unit tests for P2P Node implementation
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../../../include/ss_p2p/application/i_p2p_node.hpp"
#include "../../../include/ss_p2p/core/types.hpp"

#include <boost/asio.hpp>
#include <chrono>
#include <memory>
#include <vector>

using namespace ss::application;
using namespace ss::core;
using namespace testing;

/**
 * @brief Test fixture for P2P Node tests
 */
class P2PNodeTest : public ::testing::Test {
protected:
    void SetUp() override {
        io_context_ = std::make_unique<boost::asio::io_context>();
        
        // Create test configuration
        config_.local_endpoint = endpoint("127.0.0.1", 8080);
        config_.max_connections = 100;
        config_.connection_timeout = std::chrono::milliseconds(5000);
        config_.enable_nat_traversal = true;
        config_.enable_encryption = true;
        
        // Create P2P node
        node_ = create_p2p_node(*io_context_, config_);
        ASSERT_NE(node_, nullptr);
    }

    void TearDown() override {
        if (node_ && node_->is_running()) {
            // Stop node asynchronously
            boost::asio::co_spawn(*io_context_,
                [this]() -> boost::asio::awaitable<void> {
                    co_await node_->stop();
                },
                boost::asio::detached
            );
            
            // Run context briefly to process stop
            io_context_->run_for(std::chrono::milliseconds(100));
        }
    }

    std::unique_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<i_p2p_node> node_;
    node_config config_;
};

/**
 * @brief Test node creation and basic properties
 */
TEST_F(P2PNodeTest, Creation) {
    EXPECT_NE(node_, nullptr);
    EXPECT_EQ(node_->name(), "P2PNode");
    EXPECT_FALSE(node_->is_running());
    EXPECT_FALSE(node_->is_healthy()); // Not healthy until started
}

/**
 * @brief Test node configuration
 */
TEST_F(P2PNodeTest, Configuration) {
    // Test valid configuration
    auto result = node_->configure(config_);
    EXPECT_TRUE(result.is_ok());
    
    auto retrieved_config = node_->get_config();
    EXPECT_EQ(retrieved_config.local_endpoint.port(), config_.local_endpoint.port());
    EXPECT_EQ(retrieved_config.max_connections, config_.max_connections);
    
    // Test invalid configuration
    node_config invalid_config;
    invalid_config.max_connections = 0; // Invalid
    
    auto invalid_result = node_->configure(invalid_config);
    EXPECT_FALSE(invalid_result.is_ok());
}

/**
 * @brief Test node lifecycle (initialize, start, stop)
 */
TEST_F(P2PNodeTest, Lifecycle) {
    bool initialization_completed = false;
    bool start_completed = false;
    bool stop_completed = false;
    
    // Run lifecycle operations
    boost::asio::co_spawn(*io_context_,
        [&]() -> boost::asio::awaitable<void> {
            try {
                co_await node_->initialize(config_);
                initialization_completed = true;
                
                co_await node_->start();
                start_completed = true;
                EXPECT_TRUE(node_->is_running());
                
                co_await node_->stop();
                stop_completed = true;
                EXPECT_FALSE(node_->is_running());
                
            } catch (const std::exception& e) {
                FAIL() << "Lifecycle operation failed: " << e.what();
            }
        },
        boost::asio::detached
    );
    
    // Run the context
    io_context_->run();
    
    EXPECT_TRUE(initialization_completed);
    EXPECT_TRUE(start_completed);
    EXPECT_TRUE(stop_completed);
}

/**
 * @brief Test node information retrieval
 */
TEST_F(P2PNodeTest, NodeInformation) {
    boost::asio::co_spawn(*io_context_,
        [&]() -> boost::asio::awaitable<void> {
            co_await node_->initialize(config_);
            
            // Test node ID (should be generated)
            auto node_id = node_->get_node_id();
            EXPECT_NE(node_id, core::node_id{});
            
            // Test local endpoint
            auto endpoint = node_->get_local_endpoint();
            EXPECT_EQ(endpoint.port(), config_.local_endpoint.port());
            
            // Test statistics
            auto stats = node_->get_stats();
            EXPECT_GE(stats.uptime.count(), 0);
        },
        boost::asio::detached
    );
    
    io_context_->run();
}

/**
 * @brief Test message handling registration
 */
TEST_F(P2PNodeTest, MessageHandling) {
    ss::message::app_id test_app_id = {'t', 'e', 's', 't', 0, 0, 0, 0};
    bool handler_called = false;
    
    // Register message handler
    node_->register_message_handler(test_app_id,
        [&handler_called](const ss::message&, const endpoint&) -> boost::asio::awaitable<void> {
            handler_called = true;
            co_return;
        }
    );
    
    // Test that handler is registered (we can't easily test actual message delivery without full integration)
    
    // Unregister handler
    node_->unregister_message_handler(test_app_id);
    
    // Test passes if no exceptions are thrown
    SUCCEED();
}

/**
 * @brief Test peer management operations
 */
TEST_F(P2PNodeTest, PeerManagement) {
    boost::asio::co_spawn(*io_context_,
        [&]() -> boost::asio::awaitable<void> {
            co_await node_->initialize(config_);
            co_await node_->start();
            
            // Test getting peers (should be empty initially)
            auto peers = node_->get_peers();
            EXPECT_TRUE(peers.empty());
            
            // Test getting specific peer (should not exist)
            auto peer_id = core::node_id::random();
            auto peer_info = node_->get_peer(peer_id);
            EXPECT_FALSE(peer_info.has_value());
            
            // Test connection attempt (will likely fail without actual peer)
            auto test_endpoint = endpoint("127.0.0.1", 9090);
            auto connect_result = co_await node_->connect_to_peer(test_endpoint);
            // Result depends on actual network state, so we don't assert specific outcome
            
            co_await node_->stop();
        },
        boost::asio::detached
    );
    
    io_context_->run();
}

/**
 * @brief Test advanced feature configuration
 */
TEST_F(P2PNodeTest, AdvancedFeatures) {
    // Test NAT traversal setting
    EXPECT_TRUE(node_->is_nat_traversal_enabled()); // Default from config
    node_->set_nat_traversal_enabled(false);
    EXPECT_FALSE(node_->is_nat_traversal_enabled());
    
    // Test encryption setting
    EXPECT_TRUE(node_->is_encryption_enabled()); // Default from config
    node_->set_encryption_enabled(false);
    EXPECT_FALSE(node_->is_encryption_enabled());
    
    // Test max connections setting
    EXPECT_EQ(node_->get_max_connections(), config_.max_connections);
    node_->set_max_connections(200);
    EXPECT_EQ(node_->get_max_connections(), 200);
}

/**
 * @brief Test error handling
 */
TEST_F(P2PNodeTest, ErrorHandling) {
    // Initially no error
    EXPECT_TRUE(node_->get_last_error().empty());
    
    // Test operations on non-initialized node
    boost::asio::co_spawn(*io_context_,
        [&]() -> boost::asio::awaitable<void> {
            // Try to send message without starting
            ss::message test_msg({'t', 'e', 's', 't', 0, 0, 0, 0});
            auto result = co_await node_->send_message(test_msg, endpoint("127.0.0.1", 9999));
            EXPECT_FALSE(result); // Should fail
        },
        boost::asio::detached
    );
    
    io_context_->run();
    
    // Clear error
    node_->clear_error();
    EXPECT_TRUE(node_->get_last_error().empty());
}

/**
 * @brief Test node builder pattern
 */
TEST_F(P2PNodeTest, Builder) {
    node_builder builder(*io_context_);
    
    auto built_node = builder
        .with_local_endpoint(endpoint("127.0.0.1", 8081))
        .with_max_connections(50)
        .with_nat_traversal(false)
        .with_encryption(true)
        .with_log_level(i_logger::level::debug)
        .build();
    
    EXPECT_NE(built_node, nullptr);
    EXPECT_EQ(built_node->get_local_endpoint().port(), 8081);
    EXPECT_EQ(built_node->get_max_connections(), 50);
    EXPECT_FALSE(built_node->is_nat_traversal_enabled());
    EXPECT_TRUE(built_node->is_encryption_enabled());
}

/**
 * @brief Test configuration validation
 */
class NodeConfigTest : public ::testing::Test {
protected:
    node_config config_;
};

TEST_F(NodeConfigTest, ValidConfiguration) {
    config_.local_endpoint = endpoint("127.0.0.1", 8080);
    config_.max_connections = 100;
    config_.connection_timeout = std::chrono::milliseconds(5000);
    
    auto result = config_.validate();
    EXPECT_TRUE(result.is_ok());
}

TEST_F(NodeConfigTest, InvalidConfiguration) {
    // Invalid endpoint
    config_.max_connections = 0; // Invalid
    
    auto result = config_.validate();
    EXPECT_FALSE(result.is_ok());
    EXPECT_FALSE(result.error().empty());
}

TEST_F(NodeConfigTest, JSONSerialization) {
    config_.local_endpoint = endpoint("127.0.0.1", 8080);
    config_.max_connections = 100;
    config_.enable_nat_traversal = true;
    
    // Test to JSON
    auto json = config_.to_json();
    EXPECT_TRUE(json.contains("local_endpoint"));
    EXPECT_TRUE(json.contains("max_connections"));
    EXPECT_TRUE(json.contains("enable_nat_traversal"));
    
    // Test from JSON
    auto config_result = node_config::from_json(json);
    EXPECT_TRUE(config_result.is_ok());
    
    auto loaded_config = config_result.value();
    EXPECT_EQ(loaded_config.local_endpoint.port(), config_.local_endpoint.port());
    EXPECT_EQ(loaded_config.max_connections, config_.max_connections);
    EXPECT_EQ(loaded_config.enable_nat_traversal, config_.enable_nat_traversal);
}

/**
 * @brief Test peer info functionality
 */
class PeerInfoTest : public ::testing::Test {
protected:
    peer_info info_;
    
    void SetUp() override {
        info_.id = core::node_id::random();
        info_.endpoint = endpoint("127.0.0.1", 8080);
        info_.connection_status = peer_info::status::connected;
        info_.last_seen = std::chrono::steady_clock::now();
        info_.latency = std::chrono::milliseconds(50);
        info_.quality_score = 0.8;
    }
};

TEST_F(PeerInfoTest, IsAlive) {
    // Should be alive with recent timestamp
    EXPECT_TRUE(info_.is_alive());
    
    // Should not be alive with old timestamp
    info_.last_seen = std::chrono::steady_clock::now() - std::chrono::minutes(5);
    EXPECT_FALSE(info_.is_alive(std::chrono::minutes(1)));
}

TEST_F(PeerInfoTest, JSONSerialization) {
    auto json = info_.to_json();
    
    EXPECT_TRUE(json.contains("id"));
    EXPECT_TRUE(json.contains("endpoint"));
    EXPECT_TRUE(json.contains("connection_status"));
    EXPECT_TRUE(json.contains("latency_ms"));
    EXPECT_TRUE(json.contains("quality_score"));
    
    EXPECT_EQ(json["id"].get<std::string>(), info_.id.to_hex());
    EXPECT_EQ(json["latency_ms"].get<int>(), info_.latency.count());
    EXPECT_DOUBLE_EQ(json["quality_score"].get<double>(), info_.quality_score);
}

/**
 * @brief Test statistics functionality
 */
class NodeStatsTest : public ::testing::Test {
protected:
    node_stats stats_;
    
    void SetUp() override {
        stats_.start_time = std::chrono::steady_clock::now();
        stats_.active_connections = 10;
        stats_.messages_sent = 100;
        stats_.messages_received = 150;
        stats_.bytes_sent = 10240;
        stats_.bytes_received = 15360;
    }
};

TEST_F(NodeStatsTest, UptimeCalculation) {
    stats_.update_uptime();
    EXPECT_GT(stats_.uptime.count(), 0);
}

TEST_F(NodeStatsTest, JSONSerialization) {
    auto json = stats_.to_json();
    
    EXPECT_TRUE(json.contains("active_connections"));
    EXPECT_TRUE(json.contains("messages_sent"));
    EXPECT_TRUE(json.contains("messages_received"));
    EXPECT_TRUE(json.contains("bytes_sent"));
    EXPECT_TRUE(json.contains("bytes_received"));
    
    EXPECT_EQ(json["active_connections"].get<std::size_t>(), stats_.active_connections);
    EXPECT_EQ(json["messages_sent"].get<std::size_t>(), stats_.messages_sent);
    EXPECT_EQ(json["bytes_sent"].get<std::size_t>(), stats_.bytes_sent);
}