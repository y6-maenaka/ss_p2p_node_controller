#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ss_p2p/kademlia/k_routing_table.hpp"
#include "ss_p2p/kademlia/k_bucket.hpp"
#include "ss_p2p/kademlia/k_node.hpp"
#include "ss_p2p/core/types.hpp"

#include <algorithm>
#include <random>
#include <unordered_set>
#include <chrono>

using namespace ss::kademlia;
using namespace ss::core;

namespace ss::kademlia::test {

class KademliaRoutingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a local node ID for testing
        local_id_ = node_id::random();
        
        // Create routing table
        routing_table_ = std::make_unique<k_routing_table>(local_id_);
        
        // Generate test nodes at various distances
        generateTestNodes();
    }
    
    void TearDown() override {
        routing_table_.reset();
    }
    
    void generateTestNodes() {
        std::random_device rd;
        std::mt19937 gen(rd());
        
        // Generate nodes at specific bucket distances
        for (int bucket = 0; bucket < 10; ++bucket) {
            for (int i = 0; i < 5; ++i) {
                auto node_id = generateNodeAtDistance(local_id_, bucket);
                endpoint ep("192.168.1." + std::to_string((bucket * 5 + i) % 255 + 1), 
                           8000 + bucket * 5 + i);
                
                k_node node(node_id, ep);
                test_nodes_.push_back(node);
            }
        }
        
        // Generate some random nodes
        for (int i = 0; i < 20; ++i) {
            auto node_id = node_id::random();
            endpoint ep("10.0.0." + std::to_string(i + 1), 9000 + i);
            k_node node(node_id, ep);
            random_nodes_.push_back(node);
        }
    }
    
    node_id generateNodeAtDistance(const node_id& target, int distance) {
        node_id result = target;
        auto& data = result.data();
        
        // Flip the bit at the specified distance
        int byte_index = distance / 8;
        int bit_index = distance % 8;
        
        if (byte_index < static_cast<int>(data.size())) {
            data[byte_index] ^= (1 << (7 - bit_index));
        }
        
        // Randomize remaining bits to ensure uniqueness
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint8_t> dis(0, 255);
        
        for (int i = byte_index + 1; i < static_cast<int>(data.size()); ++i) {
            data[i] = dis(gen);
        }
        
        return result;
    }
    
    int calculateBucketIndex(const node_id& target, const node_id& node) {
        auto distance = target.distance(node);
        const auto& data = distance.data();
        
        for (int i = 0; i < static_cast<int>(data.size()); ++i) {
            if (data[i] != 0) {
                for (int bit = 7; bit >= 0; --bit) {
                    if (data[i] & (1 << bit)) {
                        return i * 8 + (7 - bit);
                    }
                }
            }
        }
        return 160; // All bits are same (should not happen for different nodes)
    }
    
    node_id local_id_;
    std::unique_ptr<k_routing_table> routing_table_;
    std::vector<k_node> test_nodes_;
    std::vector<k_node> random_nodes_;
};

// Basic routing table operations
TEST_F(KademliaRoutingTest, BasicTableOperations) {
    EXPECT_EQ(routing_table_->local_id(), local_id_);
    EXPECT_EQ(routing_table_->size(), 0);
    EXPECT_TRUE(routing_table_->empty());
    
    // Add a node
    auto& first_node = test_nodes_[0];
    bool added = routing_table_->add_node(first_node);
    EXPECT_TRUE(added);
    EXPECT_EQ(routing_table_->size(), 1);
    EXPECT_FALSE(routing_table_->empty());
    
    // Try to add the same node again
    bool added_again = routing_table_->add_node(first_node);
    EXPECT_FALSE(added_again); // Should update existing node
    EXPECT_EQ(routing_table_->size(), 1);
}

