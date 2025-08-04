#include <gtest/gtest.h>
#include "../../../include/ss_p2p/dht/kademlia_config.hpp"

#include <chrono>

namespace ss::dht::test {

/**
 * @brief Test fixture for Kademlia configuration
 */
class KademliaConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        default_config_ = kademlia_config{};
        testing_config_ = kademlia_config::testing_config();
        production_config_ = kademlia_config::production_config();
        mobile_config_ = kademlia_config::mobile_config();
    }

    kademlia_config default_config_;
    kademlia_config testing_config_;
    kademlia_config production_config_;
    kademlia_config mobile_config_;
};

/**
 * @brief Test default configuration values
 */
TEST_F(KademliaConfigTest, DefaultConfiguration) {
    // Test basic Kademlia parameters
    EXPECT_EQ(default_config_.k_bucket_size, 20u);
    EXPECT_EQ(default_config_.alpha, 3u);
    EXPECT_EQ(default_config_.beta, 20u);
    EXPECT_EQ(default_config_.node_id_bits, 160u);
    
    // Test timeout settings
    EXPECT_EQ(default_config_.rpc_timeout, std::chrono::milliseconds{5000});
    EXPECT_EQ(default_config_.find_timeout, std::chrono::milliseconds{30000});
    EXPECT_EQ(default_config_.maintenance_interval, std::chrono::minutes{10});
    EXPECT_EQ(default_config_.bucket_refresh_interval, std::chrono::minutes{60});
    EXPECT_EQ(default_config_.node_timeout, std::chrono::minutes{15});
    
    // Test failure handling
    EXPECT_EQ(default_config_.max_failures, 3u);
    EXPECT_EQ(default_config_.max_retries, 2u);
    EXPECT_EQ(default_config_.initial_retry_delay, std::chrono::milliseconds{100});
    EXPECT_EQ(default_config_.max_retry_delay, std::chrono::milliseconds{5000});
    
    // Test storage settings
    EXPECT_EQ(default_config_.replication_factor, 3u);
    EXPECT_EQ(default_config_.max_stored_values, 100000u);
    EXPECT_EQ(default_config_.max_storage_bytes, 1024u * 1024u * 100u);
    
    // Test boolean flags
    EXPECT_TRUE(default_config_.enable_bucket_splitting);
    EXPECT_TRUE(default_config_.enable_lru_eviction);
    EXPECT_TRUE(default_config_.prefer_low_latency);
    EXPECT_TRUE(default_config_.enable_statistics);
    EXPECT_FALSE(default_config_.enable_debug_logging);
}

/**
 * @brief Test configuration validation
 */
TEST_F(KademliaConfigTest, ConfigurationValidation) {
    // Default config should be valid
    EXPECT_TRUE(default_config_.validate());
    EXPECT_TRUE(testing_config_.validate());
    EXPECT_TRUE(production_config_.validate());
    EXPECT_TRUE(mobile_config_.validate());
    
    // Test invalid configurations
    auto invalid_config = default_config_;
    
    // Invalid k_bucket_size
    invalid_config.k_bucket_size = 0;
    EXPECT_FALSE(invalid_config.validate());
    
    invalid_config = default_config_;
    invalid_config.k_bucket_size = 300;
    EXPECT_FALSE(invalid_config.validate());
    
    // Invalid alpha
    invalid_config = default_config_;
    invalid_config.alpha = 0;
    EXPECT_FALSE(invalid_config.validate());
    
    invalid_config = default_config_;
    invalid_config.alpha = 50; // Greater than k_bucket_size
    EXPECT_FALSE(invalid_config.validate());
    
    // Invalid timeouts
    invalid_config = default_config_;
    invalid_config.rpc_timeout = std::chrono::milliseconds{0};
    EXPECT_FALSE(invalid_config.validate());
    
    invalid_config = default_config_;
    invalid_config.find_timeout = std::chrono::milliseconds{1000}; // Less than rpc_timeout
    EXPECT_FALSE(invalid_config.validate());
    
    // Invalid storage settings
    invalid_config = default_config_;
    invalid_config.replication_factor = 0;
    EXPECT_FALSE(invalid_config.validate());
    
    invalid_config = default_config_;
    invalid_config.replication_factor = 50; // Greater than k_bucket_size
    EXPECT_FALSE(invalid_config.validate());
}

