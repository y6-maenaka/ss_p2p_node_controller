/**
 * @file test_message_bus.cpp
 * @brief Unit tests for Message Bus implementation
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../../../include/ss_p2p/application/i_message_bus.hpp"
#include "../../../include/ss_p2p/core/types.hpp"
#include "../../../include/ss_p2p/message.hpp"

#include <boost/asio.hpp>
#include <chrono>
#include <memory>
#include <atomic>

using namespace ss::application;
using namespace ss::core;
using namespace testing;

/**
 * @brief Test fixture for Message Bus tests
 */
class MessageBusTest : public ::testing::Test {
protected:
    void SetUp() override {
        io_context_ = std::make_unique<boost::asio::io_context>();
        
        // Create test configuration
        config_.max_queue_size = 1000;
        config_.message_ttl = std::chrono::minutes(5);
        config_.worker_threads = 2;
        config_.enable_persistence = false;
        
        // Create message bus
        bus_ = create_message_bus(*io_context_, config_);
        ASSERT_NE(bus_, nullptr);
    }

    void TearDown() override {
        if (bus_ && bus_->is_running()) {
            boost::asio::co_spawn(*io_context_,
                [this]() -> boost::asio::awaitable<void> {
                    co_await bus_->stop();
                },
                boost::asio::detached
            );
            io_context_->run_for(std::chrono::milliseconds(100));
        }
    }

    std::unique_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<i_message_bus> bus_;
    message_bus_config config_;
};

/**
 * @brief Test message bus creation and basic properties
 */
TEST_F(MessageBusTest, Creation) {
    EXPECT_NE(bus_, nullptr);
    EXPECT_EQ(bus_->name(), "MessageBus");
    EXPECT_FALSE(bus_->is_running());
    EXPECT_EQ(bus_->get_queue_size(), 0);
}

/**
 * @brief Test message bus lifecycle
 */
TEST_F(MessageBusTest, Lifecycle) {
    bool initialization_completed = false;
    bool start_completed = false;
    bool stop_completed = false;
    
    boost::asio::co_spawn(*io_context_,
        [&]() -> boost::asio::awaitable<void> {
            try {
                co_await bus_->initialize(config_);
                initialization_completed = true;
                
                co_await bus_->start();
                start_completed = true;
                EXPECT_TRUE(bus_->is_running());
                
                co_await bus_->stop();
                stop_completed = true;
                EXPECT_FALSE(bus_->is_running());
                
            } catch (const std::exception& e) {
                FAIL() << "Lifecycle operation failed: " << e.what();
            }
        },
        boost::asio::detached
    );
    
    io_context_->run();
    
    EXPECT_TRUE(initialization_completed);
    EXPECT_TRUE(start_completed);
    EXPECT_TRUE(stop_completed);
}

/**
 * @brief Test basic publish/subscribe functionality
 */
TEST_F(MessageBusTest, BasicPubSub) {
    std::atomic<bool> message_received{false};
    std::string received_content;
    
    boost::asio::co_spawn(*io_context_,
        [&]() -> boost::asio::awaitable<void> {
            co_await bus_->initialize(config_);
            co_await bus_->start();
            
            // Subscribe to messages
            auto subscription_id = bus_->subscribe(
                "test.topic",
                [&](const bus_message& msg) -> boost::asio::awaitable<bool> {
                    // Extract content from message
                    auto content_param = msg.payload.get_param("content");
                    if (content_param) {
                        received_content = content_param->get<std::string>();
                    }
                    message_received.store(true);
                    co_return true;
                }
            );
            
            EXPECT_GT(subscription_id, 0);
            
            // Publish a message
            ss::message test_msg({'t', 'e', 's', 't', 0, 0, 0, 0});
            test_msg.set_param("content", "Hello, World!");
            
            auto publish_result = co_await bus_->publish(
                "test.topic",
                test_msg,
                endpoint("127.0.0.1", 8080)
            );
            
            EXPECT_TRUE(publish_result);
            
            // Wait a moment for message processing
            co_await boost::asio::steady_timer(*io_context_, std::chrono::milliseconds(100)).async_wait(boost::asio::use_awaitable);
            
            co_await bus_->stop();
        },
        boost::asio::detached
    );
    
    io_context_->run();
    
    EXPECT_TRUE(message_received.load());
    EXPECT_EQ(received_content, "Hello, World!");
}