TEST_F(KademliaRoutingTest, NodeLookup) {
    // Add several nodes
    for (size_t i = 0; i < 10; ++i) {
        routing_table_->add_node(test_nodes_[i]);
    }
    
    // Test finding existing node
    auto found_node = routing_table_->find_node(test_nodes_[5].id());
    EXPECT_TRUE(found_node.has_value());
    EXPECT_EQ(found_node->id(), test_nodes_[5].id());
    EXPECT_EQ(found_node->endpoint(), test_nodes_[5].endpoint());
    
    // Test finding non-existing node
    auto missing_node = routing_table_->find_node(node_id::random());
    EXPECT_FALSE(missing_node.has_value());
}

TEST_F(KademliaRoutingTest, NodeRemoval) {
    // Add nodes
    for (size_t i = 0; i < 5; ++i) {
        routing_table_->add_node(test_nodes_[i]);
    }
    EXPECT_EQ(routing_table_->size(), 5);
    
    // Remove a node
    bool removed = routing_table_->remove_node(test_nodes_[2].id());
    EXPECT_TRUE(removed);
    EXPECT_EQ(routing_table_->size(), 4);
    
    // Verify node is gone
    auto found = routing_table_->find_node(test_nodes_[2].id());
    EXPECT_FALSE(found.has_value());
    
    // Try to remove non-existing node
    bool removed_missing = routing_table_->remove_node(node_id::random());
    EXPECT_FALSE(removed_missing);
    EXPECT_EQ(routing_table_->size(), 4);
}

// Bucket management tests
TEST_F(KademliaRoutingTest, BucketDistribution) {
    // Add nodes and verify they go to correct buckets
    for (const auto& node : test_nodes_) {
        routing_table_->add_node(node);
        
        int expected_bucket = calculateBucketIndex(local_id_, node.id());
        auto bucket_nodes = routing_table_->get_bucket_nodes(expected_bucket);
        
        bool found_in_bucket = false;
        for (const auto& bucket_node : bucket_nodes) {
            if (bucket_node.id() == node.id()) {
                found_in_bucket = true;
                break;
            }
        }
        EXPECT_TRUE(found_in_bucket) << "Node not found in expected bucket " << expected_bucket;
    }
}

TEST_F(KademliaRoutingTest, BucketCapacityManagement) {
    const int k_bucket_size = 20; // Typical Kademlia bucket size
    
    // Generate many nodes for the same bucket
    std::vector<k_node> same_bucket_nodes;
    for (int i = 0; i < k_bucket_size + 10; ++i) {
        auto node_id = generateNodeAtDistance(local_id_, 0); // All in bucket 0
        endpoint ep("172.16.0." + std::to_string(i + 1), 8000 + i);
        same_bucket_nodes.emplace_back(node_id, ep);
    }
    
    // Add nodes to routing table
    for (const auto& node : same_bucket_nodes) {
        routing_table_->add_node(node);
    }
    
    // Check bucket size constraint
    auto bucket_0_nodes = routing_table_->get_bucket_nodes(0);
    EXPECT_LE(bucket_0_nodes.size(), k_bucket_size);
    
    // Verify LRU behavior - most recently added/updated nodes should be kept
    // This depends on the specific implementation of bucket management
}

// Closest nodes finding tests
TEST_F(KademliaRoutingTest, FindClosestNodes) {
    // Add all test nodes
    for (const auto& node : test_nodes_) {
        routing_table_->add_node(node);
    }
    for (const auto& node : random_nodes_) {
        routing_table_->add_node(node);
    }
    
    const int k = 8; // Number of closest nodes to find
    auto target = node_id::random();
    auto closest_nodes = routing_table_->find_closest_nodes(target, k);
    
    EXPECT_LE(closest_nodes.size(), k);
    EXPECT_GT(closest_nodes.size(), 0);
    
    // Verify nodes are sorted by distance to target
    for (size_t i = 1; i < closest_nodes.size(); ++i) {
        auto dist_prev = target.distance(closest_nodes[i-1].id());
        auto dist_curr = target.distance(closest_nodes[i].id());
        EXPECT_LE(dist_prev, dist_curr) << "Nodes not sorted by distance";
    }
    
    // Verify these are indeed the closest nodes
    std::vector<k_node> all_nodes = test_nodes_;
    all_nodes.insert(all_nodes.end(), random_nodes_.begin(), random_nodes_.end());
    
    std::sort(all_nodes.begin(), all_nodes.end(), 
        [&target](const k_node& a, const k_node& b) {
            return target.distance(a.id()) < target.distance(b.id());
        });
    
    size_t min_size = std::min(closest_nodes.size(), static_cast<size_t>(k));
    for (size_t i = 0; i < min_size; ++i) {
        EXPECT_EQ(closest_nodes[i].id(), all_nodes[i].id());
    }
}