/**
 * @brief Test testing configuration
 */
TEST_F(KademliaConfigTest, TestingConfiguration) {
    // Should have shorter timeouts for faster testing
    EXPECT_LT(testing_config_.rpc_timeout, default_config_.rpc_timeout);
    EXPECT_LT(testing_config_.find_timeout, default_config_.find_timeout);
    EXPECT_LT(testing_config_.maintenance_interval, default_config_.maintenance_interval);
    EXPECT_LT(testing_config_.node_timeout, default_config_.node_timeout);
    
    // Should have smaller limits for resource conservation
    EXPECT_LE(testing_config_.k_bucket_size, default_config_.k_bucket_size);
    EXPECT_LE(testing_config_.max_stored_values, default_config_.max_stored_values);
    EXPECT_LE(testing_config_.max_storage_bytes, default_config_.max_storage_bytes);
    
    // Should enable debugging and be more aggressive
    EXPECT_TRUE(testing_config_.enable_debug_logging);
    EXPECT_TRUE(testing_config_.enable_statistics);
    EXPECT_TRUE(testing_config_.enable_aggressive_bootstrap);
    EXPECT_LT(testing_config_.min_bootstrap_nodes, default_config_.min_bootstrap_nodes);
    
    // Should still be valid
    EXPECT_TRUE(testing_config_.validate());
}

/**
 * @brief Test production configuration
 */
TEST_F(KademliaConfigTest, ProductionConfiguration) {
    // Should have conservative timeouts
    EXPECT_GE(production_config_.rpc_timeout, default_config_.rpc_timeout);
    EXPECT_GE(production_config_.find_timeout, default_config_.find_timeout);
    EXPECT_GE(production_config_.node_timeout, default_config_.node_timeout);
    
    // Should have larger limits for production scale
    EXPECT_GE(production_config_.max_stored_values, default_config_.max_stored_values);
    EXPECT_GE(production_config_.max_storage_bytes, default_config_.max_storage_bytes);
    
    // Should enable security features
    EXPECT_TRUE(production_config_.enable_sybil_protection);
    EXPECT_LE(production_config_.max_requests_per_second, 100u);
    
    // Should have performance optimizations
    EXPECT_GE(production_config_.io_threads, default_config_.io_threads);
    EXPECT_TRUE(production_config_.enable_routing_optimization);
    EXPECT_TRUE(production_config_.remove_high_latency_nodes);
    
    // Should disable debug features
    EXPECT_FALSE(production_config_.enable_debug_logging);
    
    // Should still be valid
    EXPECT_TRUE(production_config_.validate());
}

/**
 * @brief Test mobile configuration
 */
TEST_F(KademliaConfigTest, MobileConfiguration) {
    // Should have minimal resource usage
    EXPECT_LE(mobile_config_.k_bucket_size, default_config_.k_bucket_size);
    EXPECT_LE(mobile_config_.max_stored_values, default_config_.max_stored_values);
    EXPECT_LE(mobile_config_.max_storage_bytes, default_config_.max_storage_bytes);
    
    // Should have longer intervals to save battery
    EXPECT_GE(mobile_config_.maintenance_interval, default_config_.maintenance_interval);
    EXPECT_GE(mobile_config_.bucket_refresh_interval, default_config_.bucket_refresh_interval);
    
    // Should be conservative with network usage
    EXPECT_EQ(mobile_config_.io_threads, 1u);
    EXPECT_LE(mobile_config_.buffer_size, default_config_.buffer_size);
    EXPECT_LE(mobile_config_.max_requests_per_second, 20u);
    
    // Should disable optional features
    EXPECT_FALSE(mobile_config_.enable_statistics);
    EXPECT_FALSE(mobile_config_.enable_debug_logging);
    EXPECT_FALSE(mobile_config_.enable_routing_optimization);
    EXPECT_FALSE(mobile_config_.ping_random_nodes);
    
    // Should still be valid
    EXPECT_TRUE(mobile_config_.validate());
}

/**
 * @brief Test result code to string conversion
 */
