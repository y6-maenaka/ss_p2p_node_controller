#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ss_p2p/network/i_protocol.hpp"

namespace ss::network::test {

/**
 * @brief Test fixture for protocol interface tests
 */
class ProtocolInterfaceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test fixtures
    }

    void TearDown() override {
        // Cleanup test fixtures
    }
};

TEST_F(ProtocolInterfaceTest, ProtocolVersionComparison) {
    protocol_version v1_0_0{1, 0, 0};
    protocol_version v1_0_1{1, 0, 1};
    protocol_version v1_1_0{1, 1, 0};
    protocol_version v2_0_0{2, 0, 0};

    // Test equality
    EXPECT_EQ(v1_0_0, protocol_version(1, 0, 0));
    EXPECT_NE(v1_0_0, v1_0_1);

    // Test ordering
    EXPECT_LT(v1_0_0, v1_0_1);
    EXPECT_LT(v1_0_1, v1_1_0);
    EXPECT_LT(v1_1_0, v2_0_0);

    // Test compatibility
    EXPECT_TRUE(v1_0_1.is_compatible(v1_0_0));
    EXPECT_TRUE(v1_1_0.is_compatible(v1_0_0));
    EXPECT_FALSE(v1_0_0.is_compatible(v1_0_1));
    EXPECT_FALSE(v1_0_0.is_compatible(v2_0_0));
}

TEST_F(ProtocolInterfaceTest, ProtocolVersionSerialization) {
    protocol_version version{1, 2, 3};
    
    std::string version_str = version.to_string();
    EXPECT_EQ(version_str, "1.2.3");
    
    auto parsed_result = protocol_version::from_string(version_str);
    ASSERT_TRUE(parsed_result.is_ok());
    EXPECT_EQ(parsed_result.value(), version);
}

TEST_F(ProtocolInterfaceTest, InvalidVersionString) {
    auto result1 = protocol_version::from_string("invalid");
    EXPECT_FALSE(result1.is_ok());
    
    auto result2 = protocol_version::from_string("1.2");
    EXPECT_FALSE(result2.is_ok());
    
    auto result3 = protocol_version::from_string("1.2.3.4");
    EXPECT_FALSE(result3.is_ok());
}

TEST_F(ProtocolInterfaceTest, ErrorCodeConversion) {
    EXPECT_NE(to_string(protocol_error::none), "");
    EXPECT_NE(to_string(protocol_error::invalid_format), "");
    EXPECT_NE(to_string(protocol_error::unsupported_version), "");
    EXPECT_NE(to_string(protocol_error::message_too_large), "");
}

TEST_F(ProtocolInterfaceTest, ProtocolConfig) {
    protocol_config config;
    
    // Test default values
    EXPECT_EQ(config.version, protocol_version(1, 0, 0));
    EXPECT_FALSE(config.enable_compression);
    EXPECT_FALSE(config.enable_encryption);
    EXPECT_TRUE(config.enable_fragmentation);
    EXPECT_GT(config.max_message_size, 0);
}

TEST_F(ProtocolInterfaceTest, MessageMetadata) {
    message_metadata metadata;
    
    // Test default values
    EXPECT_EQ(metadata.type_id, 0);
    EXPECT_EQ(metadata.sequence_number, 0);
    EXPECT_EQ(metadata.fragment.fragment_id, 0);
    EXPECT_EQ(metadata.fragment.total_fragments, 1);
    EXPECT_EQ(metadata.flags, 0);
    EXPECT_NE(metadata.created_at, std::chrono::steady_clock::time_point{});
}

TEST_F(ProtocolInterfaceTest, EncodedMessage) {
    std::vector<std::uint8_t> data = {1, 2, 3, 4, 5};
    message_metadata metadata;
    metadata.type_id = 42;
    
    encoded_message msg(data, metadata);
    
    EXPECT_EQ(msg.data, data);
    EXPECT_EQ(msg.metadata.type_id, 42);
    EXPECT_EQ(msg.original_size, data.size());
}

TEST_F(ProtocolInterfaceTest, DecodedMessage) {
    std::vector<std::uint8_t> payload = {1, 2, 3, 4, 5};
    message_metadata metadata;
    metadata.type_id = 42;
    
    decoded_message msg(payload, metadata);
    
    EXPECT_EQ(msg.payload, payload);
    EXPECT_EQ(msg.metadata.type_id, 42);
    EXPECT_EQ(msg.encoded_size, payload.size());
}

} // namespace ss::network::test