#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <ss_p2p/core/types.hpp>
#include <unordered_set>
#include <unordered_map>

using namespace ss::core;

class TypesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// node_id tests
TEST_F(TypesTest, NodeIdDefaultConstruction) {
    node_id id;
    
    // Default constructed node_id should be zero
    for (const auto& byte : id.data()) {
        EXPECT_EQ(byte, 0);
    }
}

TEST_F(TypesTest, NodeIdDataConstruction) {
    node_id::storage_type data;
    std::iota(data.begin(), data.end(), 0);
    
    node_id id(data);
    
    EXPECT_EQ(id.data(), data);
    for (size_t i = 0; i < node_id::SIZE; ++i) {
        EXPECT_EQ(id.data()[i], static_cast<std::uint8_t>(i));
    }
}

TEST_F(TypesTest, NodeIdCopyAndMove) {
    node_id::storage_type data;
    std::fill(data.begin(), data.end(), 0x42);
    node_id original(data);
    
    // Test copy construction
    node_id copied(original);
    EXPECT_EQ(copied, original);
    EXPECT_EQ(copied.data(), original.data());
    
    // Test move construction
    node_id moved(std::move(copied));
    EXPECT_EQ(moved, original);
    
    // Test copy assignment
    node_id copy_assigned;
    copy_assigned = original;
    EXPECT_EQ(copy_assigned, original);
    
    // Test move assignment
    node_id move_assigned;
    move_assigned = std::move(copy_assigned);
    EXPECT_EQ(move_assigned, original);
}

TEST_F(TypesTest, NodeIdDistance) {
    node_id::storage_type data1, data2;
    std::fill(data1.begin(), data1.end(), 0xFF);
    std::fill(data2.begin(), data2.end(), 0x00);
    
    node_id id1(data1);
    node_id id2(data2);
    
    node_id distance = id1.distance(id2);
    
    // XOR of 0xFF and 0x00 should be 0xFF
    for (const auto& byte : distance.data()) {
        EXPECT_EQ(byte, 0xFF);
    }
    
    // Distance should be symmetric
    EXPECT_EQ(id1.distance(id2), id2.distance(id1));
    
    // Distance to self should be zero
    node_id zero_distance = id1.distance(id1);
    for (const auto& byte : zero_distance.data()) {
        EXPECT_EQ(byte, 0x00);
    }
}

TEST_F(TypesTest, NodeIdComparison) {
    node_id::storage_type data1, data2, data3;
    std::fill(data1.begin(), data1.end(), 0x42);
    std::fill(data2.begin(), data2.end(), 0x42);
    std::fill(data3.begin(), data3.end(), 0x43);
    
    node_id id1(data1);
    node_id id2(data2);
    node_id id3(data3);
    
    // Test equality
    EXPECT_EQ(id1, id2);
    EXPECT_NE(id1, id3);
    
    // Test ordering
    EXPECT_LT(id1, id3);
    EXPECT_FALSE(id3 < id1);
}

TEST_F(TypesTest, NodeIdHexConversion) {
    const std::string hex_str = "0123456789abcdef0123456789abcdef01234567";
    
    // Test from_hex
    node_id id = node_id::from_hex(hex_str);
    
    // Test to_hex
    std::string result_hex = id.to_hex();
    EXPECT_EQ(result_hex, hex_str);
    
    // Test round trip
    node_id round_trip = node_id::from_hex(result_hex);
    EXPECT_EQ(id, round_trip);
}

TEST_F(TypesTest, NodeIdHexConversionInvalidInput) {
    // Test invalid length
    EXPECT_THROW(node_id::from_hex("123"), std::invalid_argument);
    EXPECT_THROW(node_id::from_hex(""), std::invalid_argument);
    
    // Test invalid characters
    EXPECT_THROW(node_id::from_hex("0123456789abcdef0123456789abcdef0123456g"), 
                 std::invalid_argument);
}

TEST_F(TypesTest, NodeIdRandom) {
    // Generate multiple random IDs
    std::unordered_set<node_id> ids;
    constexpr int num_ids = 1000;
    
    for (int i = 0; i < num_ids; ++i) {
        node_id id = node_id::random();
        ids.insert(id);
    }
    
    // All IDs should be unique (with very high probability)
    EXPECT_EQ(ids.size(), num_ids);
}

TEST_F(TypesTest, NodeIdHash) {
    node_id::storage_type data1, data2;
    std::fill(data1.begin(), data1.end(), 0x42);
    std::fill(data2.begin(), data2.end(), 0x43);
    
    node_id id1(data1);
    node_id id2(data2);
    
    std::hash<node_id> hasher;
    auto hash1 = hasher(id1);
    auto hash2 = hasher(id2);
    
    // Different IDs should have different hashes (with high probability)
    EXPECT_NE(hash1, hash2);
    
    // Same ID should have same hash
    EXPECT_EQ(hash1, hasher(id1));
    
    // Test in unordered containers
    std::unordered_map<node_id, std::string> map;
    map[id1] = "value1";
    map[id2] = "value2";
    
    EXPECT_EQ(map[id1], "value1");
    EXPECT_EQ(map[id2], "value2");
}

// message_id tests
TEST_F(TypesTest, MessageIdDefaultConstruction) {
    message_id id;
    EXPECT_EQ(id.value(), 0);
}