TEST_F(KademliaConfigTest, ResultCodeToString) {
    EXPECT_STREQ(to_string(dht_result_code::success), "success");
    EXPECT_STREQ(to_string(dht_result_code::timeout), "timeout");
    EXPECT_STREQ(to_string(dht_result_code::not_found), "not_found");
    EXPECT_STREQ(to_string(dht_result_code::network_error), "network_error");
    EXPECT_STREQ(to_string(dht_result_code::invalid_parameters), "invalid_parameters");
    EXPECT_STREQ(to_string(dht_result_code::storage_full), "storage_full");
    EXPECT_STREQ(to_string(dht_result_code::permission_denied), "permission_denied");
    EXPECT_STREQ(to_string(dht_result_code::duplicate), "duplicate");
    EXPECT_STREQ(to_string(dht_result_code::cancelled), "cancelled");
    EXPECT_STREQ(to_string(dht_result_code::internal_error), "internal_error");
    EXPECT_STREQ(to_string(dht_result_code::unreachable), "unreachable");
    EXPECT_STREQ(to_string(dht_result_code::rate_limited), "rate_limited");
    EXPECT_STREQ(to_string(dht_result_code::invalid_signature), "invalid_signature");
    EXPECT_STREQ(to_string(dht_result_code::unsupported), "unsupported");
    
    // Test unknown/invalid code
    auto invalid_code = static_cast<dht_result_code>(999);
    EXPECT_STREQ(to_string(invalid_code), "unknown");
}

/**
 * @brief Test configuration parameter bounds
 */
TEST_F(KademliaConfigTest, ParameterBounds) {
    auto config = default_config_;
    
    // Test bucket size bounds
    config.k_bucket_size = 1;
    EXPECT_TRUE(config.validate());
    
    config.k_bucket_size = 255;
    EXPECT_TRUE(config.validate());
    
    config.k_bucket_size = 256;
    EXPECT_FALSE(config.validate());
    
    // Test alpha bounds
    config = default_config_;
    config.alpha = 1;
    EXPECT_TRUE(config.validate());
    
    config.alpha = config.k_bucket_size;
    EXPECT_TRUE(config.validate());
    
    config.alpha = config.k_bucket_size + 1;
    EXPECT_FALSE(config.validate());
    
    // Test node ID bits bounds
    config = default_config_;
    config.node_id_bits = 1;
    EXPECT_TRUE(config.validate());
    
    config.node_id_bits = 512;
    EXPECT_TRUE(config.validate());
    
    config.node_id_bits = 513;
    EXPECT_FALSE(config.validate());
    
    // Test thread bounds
    config = default_config_;
    config.io_threads = 1;
    EXPECT_TRUE(config.validate());
    
    config.io_threads = 64;
    EXPECT_TRUE(config.validate());
    
    config.io_threads = 65;
    EXPECT_FALSE(config.validate());
}

/**
 * @brief Test timeout relationships
 */
TEST_F(KademliaConfigTest, TimeoutRelationships) {
    auto config = default_config_;
    
    // find_timeout should be greater than rpc_timeout
    config.rpc_timeout = std::chrono::milliseconds{10000};
    config.find_timeout = std::chrono::milliseconds{5000};
    EXPECT_FALSE(config.validate());
    
    config.find_timeout = std::chrono::milliseconds{15000};
    EXPECT_TRUE(config.validate());
    
    // All timeouts should be positive
    config.rpc_timeout = std::chrono::milliseconds{0};
    EXPECT_FALSE(config.validate());
    
    config.rpc_timeout = std::chrono::milliseconds{-1000};
    EXPECT_FALSE(config.validate());
    
    config = default_config_;
    config.maintenance_interval = std::chrono::minutes{0};
    EXPECT_FALSE(config.validate());
}

/**
 * @brief Test bucket split configuration
 */
TEST_F(KademliaConfigTest, BucketSplitConfiguration) {
    auto config = default_config_;
    
    // bucket_split_threshold should be greater than k_bucket_size
    config.k_bucket_size = 20;
    config.bucket_split_threshold = 19;
    EXPECT_FALSE(config.validate());
    
    config.bucket_split_threshold = 20;
    EXPECT_FALSE(config.validate());
    
    config.bucket_split_threshold = 21;
    EXPECT_TRUE(config.validate());
    
    // max_bucket_depth should be reasonable
    config.max_bucket_depth = 0;
    EXPECT_FALSE(config.validate());
    
    config.max_bucket_depth = 21;
    EXPECT_FALSE(config.validate());
    
    config.max_bucket_depth = 10;
    EXPECT_TRUE(config.validate());
}