/**
 * @brief Test topic pattern matching
 */
TEST_F(MessageBusTest, TopicPatterns) {
    std::atomic<int> messages_received{0};
    
    boost::asio::co_spawn(*io_context_,
        [&]() -> boost::asio::awaitable<void> {
            co_await bus_->initialize(config_);
            co_await bus_->start();
            
            // Subscribe with wildcard pattern
            bus_->subscribe(
                "test.*",
                [&](const bus_message& msg) -> boost::asio::awaitable<bool> {
                    messages_received.fetch_add(1);
                    co_return true;
                }
            );
            
            // Publish messages to different topics
            ss::message test_msg({'t', 'e', 's', 't', 0, 0, 0, 0});
            auto endpoint = endpoint("127.0.0.1", 8080);
            
            co_await bus_->publish("test.topic1", test_msg, endpoint);
            co_await bus_->publish("test.topic2", test_msg, endpoint);
            co_await bus_->publish("other.topic", test_msg, endpoint); // Should not match
            
            // Wait for processing
            co_await boost::asio::steady_timer(*io_context_, std::chrono::milliseconds(100)).async_wait(boost::asio::use_awaitable);
            
            co_await bus_->stop();
        },
        boost::asio::detached
    );
    
    io_context_->run();
    
    EXPECT_EQ(messages_received.load(), 2); // Only test.* topics should match
}

/**
 * @brief Test message filtering by tags
 */
TEST_F(MessageBusTest, MessageFiltering) {
    std::atomic<int> messages_received{0};
    
    boost::asio::co_spawn(*io_context_,
        [&]() -> boost::asio::awaitable<void> {
            co_await bus_->initialize(config_);
            co_await bus_->start();
            
            // Subscribe with tag filtering
            bus_->subscribe(
                "test.topic",
                [&](const bus_message& msg) -> boost::asio::awaitable<bool> {
                    messages_received.fetch_add(1);
                    co_return true;
                },
                {"important"}, // Required tags
                {"spam"}       // Excluded tags
            );
            
            ss::message test_msg({'t', 'e', 's', 't', 0, 0, 0, 0});
            auto endpoint = endpoint("127.0.0.1", 8080);
            
            // Should match (has important tag, no spam tag)
            co_await bus_->publish("test.topic", test_msg, endpoint, 0, {"important"});
            
            // Should not match (has spam tag)
            co_await bus_->publish("test.topic", test_msg, endpoint, 0, {"important", "spam"});
            
            // Should not match (missing important tag)
            co_await bus_->publish("test.topic", test_msg, endpoint, 0, {"other"});
            
            // Wait for processing
            co_await boost::asio::steady_timer(*io_context_, std::chrono::milliseconds(100)).async_wait(boost::asio::use_awaitable);
            
            co_await bus_->stop();
        },
        boost::asio::detached
    );
    
    io_context_->run();
    
    EXPECT_EQ(messages_received.load(), 1); // Only first message should match
}

/**
 * @brief Test topic management
 */
TEST_F(MessageBusTest, TopicManagement) {
    boost::asio::co_spawn(*io_context_,
        [&]() -> boost::asio::awaitable<void> {
            co_await bus_->initialize(config_);
            co_await bus_->start();
            
            // Test topic creation
            EXPECT_TRUE(bus_->create_topic("test.topic1"));
            EXPECT_TRUE(bus_->create_topic("test.topic2", 500)); // Custom size
            EXPECT_FALSE(bus_->create_topic("test.topic1")); // Already exists
            
            // Test topic existence
            EXPECT_TRUE(bus_->topic_exists("test.topic1"));
            EXPECT_TRUE(bus_->topic_exists("test.topic2"));
            EXPECT_FALSE(bus_->topic_exists("nonexistent.topic"));
            
            // Test getting topics
            auto topics = bus_->get_topics();
            EXPECT_GE(topics.size(), 2);
            EXPECT_TRUE(std::find(topics.begin(), topics.end(), "test.topic1") != topics.end());
            
            // Test topic deletion
            EXPECT_TRUE(bus_->delete_topic("test.topic1"));
            EXPECT_FALSE(bus_->topic_exists("test.topic1"));
            
            co_await bus_->stop();
        },
        boost::asio::detached
    );
    
    io_context_->run();
}

