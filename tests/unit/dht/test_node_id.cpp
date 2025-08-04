#include <gtest/gtest.h>
#include "../../../include/ss_p2p/dht/node_id.hpp"
#include "../../../include/ss_p2p/core/types.hpp"

#include <random>
#include <set>
#include <algorithm>

namespace ss::dht::test {

/**
 * @brief Test fixture for node_id utility functions
 */
class NodeIdUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test node IDs with known bit patterns
        zero_id_ = node_id{}; // All zeros
        max_id_ = node_id::from_hex("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
        
        // Node ID with alternating bit pattern
        alt_id_ = node_id_utils::from_bit_pattern("1010101010101010");
        
        // Random node IDs for testing
        std::random_device rd;
        std::mt19937 gen(rd());
        for (int i = 0; i < 10; ++i) {
            random_ids_.push_back(node_id::random());
        }
    }

    node_id zero_id_;
    node_id max_id_;
    node_id alt_id_;
    std::vector<node_id> random_ids_;
};

/**
 * @brief Test basic node_id operations from core module
 */
TEST_F(NodeIdUtilsTest, BasicNodeIdOperations) {
    // Test equality
    EXPECT_EQ(zero_id_, zero_id_);
    EXPECT_NE(zero_id_, max_id_);
    
    // Test copy construction
    auto zero_copy = zero_id_;
    EXPECT_EQ(zero_id_, zero_copy);
    
    // Test move construction
    auto zero_moved = std::move(zero_copy);
    EXPECT_EQ(zero_id_, zero_moved);
    
    // Test hex conversion
    auto hex_str = max_id_.to_hex();
    EXPECT_EQ(hex_str, "ffffffffffffffffffffffffffffffffffffffff");
    
    auto from_hex = node_id::from_hex(hex_str);
    EXPECT_EQ(max_id_, from_hex);
}

/**
 * @brief Test XOR distance calculation
 */
TEST_F(NodeIdUtilsTest, XorDistanceCalculation) {
    // Distance to self should be zero
    auto self_distance = zero_id_.distance(zero_id_);
    EXPECT_EQ(self_distance, zero_id_);
    
    // Distance should be symmetric
    auto dist1 = zero_id_.distance(max_id_);
    auto dist2 = max_id_.distance(zero_id_);
    EXPECT_EQ(dist1, dist2);
    
    // Distance from zero to max should be max
    EXPECT_EQ(dist1, max_id_);
    
    // Triangle inequality (approximately - XOR doesn't strictly satisfy this)
    for (size_t i = 0; i < random_ids_.size() - 2; ++i) {
        auto& a = random_ids_[i];
        auto& b = random_ids_[i + 1];
        auto& c = random_ids_[i + 2];
        
        auto ab = a.distance(b);
        auto bc = b.distance(c);
        auto ac = a.distance(c);
        
        // For XOR metric, we can verify that d(a,c) XOR d(a,b) XOR d(b,c) = 0
        auto combined = ac.distance(ab.distance(bc));
        EXPECT_EQ(combined, zero_id_);
    }
}

/**
 * @brief Test bucket index calculation
 */
TEST_F(NodeIdUtilsTest, BucketIndexCalculation) {
    // Distance to self should result in bucket 160 (but clamped to 159)
    auto self_bucket = node_id_utils::bucket_index(zero_id_, zero_id_);
    EXPECT_EQ(self_bucket, 160u); // All 160 bits are the same
    
    // Test known bit patterns
    auto pattern1 = node_id_utils::from_bit_pattern("1000000000000000"); // First bit different
    auto bucket1 = node_id_utils::bucket_index(zero_id_, pattern1);
    EXPECT_EQ(bucket1, 0u); // First bit differs
    
    auto pattern2 = node_id_utils::from_bit_pattern("0100000000000000"); // Second bit different
    auto bucket2 = node_id_utils::bucket_index(zero_id_, pattern2);
    EXPECT_EQ(bucket2, 1u); // Second bit differs
    
    auto pattern3 = node_id_utils::from_bit_pattern("0010000000000000"); // Third bit different
    auto bucket3 = node_id_utils::bucket_index(zero_id_, pattern3);
    EXPECT_EQ(bucket3, 2u); // Third bit differs
}

/**
 * @brief Test leading zero bits calculation
 */
TEST_F(NodeIdUtilsTest, LeadingZeroBits) {
    // All zeros should have 160 leading zero bits
    auto zero_bits = node_id_utils::leading_zero_bits(zero_id_);
    EXPECT_EQ(zero_bits, 160u);
    
    // Max value should have 0 leading zero bits
    auto max_bits = node_id_utils::leading_zero_bits(max_id_);
    EXPECT_EQ(max_bits, 0u);
    
    // Test specific patterns
    auto one_bit = node_id_utils::from_bit_pattern("1");
    auto one_leading = node_id_utils::leading_zero_bits(one_bit);
    EXPECT_EQ(one_leading, 0u);
    
    auto second_bit = node_id_utils::from_bit_pattern("01");
    auto second_leading = node_id_utils::leading_zero_bits(second_bit);
    EXPECT_EQ(second_leading, 1u);
    
    auto eighth_bit = node_id_utils::from_bit_pattern("00000001");
    auto eighth_leading = node_id_utils::leading_zero_bits(eighth_bit);
    EXPECT_EQ(eighth_leading, 7u);
}