TEST_F(KademliaRoutingTest, FindClosestNodesWithSelf) {
    // Add nodes
    for (size_t i = 0; i < 10; ++i) {
        routing_table_->add_node(test_nodes_[i]);
    }
    
    // Find closest nodes to our own ID
    auto closest_to_self = routing_table_->find_closest_nodes(local_id_, 5);
    
    // Should not include self in results
    for (const auto& node : closest_to_self) {
        EXPECT_NE(node.id(), local_id_);
    }
}

// Node freshness and activity tracking
TEST_F(KademliaRoutingTest, NodeFreshnessTracking) {
    auto node = test_nodes_[0];
    routing_table_->add_node(node);
    
    // Get initial last seen time
    auto found_node = routing_table_->find_node(node.id());
    ASSERT_TRUE(found_node.has_value());
    auto initial_time = found_node->last_seen();
    
    // Wait a bit and update the node
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    routing_table_->update_node_activity(node.id());
    
    // Check that last seen time was updated
    auto updated_node = routing_table_->find_node(node.id());
    ASSERT_TRUE(updated_node.has_value());
    auto updated_time = updated_node->last_seen();
    
    EXPECT_GT(updated_time, initial_time);
}

TEST_F(KademliaRoutingTest, StaleNodeRemoval) {
    // Add nodes
    for (size_t i = 0; i < 5; ++i) {
        routing_table_->add_node(test_nodes_[i]);
    }
    
    auto initial_size = routing_table_->size();
    
    // Mark some nodes as stale and remove them
    auto stale_threshold = std::chrono::steady_clock::now() + std::chrono::hours(1);
    routing_table_->remove_stale_nodes(stale_threshold);
    
    // All nodes should be removed as they are "stale" relative to future threshold
    EXPECT_LT(routing_table_->size(), initial_size);
}

// Bucket splitting and tree structure tests
TEST_F(KademliaRoutingTest, BucketRangeVerification) {
    // Add nodes to various buckets
    for (const auto& node : test_nodes_) {
        routing_table_->add_node(node);
    }
    
    // Verify each bucket contains only nodes in its expected range
    for (int bucket_index = 0; bucket_index < 160; ++bucket_index) {
        auto bucket_nodes = routing_table_->get_bucket_nodes(bucket_index);
        
        for (const auto& node : bucket_nodes) {
            int actual_bucket = calculateBucketIndex(local_id_, node.id());
            EXPECT_EQ(actual_bucket, bucket_index) 
                << "Node in wrong bucket. Expected: " << bucket_index 
                << ", Actual: " << actual_bucket;
        }
    }
}