TEST_F(TypesTest, MessageIdValueConstruction) {
    constexpr message_id::value_type test_value = 12345;
    message_id id(test_value);
    EXPECT_EQ(id.value(), test_value);
}

TEST_F(TypesTest, MessageIdComparison) {
    message_id id1(100);
    message_id id2(100);
    message_id id3(200);
    
    EXPECT_EQ(id1, id2);
    EXPECT_NE(id1, id3);
    EXPECT_LT(id1, id3);
}

TEST_F(TypesTest, MessageIdGenerate) {
    std::unordered_set<message_id> ids;
    constexpr int num_ids = 1000;
    
    for (int i = 0; i < num_ids; ++i) {
        message_id id = message_id::generate();
        ids.insert(id);
    }
    
    // All generated IDs should be unique
    EXPECT_EQ(ids.size(), num_ids);
    
    // Generated IDs should not be zero
    for (const auto& id : ids) {
        EXPECT_NE(id.value(), 0);
    }
}

TEST_F(TypesTest, MessageIdHash) {
    message_id id1(123);
    message_id id2(456);
    
    std::hash<message_id> hasher;
    auto hash1 = hasher(id1);
    auto hash2 = hasher(id2);
    
    EXPECT_NE(hash1, hash2);
    EXPECT_EQ(hash1, hasher(id1));
    
    // Test in unordered containers
    std::unordered_set<message_id> set;
    set.insert(id1);
    set.insert(id2);
    
    EXPECT_EQ(set.size(), 2);
    EXPECT_TRUE(set.contains(id1));
    EXPECT_TRUE(set.contains(id2));
}

// endpoint tests
TEST_F(TypesTest, EndpointDefaultConstruction) {
    endpoint ep;
    EXPECT_FALSE(ep.is_valid());
    EXPECT_EQ(ep.port(), 0);
}

TEST_F(TypesTest, EndpointNativeConstruction) {
    boost::asio::ip::udp::endpoint native_ep(
        boost::asio::ip::make_address("127.0.0.1"), 8080);
    
    endpoint ep(native_ep);
    EXPECT_TRUE(ep.is_valid());
    EXPECT_EQ(ep.address(), "127.0.0.1");
    EXPECT_EQ(ep.port(), 8080);
    EXPECT_EQ(ep.native(), native_ep);
}

TEST_F(TypesTest, EndpointStringConstruction) {
    endpoint ep("192.168.1.1", 9090);
    EXPECT_TRUE(ep.is_valid());
    EXPECT_EQ(ep.address(), "192.168.1.1");
    EXPECT_EQ(ep.port(), 9090);
}

TEST_F(TypesTest, EndpointStringConstructionIPv6) {
    endpoint ep("::1", 8080);
    EXPECT_TRUE(ep.is_valid());
    EXPECT_EQ(ep.address(), "::1");
    EXPECT_EQ(ep.port(), 8080);
}

TEST_F(TypesTest, EndpointInvalidAddress) {
    EXPECT_THROW(endpoint("invalid.address", 8080), std::invalid_argument);
    EXPECT_THROW(endpoint("999.999.999.999", 8080), std::invalid_argument);
}

TEST_F(TypesTest, EndpointComparison) {
    endpoint ep1("127.0.0.1", 8080);
    endpoint ep2("127.0.0.1", 8080);
    endpoint ep3("127.0.0.1", 9090);
    endpoint ep4("192.168.1.1", 8080);
    
    EXPECT_EQ(ep1, ep2);
    EXPECT_NE(ep1, ep3);
    EXPECT_NE(ep1, ep4);
    
    // Test ordering
    EXPECT_LT(ep1, ep3);  // Same address, different port
}

TEST_F(TypesTest, EndpointToString) {
    endpoint ep1("127.0.0.1", 8080);
    EXPECT_EQ(ep1.to_string(), "127.0.0.1:8080");
    
    endpoint ep2("::1", 8080);
    EXPECT_EQ(ep2.to_string(), "[::1]:8080");
    
    endpoint ep3;  // Invalid endpoint
    EXPECT_EQ(ep3.to_string(), "invalid_endpoint");
}

TEST_F(TypesTest, EndpointHash) {
    endpoint ep1("127.0.0.1", 8080);
    endpoint ep2("127.0.0.1", 9090);
    endpoint ep3("192.168.1.1", 8080);
    
    std::hash<endpoint> hasher;
    auto hash1 = hasher(ep1);
    auto hash2 = hasher(ep2);
    auto hash3 = hasher(ep3);
    
    // Different endpoints should have different hashes
    EXPECT_NE(hash1, hash2);
    EXPECT_NE(hash1, hash3);
    EXPECT_NE(hash2, hash3);
    
    // Same endpoint should have same hash
    EXPECT_EQ(hash1, hasher(ep1));
    
    // Test in unordered containers
    std::unordered_set<endpoint> set;
    set.insert(ep1);
    set.insert(ep2);
    set.insert(ep3);
    
    EXPECT_EQ(set.size(), 3);
}

// peer_id tests (should behave like node_id)
TEST_F(TypesTest, PeerIdBasicFunctionality) {
    peer_id pid1 = node_id::random();
    peer_id pid2 = node_id::random();
    
    EXPECT_NE(pid1, pid2);
    EXPECT_EQ(pid1, pid1);
    
    // Test that peer_id is just an alias for node_id
    static_assert(std::is_same_v<peer_id, node_id>);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}