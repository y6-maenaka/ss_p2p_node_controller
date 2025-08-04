#include <gtest/gtest.h>
#include <ss_p2p/kademlia/k_node.hpp>
#include <ss_p2p/kademlia/node_id.hpp>

#include <chrono>

using namespace ss::kademlia;

class KNodeAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test endpoint
        test_ep_ = boost::asio::ip::udp::endpoint{
            boost::asio::ip::make_address("192.168.1.100"), 8080
        };
        
        // Create test node_id
        std::array<std::uint8_t, 20> test_data;
        test_data.fill(0);
        for (size_t i = 0; i < 8; ++i) {
            test_data[i] = static_cast<std::uint8_t>(i + 1);
        }
        test_id_ = node_id{test_data.data()};
    }

    boost::asio::ip::udp::endpoint test_ep_;
    node_id test_id_;
};

TEST_F(KNodeAdapterTest, DefaultConstructor) {
    k_node node;
    
    // Should have default/blank values
    auto ep = node.get_endpoint();
    EXPECT_EQ(ep.port(), 0);
    
    // Node ID should be blank/zero
    auto& id = node.get_id();
    auto data = id.data();
    bool all_zero = true;
    for (auto byte : data) {
        if (byte != 0) {
            all_zero = false;
            break;
        }
    }
    EXPECT_TRUE(all_zero);
}

TEST_F(KNodeAdapterTest, ConstructorFromEndpoint) {
    k_node node(test_ep_);
    
    // Should have the same endpoint
    auto ep = node.get_endpoint();
    EXPECT_EQ(ep, test_ep_);
    
    // Should have calculated node_id from endpoint
    auto& id = node.get_id();
    auto data = id.data();
    bool has_non_zero = false;
    for (auto byte : data) {
        if (byte != 0) {
            has_non_zero = true;
            break;
        }
    }
    EXPECT_TRUE(has_non_zero); // Calculated ID should not be all zeros
}

TEST_F(KNodeAdapterTest, ConstructorFromIdAndEndpoint) {
    k_node node(test_id_, test_ep_);
    
    // Should have the same endpoint and ID
    auto ep = node.get_endpoint();
    EXPECT_EQ(ep, test_ep_);
    
    auto& id = node.get_id();
    EXPECT_EQ(id, test_id_);
}

TEST_F(KNodeAdapterTest, ConstructorFromPeerInfo) {
    // Create peer_info
    ss::core::endpoint core_ep{test_ep_.address().to_string(), test_ep_.port()};
    ss::dht::peer_info peer_info{test_id_.core(), core_ep};
    
    k_node node(peer_info);
    
    // Should match original data
    auto ep = node.get_endpoint();
    EXPECT_EQ(ep, test_ep_);
    
    auto& id = node.get_id();
    EXPECT_EQ(id, test_id_);
}

TEST_F(KNodeAdapterTest, CopyConstructor) {
    k_node original(test_id_, test_ep_);
    k_node copy(original);
    
    // Should be equal
    EXPECT_EQ(original, copy);
    EXPECT_EQ(original.get_endpoint(), copy.get_endpoint());
    EXPECT_EQ(original.get_id(), copy.get_id());
}

TEST_F(KNodeAdapterTest, MoveConstructor) {
    k_node original(test_id_, test_ep_);
    auto original_ep = original.get_endpoint();
    auto original_id = original.get_id();
    
    k_node moved(std::move(original));
    
    // Moved object should have the data
    EXPECT_EQ(moved.get_endpoint(), original_ep);
    EXPECT_EQ(moved.get_id(), original_id);
}

TEST_F(KNodeAdapterTest, CopyAssignment) {
    k_node node1(test_id_, test_ep_);
    k_node node2;
    
    node2 = node1;
    
    EXPECT_EQ(node1, node2);
    EXPECT_EQ(node1.get_endpoint(), node2.get_endpoint());
    EXPECT_EQ(node1.get_id(), node2.get_id());
}

TEST_F(KNodeAdapterTest, MoveAssignment) {
    k_node node1(test_id_, test_ep_);
    k_node node2;
    auto original_ep = node1.get_endpoint();
    auto original_id = node1.get_id();
    
    node2 = std::move(node1);
    
    EXPECT_EQ(node2.get_endpoint(), original_ep);
    EXPECT_EQ(node2.get_id(), original_id);
}

TEST_F(KNodeAdapterTest, EqualityComparison) {
    k_node node1(test_id_, test_ep_);
    k_node node2(test_id_, test_ep_);
    k_node node3; // Different (blank)
    
    EXPECT_TRUE(node1 == node2);
    EXPECT_FALSE(node1 == node3);
    EXPECT_TRUE(node1 != node3);
    EXPECT_FALSE(node1 != node2);
}

TEST_F(KNodeAdapterTest, GetEndpoint) {
    k_node node(test_id_, test_ep_);
    
    auto ep = node.get_endpoint();
    EXPECT_EQ(ep.address(), test_ep_.address());
    EXPECT_EQ(ep.port(), test_ep_.port());
}

TEST_F(KNodeAdapterTest, GetIdMutable) {
    k_node node(test_id_, test_ep_);
    
    auto& id = node.get_id();
    EXPECT_EQ(id, test_id_);
    
    // Should be able to modify (though not recommended)
    // This tests the mutable reference return
    static_cast<void>(id); // Suppress unused variable warning
}

TEST_F(KNodeAdapterTest, GetIdConst) {
    const k_node node(test_id_, test_ep_);
    
    const auto& id = node.get_id();
    EXPECT_EQ(id, test_id_);
}