// Performance and stress tests
TEST_F(KademliaRoutingTest, MassNodeInsertion) {
    const int num_nodes = 1000;
    std::vector<k_node> mass_nodes;
    
    // Generate many random nodes
    for (int i = 0; i < num_nodes; ++i) {
        auto id = node_id::random();
        endpoint ep("10." + std::to_string((i / 256) % 256) + "." + 
                   std::to_string((i / 256) % 256) + "." + 
                   std::to_string(i % 256), 8000 + i);
        mass_nodes.emplace_back(id, ep);
    }
    
    // Measure insertion time
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (const auto& node : mass_nodes) {
        routing_table_->add_node(node);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    // Should complete in reasonable time (less than 1ms per node on average)
    EXPECT_LT(duration.count(), num_nodes * 1000);
    
    // Verify table structure integrity
    EXPECT_GT(routing_table_->size(), 0);
    EXPECT_LE(routing_table_->size(), num_nodes);
}

TEST_F(KademliaRoutingTest, ConcurrentAccess) {
    const int num_threads = 4;
    const int operations_per_thread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> successful_operations{0};
    
    // Launch threads that perform concurrent operations
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, operations_per_thread, &successful_operations]() {
            for (int i = 0; i < operations_per_thread; ++i) {
                try {
                    if (i % 3 == 0) {
                        // Add node
                        auto id = node_id::random();
                        endpoint ep("192.168." + std::to_string(t) + "." + std::to_string(i), 
                                   8000 + t * 1000 + i);
                        k_node node(id, ep);
                        routing_table_->add_node(node);
                    } else if (i % 3 == 1) {
                        // Find closest nodes
                        auto target = node_id::random();
                        routing_table_->find_closest_nodes(target, 5);
                    } else {
                        // Look up random node
                        auto target = node_id::random();
                        routing_table_->find_node(target);
                    }
                    successful_operations.fetch_add(1);
                } catch (...) {
                    // Handle any threading issues
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Most operations should succeed
    EXPECT_GT(successful_operations.load(), num_threads * operations_per_thread * 0.8);
}

// Edge cases and error conditions
TEST_F(KademliaRoutingTest, SelfNodeHandling) {
    // Try to add self to routing table
    k_node self_node(local_id_, endpoint("127.0.0.1", 8080));
    bool added_self = routing_table_->add_node(self_node);
    
    // Should not add self to routing table
    EXPECT_FALSE(added_self);
    EXPECT_EQ(routing_table_->size(), 0);
}

TEST_F(KademliaRoutingTest, DuplicateNodeHandling) {
    auto node = test_nodes_[0];
    
    // Add node first time
    bool first_add = routing_table_->add_node(node);
    EXPECT_TRUE(first_add);
    EXPECT_EQ(routing_table_->size(), 1);
    
    // Add same node again with different endpoint
    k_node duplicate_node(node.id(), endpoint("192.168.2.100", 9090));
    bool second_add = routing_table_->add_node(duplicate_node);
    
    // Should update existing node, not add new one
    EXPECT_FALSE(second_add); // Returns false for update
    EXPECT_EQ(routing_table_->size(), 1);
    
    // Verify endpoint was updated
    auto found = routing_table_->find_node(node.id());
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->endpoint(), duplicate_node.endpoint());
}

TEST_F(KademliaRoutingTest, EmptyTableOperations) {
    // Operations on empty table should handle gracefully
    EXPECT_TRUE(routing_table_->empty());
    
    auto closest = routing_table_->find_closest_nodes(node_id::random(), 5);
    EXPECT_TRUE(closest.empty());
    
    auto missing = routing_table_->find_node(node_id::random());
    EXPECT_FALSE(missing.has_value());
    
    bool removed = routing_table_->remove_node(node_id::random());
    EXPECT_FALSE(removed);
}

// Serialization and persistence tests
TEST_F(KademliaRoutingTest, TableStateSerialization) {
    // Add nodes to table
    for (size_t i = 0; i < 10; ++i) {
        routing_table_->add_node(test_nodes_[i]);
    }
    
    // Get table state
    auto table_state = routing_table_->get_all_nodes();
    EXPECT_EQ(table_state.size(), 10);
    
    // Create new table and restore state
    auto new_table = std::make_unique<k_routing_table>(local_id_);
    for (const auto& node : table_state) {
        new_table->add_node(node);
    }
    
    // Verify restored table has same nodes
    EXPECT_EQ(new_table->size(), routing_table_->size());
    
    for (const auto& original_node : table_state) {
        auto found = new_table->find_node(original_node.id());
        EXPECT_TRUE(found.has_value());
        EXPECT_EQ(found->endpoint(), original_node.endpoint());
    }
}

} // namespace ss::kademlia::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}