/**
 * @brief Test subscription management
 */
TEST_F(MessageBusTest, SubscriptionManagement) {
    boost::asio::co_spawn(*io_context_,
        [&]() -> boost::asio::awaitable<void> {
            co_await bus_->initialize(config_);
            co_await bus_->start();
            
            // Create subscriptions
            auto sub1 = bus_->subscribe("test.topic1", 
                [](const bus_message&) -> boost::asio::awaitable<bool> { co_return true; });
            auto sub2 = bus_->subscribe("test.topic2",
                [](const bus_message&) -> boost::asio::awaitable<bool> { co_return true; });
            
            EXPECT_GT(sub1, 0);
            EXPECT_GT(sub2, 0);
            EXPECT_NE(sub1, sub2);
            
            // Test subscription info retrieval
            auto sub_info = bus_->get_subscription(sub1);
            EXPECT_TRUE(sub_info.has_value());
            EXPECT_EQ(sub_info->topic_pattern, "test.topic1");
            
            // Test getting all subscriptions
            auto all_subs = bus_->get_subscriptions();
            EXPECT_GE(all_subs.size(), 2);
            
            // Test unsubscription
            EXPECT_TRUE(bus_->unsubscribe(sub1));
            EXPECT_FALSE(bus_->unsubscribe(sub1)); // Already unsubscribed
            
            // Test topic-based unsubscription
            bus_->subscribe("test.batch1", [](const bus_message&) -> boost::asio::awaitable<bool> { co_return true; });
            bus_->subscribe("test.batch1", [](const bus_message&) -> boost::asio::awaitable<bool> { co_return true; });
            bus_->subscribe("test.batch2", [](const bus_message&) -> boost::asio::awaitable<bool> { co_return true; });
            
            auto removed_count = bus_->unsubscribe_topic("test.batch1");
            EXPECT_EQ(removed_count, 2);
            
            co_await bus_->stop();
        },
        boost::asio::detached
    );
    
    io_context_->run();
}

/**
 * @brief Test queue management and limits
 */
TEST_F(MessageBusTest, QueueManagement) {
    // Use smaller queue for testing
    config_.max_queue_size = 10;
    
    boost::asio::co_spawn(*io_context_,
        [&]() -> boost::asio::awaitable<void> {
            co_await bus_->initialize(config_);
            co_await bus_->start();
            
            // Test initial queue state
            EXPECT_EQ(bus_->get_queue_size(), 0);
            EXPECT_EQ(bus_->get_max_queue_size(), 10);
            
            // Publish messages
            ss::message test_msg({'t', 'e', 's', 't', 0, 0, 0, 0});
            auto endpoint = endpoint("127.0.0.1", 8080);
            
            for (int i = 0; i < 15; ++i) {
                co_await bus_->publish("test.topic", test_msg, endpoint);
            }
            
            // Queue should be limited
            EXPECT_LE(bus_->get_queue_size(), 10);
            
            // Test queue clearing
            auto cleared = bus_->clear_all_queues();
            EXPECT_GT(cleared, 0);
            EXPECT_EQ(bus_->get_queue_size(), 0);
            
            // Test max queue size modification
            bus_->set_max_queue_size(20);
            EXPECT_EQ(bus_->get_max_queue_size(), 20);
            
            co_await bus_->stop();
        },
        boost::asio::detached
    );
    
    io_context_->run();
}

/**
 * @brief Test message priorities
 */