TEST_F(KNodeAdapterTest, ToStringConversion) {
    k_node node(test_id_, test_ep_);
    
    std::string str = node.to_str();
    
    // Should contain some representation of the node
    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("k_node"), std::string::npos);
}

TEST_F(KNodeAdapterTest, SharedPointerCreation) {
    auto node = std::make_shared<k_node>(test_id_, test_ep_);
    
    auto ref = node->to_ref();
    EXPECT_EQ(ref, node);
    EXPECT_EQ(ref.use_count(), 2); // node and ref
}

TEST_F(KNodeAdapterTest, PrintMethod) {
    k_node node(test_id_, test_ep_);
    
    // Should not throw (difficult to test output without capturing stdout)
    EXPECT_NO_THROW(node.print());
}

TEST_F(KNodeAdapterTest, BlankStaticMethod) {
    auto blank_node = k_node::blank();
    
    // Should be same as default constructor
    k_node default_node;
    EXPECT_EQ(blank_node.get_endpoint().port(), 0);
}

TEST_F(KNodeAdapterTest, PeerInfoAccess) {
    k_node node(test_id_, test_ep_);
    
    const auto& peer_info = node.peer_info();
    EXPECT_EQ(peer_info.id, test_id_.core());
    EXPECT_EQ(peer_info.endpoint.address(), test_ep_.address().to_string());
    EXPECT_EQ(peer_info.endpoint.port(), test_ep_.port());
}

TEST_F(KNodeAdapterTest, PeerInfoMutableAccess) {
    k_node node(test_id_, test_ep_);
    
    auto& peer_info = node.peer_info();
    
    // Should be able to modify
    peer_info.failure_count = 5;
    EXPECT_EQ(node.failure_count(), 5);
}

TEST_F(KNodeAdapterTest, LivenessOperations) {
    k_node node(test_id_, test_ep_);
    
    // Initially should be alive (just created)
    EXPECT_TRUE(node.is_alive());
    
    // Update liveness
    auto rtt = std::chrono::microseconds{1000};
    node.update_liveness(rtt);
    EXPECT_EQ(node.rtt(), rtt);
    EXPECT_EQ(node.failure_count(), 0);
    
    // Record failures
    node.record_failure();
    EXPECT_EQ(node.failure_count(), 1);
    
    node.record_failure();
    EXPECT_EQ(node.failure_count(), 2);
    
    // Update liveness should reset failure count
    node.update_liveness();
    EXPECT_EQ(node.failure_count(), 0);
}

TEST_F(KNodeAdapterTest, LivenessTimeout) {
    k_node node(test_id_, test_ep_);
    
    // Modify last_seen to be old
    auto& peer_info = node.peer_info();
    peer_info.last_seen = std::chrono::steady_clock::now() - std::chrono::minutes{10};
    
    // Should not be alive with short timeout
    EXPECT_FALSE(node.is_alive(std::chrono::minutes{5}));
    
    // Should be alive with long timeout
    EXPECT_TRUE(node.is_alive(std::chrono::minutes{15}));
}

TEST_F(KNodeAdapterTest, StringToKNodeConversion) {
    // Test format: "ip:port"
    std::string endpoint_str = "10.0.0.1:9000";
    auto node = str_to_k_node(endpoint_str);
    
    auto ep = node.get_endpoint();
    EXPECT_EQ(ep.address().to_string(), "10.0.0.1");
    EXPECT_EQ(ep.port(), 9000);
    
    // Test format: "node_id@ip:port"
    std::string hex_id = "0123456789abcdef0123456789abcdef01234567";
    std::string full_str = hex_id + "@10.0.0.2:9001";
    auto node_with_id = str_to_k_node(full_str);
    
    auto ep2 = node_with_id.get_endpoint();
    EXPECT_EQ(ep2.address().to_string(), "10.0.0.2");
    EXPECT_EQ(ep2.port(), 9001);
    
    // Test invalid string
    auto invalid_node = str_to_k_node("invalid_format");
    EXPECT_EQ(invalid_node.get_endpoint().port(), 0); // Should be blank
}

TEST_F(KNodeAdapterTest, ExceptionSafety) {
    // Test various operations don't throw unexpectedly
    k_node node;
    
    EXPECT_NO_THROW(node.get_endpoint());
    EXPECT_NO_THROW(node.get_id());
    EXPECT_NO_THROW(node.to_str());
    EXPECT_NO_THROW(node.print());
    EXPECT_NO_THROW(node.is_alive());
    EXPECT_NO_THROW(node.failure_count());
    EXPECT_NO_THROW(node.rtt());
    EXPECT_NO_THROW(node.update_liveness());
    EXPECT_NO_THROW(node.record_failure());
}

TEST_F(KNodeAdapterTest, MemoryManagement) {
    // Test that creating many nodes doesn't cause issues
    std::vector<k_node> nodes;
    
    for (int i = 0; i < 100; ++i) {
        boost::asio::ip::udp::endpoint ep{
            boost::asio::ip::make_address("127.0.0.1"), 
            static_cast<std::uint16_t>(8000 + i)
        };
        nodes.emplace_back(ep);
    }
    
    // All should have different endpoints
    for (size_t i = 0; i < nodes.size(); ++i) {
        for (size_t j = i + 1; j < nodes.size(); ++j) {
            EXPECT_NE(nodes[i].get_endpoint().port(), nodes[j].get_endpoint().port());
        }
    }
}