#include <gtest/gtest.h>
#include <ss_p2p/kademlia/node_id.hpp>
#include <ss_p2p/core/types.hpp>

#include <array>
#include <cstring>

using namespace ss::kademlia;

class NodeIdAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test data
        test_data_.fill(0);
        for (size_t i = 0; i < test_data_.size(); ++i) {
            test_data_[i] = static_cast<std::uint8_t>(i % 256);
        }
    }

    std::array<std::uint8_t, 20> test_data_;
};

TEST_F(NodeIdAdapterTest, DefaultConstructor) {
    node_id id;
    
    // Should be zero-initialized
    auto data = id.data();
    for (auto byte : data) {
        EXPECT_EQ(byte, 0);
    }
}

TEST_F(NodeIdAdapterTest, ConstructorFromRawData) {
    node_id id(test_data_.data());
    
    // Should match test data
    auto data = id.data();
    for (size_t i = 0; i < data.size(); ++i) {
        EXPECT_EQ(data[i], test_data_[i]);
    }
}

TEST_F(NodeIdAdapterTest, CopyConstructor) {
    node_id original(test_data_.data());
    node_id copy(original);
    
    // Should be equal
    EXPECT_EQ(original, copy);
    EXPECT_EQ(original.data(), copy.data());
}

TEST_F(NodeIdAdapterTest, MoveConstructor) {
    node_id original(test_data_.data());
    auto original_data = original.data();
    
    node_id moved(std::move(original));
    
    // Moved object should have the data
    EXPECT_EQ(moved.data(), original_data);
}

TEST_F(NodeIdAdapterTest, CopyAssignment) {
    node_id id1(test_data_.data());
    node_id id2;
    
    id2 = id1;
    
    EXPECT_EQ(id1, id2);
    EXPECT_EQ(id1.data(), id2.data());
}

TEST_F(NodeIdAdapterTest, MoveAssignment) {
    node_id id1(test_data_.data());
    node_id id2;
    auto original_data = id1.data();
    
    id2 = std::move(id1);
    
    EXPECT_EQ(id2.data(), original_data);
}

TEST_F(NodeIdAdapterTest, ConstructorFromCoreNodeId) {
    // Create core node_id
    ss::core::node_id core_id;
    auto& core_data = core_id.data();
    std::copy(test_data_.begin(), test_data_.end(), core_data.begin());
    
    // Create adapter from core
    node_id adapter_id(core_id);
    
    // Should match
    auto adapter_data = adapter_id.data();
    for (size_t i = 0; i < adapter_data.size(); ++i) {
        EXPECT_EQ(adapter_data[i], test_data_[i]);
    }
}

TEST_F(NodeIdAdapterTest, ToStringConversion) {
    node_id id(test_data_.data());
    
    std::string hex_str = id.to_str();
    
    // Should be valid hex string
    EXPECT_EQ(hex_str.length(), 40); // 20 bytes * 2 chars per byte
    
    // Should only contain hex characters
    for (char c : hex_str) {
        EXPECT_TRUE(std::isxdigit(c));
    }
}

TEST_F(NodeIdAdapterTest, EqualityComparison) {
    node_id id1(test_data_.data());
    node_id id2(test_data_.data());
    node_id id3; // Zero-initialized
    
    EXPECT_TRUE(id1 == id2);
    EXPECT_FALSE(id1 == id3);
    EXPECT_TRUE(id1 != id3);
    EXPECT_FALSE(id1 != id2);
}

TEST_F(NodeIdAdapterTest, LessThanComparison) {
    std::array<std::uint8_t, 20> data1{};
    std::array<std::uint8_t, 20> data2{};
    
    data1[0] = 1;
    data2[0] = 2;
    
    node_id id1(data1.data());
    node_id id2(data2.data());
    
    EXPECT_TRUE(id1 < id2);
    EXPECT_FALSE(id2 < id1);
}

TEST_F(NodeIdAdapterTest, ArraySubscriptOperator) {
    node_id id(test_data_.data());
    
    for (size_t i = 0; i < test_data_.size(); ++i) {
        EXPECT_EQ(id[i], test_data_[i]);
    }
}

TEST_F(NodeIdAdapterTest, ArraySubscriptOutOfRange) {
    node_id id;
    
    EXPECT_THROW(id[20], std::out_of_range);
    EXPECT_THROW(id[255], std::out_of_range);
}