TEST_F(MessageBusTest, MessagePriority) {
    std::vector<std::uint8_t> received_priorities;
    
    boost::asio::co_spawn(*io_context_,
        [&]() -> boost::asio::awaitable<void> {
            co_await bus_->initialize(config_);
            co_await bus_->start();
            
            // Subscribe with minimum priority filter
            bus_->subscribe(
                "test.priority",
                [&](const bus_message& msg) -> boost::asio::awaitable<bool> {
                    received_priorities.push_back(msg.metadata.priority);
                    co_return true;
                },
                {}, {}, 5 // Min priority = 5
            );
            
            ss::message test_msg({'t', 'e', 's', 't', 0, 0, 0, 0});
            auto endpoint = endpoint("127.0.0.1", 8080);
            
            // Publish messages with different priorities
            co_await bus_->publish("test.priority", test_msg, endpoint, 10); // Should receive
            co_await bus_->publish("test.priority", test_msg, endpoint, 3);  // Should not receive
            co_await bus_->publish("test.priority", test_msg, endpoint, 7);  // Should receive
            
            // Wait for processing
            co_await boost::asio::steady_timer(*io_context_, std::chrono::milliseconds(100)).async_wait(boost::asio::use_awaitable);
            
            co_await bus_->stop();
        },
        boost::asio::detached
    );
    
    io_context_->run();
    
    EXPECT_EQ(received_priorities.size(), 2);
    EXPECT_TRUE(std::find(received_priorities.begin(), received_priorities.end(), 10) != received_priorities.end());
    EXPECT_TRUE(std::find(received_priorities.begin(), received_priorities.end(), 7) != received_priorities.end());
}

/**
 * @brief Test statistics collection
 */
TEST_F(MessageBusTest, Statistics) {
    boost::asio::co_spawn(*io_context_,
        [&]() -> boost::asio::awaitable<void> {
            co_await bus_->initialize(config_);
            co_await bus_->start();
            
            // Subscribe to receive messages
            bus_->subscribe("test.stats", 
                [](const bus_message&) -> boost::asio::awaitable<bool> { co_return true; });
            
            ss::message test_msg({'t', 'e', 's', 't', 0, 0, 0, 0});
            auto endpoint = endpoint("127.0.0.1", 8080);
            
            // Publish several messages
            for (int i = 0; i < 5; ++i) {
                co_await bus_->publish("test.stats", test_msg, endpoint);
            }
            
            // Wait for processing
            co_await boost::asio::steady_timer(*io_context_, std::chrono::milliseconds(200)).async_wait(boost::asio::use_awaitable);
            
            auto stats = bus_->get_stats();
            EXPECT_GE(stats.messages_published, 5);
            EXPECT_GE(stats.active_subscriptions, 1);
            
            // Test stats reset
            bus_->reset_stats();
            auto reset_stats = bus_->get_stats();
            EXPECT_EQ(reset_stats.messages_published, 0);
            
            co_await bus_->stop();
        },
        boost::asio::detached
    );
    
    io_context_->run();
}

/**
 * @brief Test configuration management
 */
class MessageBusConfigTest : public ::testing::Test {
protected:
    message_bus_config config_;
};

TEST_F(MessageBusConfigTest, ValidConfiguration) {
    config_.max_queue_size = 1000;
    config_.message_ttl = std::chrono::minutes(5);
    config_.worker_threads = 4;
    
    auto result = config_.validate();
    EXPECT_TRUE(result.is_ok());
}

TEST_F(MessageBusConfigTest, InvalidConfiguration) {
    config_.max_queue_size = 0; // Invalid
    
    auto result = config_.validate();
    EXPECT_FALSE(result.is_ok());
    EXPECT_FALSE(result.error().empty());
}

TEST_F(MessageBusConfigTest, JSONSerialization) {
    config_.max_queue_size = 1000;
    config_.message_ttl = std::chrono::minutes(5);
    config_.enable_persistence = true;
    config_.persistence_path = "/tmp/test.dat";
    
    auto json = config_.to_json();
    EXPECT_TRUE(json.contains("max_queue_size"));
    EXPECT_TRUE(json.contains("message_ttl_ms"));
    EXPECT_TRUE(json.contains("enable_persistence"));
    
    auto config_result = message_bus_config::from_json(json);
    EXPECT_TRUE(config_result.is_ok());
    
    auto loaded_config = config_result.value();
    EXPECT_EQ(loaded_config.max_queue_size, config_.max_queue_size);
    EXPECT_EQ(loaded_config.enable_persistence, config_.enable_persistence);
}