/**
 * @brief Test replication factor validation
 */
TEST_F(KademliaConfigTest, ReplicationFactorValidation) {
    auto config = default_config_;
    
    // Replication factor should be positive
    config.replication_factor = 0;
    EXPECT_FALSE(config.validate());
    
    // Should not exceed k_bucket_size
    config.replication_factor = config.k_bucket_size + 1;
    EXPECT_FALSE(config.validate());
    
    config.replication_factor = config.k_bucket_size;
    EXPECT_TRUE(config.validate());
}

/**
 * @brief Test storage size validation
 */
TEST_F(KademliaConfigTest, StorageSizeValidation) {
    auto config = default_config_;
    
    // Storage sizes should be positive
    config.max_storage_bytes = 0;
    EXPECT_FALSE(config.validate());
    
    config = default_config_;
    config.max_stored_values = 0;
    EXPECT_TRUE(config.validate()); // 0 stored values might be valid for some use cases
    
    // Buffer size should be reasonable
    config = default_config_;
    config.buffer_size = 512; // Too small
    EXPECT_FALSE(config.validate());
    
    config.buffer_size = 1024;
    EXPECT_TRUE(config.validate());
}

/**
 * @brief Test retry configuration
 */
TEST_F(KademliaConfigTest, RetryConfiguration) {
    auto config = default_config_;
    
    // max_failures should be positive
    config.max_failures = 0;
    EXPECT_FALSE(config.validate());
    
    config.max_failures = 1;
    EXPECT_TRUE(config.validate());
    
    // max_retries should be reasonable
    config.max_retries = 11; // Too many
    EXPECT_FALSE(config.validate());
    
    config.max_retries = 10;
    EXPECT_TRUE(config.validate());
}

/**
 * @brief Test configuration copying and assignment
 */
TEST_F(KademliaConfigTest, ConfigurationCopyAndAssignment) {
    // Test copy constructor
    auto copied_config = default_config_;
    EXPECT_EQ(copied_config.k_bucket_size, default_config_.k_bucket_size);
    EXPECT_EQ(copied_config.alpha, default_config_.alpha);
    EXPECT_EQ(copied_config.rpc_timeout, default_config_.rpc_timeout);
    
    // Test assignment
    auto assigned_config = testing_config_;
    assigned_config = production_config_;
    EXPECT_EQ(assigned_config.k_bucket_size, production_config_.k_bucket_size);
    EXPECT_EQ(assigned_config.enable_sybil_protection, production_config_.enable_sybil_protection);
    
    // Test move semantics
    auto moved_config = std::move(assigned_config);
    EXPECT_EQ(moved_config.k_bucket_size, production_config_.k_bucket_size);
}

/**
 * @brief Test specialized configurations have expected characteristics
 */
TEST_F(KademliaConfigTest, SpecializedConfigurationCharacteristics) {
    // Testing config should be faster and smaller
    EXPECT_TRUE(testing_config_.enable_aggressive_bootstrap);
    EXPECT_LT(testing_config_.bootstrap_timeout, default_config_.bootstrap_timeout);
    
    // Production config should be more secure and stable
    EXPECT_TRUE(production_config_.enable_sybil_protection);
    EXPECT_GE(production_config_.rpc_timeout, default_config_.rpc_timeout);
    
    // Mobile config should be resource-efficient
    EXPECT_LE(mobile_config_.alpha, default_config_.alpha);
    EXPECT_LE(mobile_config_.max_requests_per_second, default_config_.max_requests_per_second);
    EXPECT_FALSE(mobile_config_.enable_statistics);
    
    // All should have compatible alpha/k relationships
    EXPECT_LE(testing_config_.alpha, testing_config_.k_bucket_size);
    EXPECT_LE(production_config_.alpha, production_config_.k_bucket_size);
    EXPECT_LE(mobile_config_.alpha, mobile_config_.k_bucket_size);
}

} // namespace ss::dht::test