/**
 * @brief Test common prefix length calculation
 */
TEST_F(NodeIdUtilsTest, CommonPrefixLength) {
    // Identical IDs should have 160 common prefix bits
    auto identical_prefix = node_id_utils::common_prefix_length(zero_id_, zero_id_);
    EXPECT_EQ(identical_prefix, 160u);
    
    // Test specific patterns
    auto pattern1 = node_id_utils::from_bit_pattern("10101010");
    auto pattern2 = node_id_utils::from_bit_pattern("10101011");
    auto common = node_id_utils::common_prefix_length(pattern1, pattern2);
    EXPECT_EQ(common, 7u); // First 7 bits are common, 8th bit differs
    
    auto pattern3 = node_id_utils::from_bit_pattern("11111111");
    auto pattern4 = node_id_utils::from_bit_pattern("01111111");
    auto common2 = node_id_utils::common_prefix_length(pattern3, pattern4);
    EXPECT_EQ(common2, 0u); // First bit differs
}

/**
 * @brief Test distance comparison
 */
TEST_F(NodeIdUtilsTest, DistanceComparison) {
    auto target = node_id::random();
    auto close1 = target;
    auto close2 = target;
    
    // Modify one bit in close1 and two bits in close2
    node_id_utils::set_bit(close1, 159, 1 - node_id_utils::get_bit(close1, 159));
    node_id_utils::set_bit(close2, 158, 1 - node_id_utils::get_bit(close2, 158));
    node_id_utils::set_bit(close2, 159, 1 - node_id_utils::get_bit(close2, 159));
    
    // close1 should be closer to target than close2
    EXPECT_TRUE(node_id_utils::is_closer(target, close1, close2));
    EXPECT_FALSE(node_id_utils::is_closer(target, close2, close1));
}

/**
 * @brief Test random ID generation in specific bucket
 */
TEST_F(NodeIdUtilsTest, RandomInBucket) {
    auto local_id = node_id::random();
    
    for (std::uint32_t bucket = 0; bucket < 10; ++bucket) {
        auto random_in_bucket = node_id_utils::random_in_bucket(local_id, bucket);
        auto calculated_bucket = node_id_utils::bucket_index(local_id, random_in_bucket);
        EXPECT_EQ(calculated_bucket, bucket);
    }
    
    // Test edge cases
    EXPECT_THROW(node_id_utils::random_in_bucket(local_id, 160), std::invalid_argument);
    EXPECT_THROW(node_id_utils::random_in_bucket(local_id, 200), std::invalid_argument);
}

/**
 * @brief Test bit manipulation functions
 */
TEST_F(NodeIdUtilsTest, BitManipulation) {
    auto test_id = zero_id_;
    
    // Test setting bits
    for (std::uint32_t i = 0; i < 160; i += 10) {
        node_id_utils::set_bit(test_id, i, 1);
        EXPECT_EQ(node_id_utils::get_bit(test_id, i), 1u);
    }
    
    // Test clearing bits
    for (std::uint32_t i = 0; i < 160; i += 10) {
        node_id_utils::set_bit(test_id, i, 0);
        EXPECT_EQ(node_id_utils::get_bit(test_id, i), 0u);
    }
    
    // Test out of bounds (should not crash)
    node_id_utils::set_bit(test_id, 200, 1);
    EXPECT_EQ(node_id_utils::get_bit(test_id, 200), 0u);
}

/**
 * @brief Test bit string conversion
 */
TEST_F(NodeIdUtilsTest, BitStringConversion) {
    // Test known patterns
    auto pattern = node_id_utils::from_bit_pattern("10101010");
    auto bit_string = node_id_utils::to_bit_string(pattern, 8);
    EXPECT_EQ(bit_string, "10101010");
    
    // Test full length
    auto full_string = node_id_utils::to_bit_string(zero_id_, 160);
    EXPECT_EQ(full_string.length(), 160u);
    EXPECT_EQ(full_string, std::string(160, '0'));
    
    auto max_string = node_id_utils::to_bit_string(max_id_, 160);
    EXPECT_EQ(max_string.length(), 160u);
    EXPECT_EQ(max_string, std::string(160, '1'));
}

/**
 * @brief Test distance metric calculation
 */