/**
 * @brief Test message bus builder
 */
TEST_F(MessageBusTest, Builder) {
    message_bus_builder builder(*io_context_);
    
    auto built_bus = builder
        .with_max_queue_size(500)
        .with_message_ttl(std::chrono::minutes(10))
        .with_persistence(true, "/tmp/test_bus.dat")
        .with_worker_threads(8)
        .build();
    
    EXPECT_NE(built_bus, nullptr);
    
    auto config = built_bus->get_config();
    EXPECT_EQ(config.max_queue_size, 500);
    EXPECT_EQ(config.message_ttl, std::chrono::minutes(10));
    EXPECT_TRUE(config.enable_persistence);
    EXPECT_EQ(config.worker_threads, 8);
}

/**
 * @brief Test bus message structure
 */
class BusMessageTest : public ::testing::Test {
protected:
    ss::message payload_;
    endpoint sender_;
    
    void SetUp() override {
        payload_ = ss::message({'t', 'e', 's', 't', 0, 0, 0, 0});
        payload_.set_param("content", "test message");
        sender_ = endpoint("127.0.0.1", 8080);
    }
};

TEST_F(BusMessageTest, Construction) {
    bus_message msg("test.topic", payload_, sender_);
    
    EXPECT_EQ(msg.metadata.topic, "test.topic");
    EXPECT_EQ(msg.metadata.sender, sender_);
    EXPECT_NE(msg.metadata.id.value(), 0);
    EXPECT_GT(msg.size(), 0);
}

TEST_F(BusMessageTest, Serialization) {
    bus_message msg("test.topic", payload_, sender_);
    
    auto serialized = msg.serialize();
    EXPECT_GT(serialized.size(), 0);
    
    // Note: Deserialization test would require proper implementation
    // auto deserialized = bus_message::deserialize(serialized);
    // EXPECT_TRUE(deserialized.is_ok());
}

/**
 * @brief Test metadata functionality
 */
class MessageMetadataTest : public ::testing::Test {
protected:
    message_metadata metadata_;
    
    void SetUp() override {
        metadata_.id = message_id::generate();
        metadata_.topic = "test.topic";
        metadata_.sender = endpoint("127.0.0.1", 8080);
        metadata_.timestamp = std::chrono::steady_clock::now();
        metadata_.expires_at = metadata_.timestamp + std::chrono::minutes(5);
        metadata_.priority = 5;
        metadata_.tags = {"test", "important"};
    }
};

TEST_F(MessageMetadataTest, Expiration) {
    EXPECT_FALSE(metadata_.is_expired());
    
    // Set expiration to past
    metadata_.expires_at = std::chrono::steady_clock::now() - std::chrono::minutes(1);
    EXPECT_TRUE(metadata_.is_expired());
}

TEST_F(MessageMetadataTest, Retry) {
    EXPECT_TRUE(metadata_.should_retry());
    
    // Exceed max attempts
    metadata_.delivery_attempts = metadata_.max_delivery_attempts;
    EXPECT_FALSE(metadata_.should_retry());
    
    // Reset attempts but expire message
    metadata_.delivery_attempts = 0;
    metadata_.expires_at = std::chrono::steady_clock::now() - std::chrono::minutes(1);
    EXPECT_FALSE(metadata_.should_retry());
}

TEST_F(MessageMetadataTest, JSONSerialization) {
    auto json = metadata_.to_json();
    
    EXPECT_TRUE(json.contains("id"));
    EXPECT_TRUE(json.contains("topic"));
    EXPECT_TRUE(json.contains("priority"));
    EXPECT_TRUE(json.contains("tags"));
    
    EXPECT_EQ(json["topic"].get<std::string>(), metadata_.topic);
    EXPECT_EQ(json["priority"].get<std::uint8_t>(), metadata_.priority);
    EXPECT_TRUE(json["tags"].is_array());
    EXPECT_EQ(json["tags"].size(), metadata_.tags.size());
}