TEST_F(NodeIdAdapterTest, CallOperator) {
    node_id id(test_data_.data());
    
    auto data = id();
    
    for (size_t i = 0; i < data.size(); ++i) {
        EXPECT_EQ(data[i], test_data_[i]);
    }
}

TEST_F(NodeIdAdapterTest, NoneStaticMethod) {
    auto none_id = node_id::none();
    
    auto data = none_id.data();
    for (auto byte : data) {
        EXPECT_EQ(byte, 0);
    }
}

TEST_F(NodeIdAdapterTest, RandomStaticMethod) {
    auto random_id1 = node_id::random();
    auto random_id2 = node_id::random();
    
    // Should be different (very high probability)
    EXPECT_NE(random_id1, random_id2);
    
    // Should not be all zeros
    auto data = random_id1.data();
    bool has_non_zero = false;
    for (auto byte : data) {
        if (byte != 0) {
            has_non_zero = true;
            break;
        }
    }
    EXPECT_TRUE(has_non_zero);
}

TEST_F(NodeIdAdapterTest, CoreAccess) {
    node_id id(test_data_.data());
    
    const auto& core_id = id.core();
    auto core_data = core_id.data();
    
    for (size_t i = 0; i < core_data.size(); ++i) {
        EXPECT_EQ(core_data[i], test_data_[i]);
    }
}

TEST_F(NodeIdAdapterTest, XorDistanceCalculation) {
    std::array<std::uint8_t, 20> data1{};
    std::array<std::uint8_t, 20> data2{};
    
    data1[0] = 0b10101010;
    data2[0] = 0b01010101;
    
    node_id id1(data1.data());
    node_id id2(data2.data());
    
    auto distance = calc_node_xor_distance(id1, id2);
    
    // First byte XOR gives 0b11111111, so leading zero count should be 0
    EXPECT_EQ(distance, 0);
}

TEST_F(NodeIdAdapterTest, XorDistanceWithLeadingZeros) {
    std::array<std::uint8_t, 20> data1{};
    std::array<std::uint8_t, 20> data2{};
    
    // Set up data so XOR has leading zeros
    data1[0] = 0b00000000;
    data1[1] = 0b00000000;
    data1[2] = 0b10000000; // First different bit at bit 16
    
    data2[0] = 0b00000000;
    data2[1] = 0b00000000;
    data2[2] = 0b00000000;
    
    node_id id1(data1.data());
    node_id id2(data2.data());
    
    auto distance = calc_node_xor_distance(id1, id2);
    
    // Should have 16 leading zeros
    EXPECT_EQ(distance, 16);
}

TEST_F(NodeIdAdapterTest, StringToNodeIdConversion) {
    // Test valid hex string
    std::string hex_str = "0123456789abcdef0123456789abcdef01234567";
    auto id = str_to_node_id(hex_str);
    
    // Should convert back to same string (case may differ)
    auto converted_back = id.to_str();
    EXPECT_EQ(converted_back.length(), hex_str.length());
    
    // Test invalid string - should return none
    auto invalid_id = str_to_node_id("invalid_hex_string");
    EXPECT_EQ(invalid_id, node_id::none());
}

TEST_F(NodeIdAdapterTest, PrintMethod) {
    node_id id(test_data_.data());
    
    // Should not throw (difficult to test output without capturing stdout)
    EXPECT_NO_THROW(id.print());
}

TEST_F(NodeIdAdapterTest, ConstCorrectness) {
    const node_id id(test_data_.data());
    
    // Should be able to call const methods
    EXPECT_NO_THROW(id.data());
    EXPECT_NO_THROW(id.to_str());
    EXPECT_NO_THROW(id[0]);
    EXPECT_NO_THROW(id());
    EXPECT_NO_THROW(id.core());
    EXPECT_NO_THROW(id.print());
}

TEST_F(NodeIdAdapterTest, MemoryManagement) {
    // Test that multiple instances don't interfere
    std::vector<node_id> ids;
    
    for (int i = 0; i < 100; ++i) {
        ids.emplace_back(node_id::random());
    }
    
    // All should be different
    for (size_t i = 0; i < ids.size(); ++i) {
        for (size_t j = i + 1; j < ids.size(); ++j) {
            EXPECT_NE(ids[i], ids[j]);
        }
    }
}