TEST_F(NodeIdUtilsTest, DistanceMetric) {
    // Distance to self should be 0
    auto self_metric = node_id_utils::distance_metric(zero_id_, zero_id_);
    EXPECT_EQ(self_metric, 0u);
    
    // Distance should be symmetric
    auto metric1 = node_id_utils::distance_metric(zero_id_, max_id_);
    auto metric2 = node_id_utils::distance_metric(max_id_, zero_id_);
    EXPECT_EQ(metric1, metric2);
    
    // Test ordering consistency
    std::vector<node_id> test_ids = {zero_id_, max_id_, alt_id_};
    test_ids.insert(test_ids.end(), random_ids_.begin(), random_ids_.end());
    
    auto target = node_id::random();
    
    // Sort by distance comparator
    std::sort(test_ids.begin(), test_ids.end(), distance_comparator(target));
    
    // Verify order using distance metric
    for (size_t i = 1; i < test_ids.size(); ++i) {
        auto metric_prev = node_id_utils::distance_metric(target, test_ids[i-1]);
        auto metric_curr = node_id_utils::distance_metric(target, test_ids[i]);
        EXPECT_LE(metric_prev, metric_curr);
    }
}

/**
 * @brief Test generate at distance function
 */
TEST_F(NodeIdUtilsTest, GenerateAtDistance) {
    auto target = node_id::random();
    
    for (std::uint32_t distance = 0; distance < 10; ++distance) {
        auto generated = node_id_utils::generate_at_distance(target, distance);
        auto calculated_bucket = node_id_utils::bucket_index(target, generated);
        EXPECT_EQ(calculated_bucket, distance);
    }
    
    // Test edge case
    auto max_distance = node_id_utils::generate_at_distance(target, 159);
    auto max_bucket = node_id_utils::bucket_index(target, max_distance);
    EXPECT_EQ(max_bucket, 159u);
}

/**
 * @brief Test bucket membership checking
 */
TEST_F(NodeIdUtilsTest, BucketMembership) {
    auto local_id = node_id::random();
    
    for (std::uint32_t bucket = 0; bucket < 10; ++bucket) {
        auto id_in_bucket = node_id_utils::random_in_bucket(local_id, bucket);
        EXPECT_TRUE(node_id_utils::is_in_bucket(local_id, id_in_bucket, bucket));
        
        // Should not be in other buckets (with high probability)
        for (std::uint32_t other_bucket = 0; other_bucket < 10; ++other_bucket) {
            if (other_bucket != bucket) {
                EXPECT_FALSE(node_id_utils::is_in_bucket(local_id, id_in_bucket, other_bucket));
            }
        }
    }
}

/**
 * @brief Test distance comparator class
 */
TEST_F(NodeIdUtilsTest, DistanceComparatorClass) {
    auto target = node_id::random();
    distance_comparator comp(target);
    
    // Generate test nodes at different distances
    std::vector<node_id> test_nodes;
    for (std::uint32_t i = 0; i < 10; ++i) {
        test_nodes.push_back(node_id_utils::generate_at_distance(target, i));
    }
    
    // Add some random nodes
    for (int i = 0; i < 5; ++i) {
        test_nodes.push_back(node_id::random());
    }
    
    // Sort using comparator
    std::sort(test_nodes.begin(), test_nodes.end(), comp);
    
    // Verify ordering
    for (size_t i = 1; i < test_nodes.size(); ++i) {
        EXPECT_FALSE(comp(test_nodes[i], test_nodes[i-1]));
    }
}

/**
 * @brief Test hash function for node_id
 */
TEST_F(NodeIdUtilsTest, HashFunction) {
    std::hash<node_id> hasher;
    
    // Same node ID should produce same hash
    auto hash1 = hasher(zero_id_);
    auto hash2 = hasher(zero_id_);
    EXPECT_EQ(hash1, hash2);
    
    // Different node IDs should produce different hashes (with high probability)
    auto hash3 = hasher(max_id_);
    EXPECT_NE(hash1, hash3);
    
    // Test hash distribution with set
    std::set<std::size_t> hash_values;
    for (const auto& id : random_ids_) {
        hash_values.insert(hasher(id));
    }
    
    // Should have good distribution (most hashes different)
    EXPECT_GE(hash_values.size(), random_ids_.size() * 0.8);
}

/**
 * @brief Performance test for node_id operations
 */
TEST_F(NodeIdUtilsTest, PerformanceTest) {
    const int num_operations = 10000;
    
    // Test distance calculation performance
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i) {
        auto dist = random_ids_[i % random_ids_.size()].distance(
            random_ids_[(i + 1) % random_ids_.size()]);
        (void)dist; // Suppress unused variable warning
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Should complete in reasonable time (less than 1ms per operation)
    EXPECT_LT(duration.count(), num_operations * 1000);
    
    // Test bucket index calculation performance
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i) {
        auto bucket = node_id_utils::bucket_index(
            random_ids_[i % random_ids_.size()],
            random_ids_[(i + 1) % random_ids_.size()]);
        (void)bucket;
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    EXPECT_LT(duration.count(), num_operations * 1000);
}

} // namespace ss::dht::test