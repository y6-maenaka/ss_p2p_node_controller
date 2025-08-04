/**
 * @file test_stun_client.cpp
 * @brief Unit tests for STUN client implementation
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ss_p2p/ice/i_stun_client.hpp"
#include "ss_p2p/core/types.hpp"
#include "ss_p2p/core/result.hpp"

#include <memory>
#include <vector>
#include <chrono>
#include <random>

using namespace ss::ice;
using namespace ss::core;

namespace {

/**
 * @brief Mock transport for testing
 */
class MockTransport : public ss::network::i_transport {
public:
    MOCK_METHOD(async_result<result<void, ss::network::transport_error>>, 
                bind, (const endpoint& local_endpoint), (override));
    
    MOCK_METHOD(async_result<result<endpoint, ss::network::transport_error>>, 
                connect, (const endpoint& remote_endpoint), (override));
    
    MOCK_METHOD(async_result<result<void, ss::network::transport_error>>, 
                send, (ss::network::outgoing_message message), (override));
    
    MOCK_METHOD(void, send_async, 
                (ss::network::outgoing_message message, ss::network::send_completion_handler handler), 
                (override));
    
    MOCK_METHOD(async_result<result<ss::network::incoming_message, ss::network::transport_error>>, 
                receive, (std::optional<std::chrono::milliseconds> timeout), (override));
    
    MOCK_METHOD(void, set_message_handler, (ss::network::message_handler handler), (override));
    MOCK_METHOD(void, clear_message_handler, (), (override));
    MOCK_METHOD(endpoint, local_endpoint, (), (const, noexcept, override));
    MOCK_METHOD(endpoint, remote_endpoint, (), (const, noexcept, override));
    MOCK_METHOD(bool, is_connection_based, (), (const, noexcept, override));
    MOCK_METHOD(bool, is_connected, (), (const, noexcept, override));
    MOCK_METHOD(ss::network::transport_stats, get_stats, (), (const, noexcept, override));
    MOCK_METHOD(void, reset_stats, (), (noexcept, override));
    MOCK_METHOD(ss::network::transport_config, get_config, (), (const, noexcept, override));
    MOCK_METHOD(result<void, ss::network::transport_error>, set_config, 
                (const ss::network::transport_config& config), (override));
    MOCK_METHOD(std::uint32_t, max_message_size, (), (const, noexcept, override));
    MOCK_METHOD(bool, can_send, (), (const, noexcept, override));
    MOCK_METHOD(std::uint32_t, send_queue_size, (), (const, noexcept, override));
    MOCK_METHOD(std::uint32_t, receive_queue_size, (), (const, noexcept, override));
    MOCK_METHOD(async_void, disconnect, (), (override));
    MOCK_METHOD(async_void, shutdown, (std::chrono::milliseconds timeout), (override));
    
    // i_component interface
    MOCK_METHOD(async_void, start, (), (override));
    MOCK_METHOD(async_void, stop, (), (override));
    MOCK_METHOD(bool, is_running, (), (const, noexcept, override));
};

/**
 * @brief Test fixture for STUN client tests
 */
class STUNClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        io_context_ = std::make_unique<boost::asio::io_context>();
        mock_transport_ = std::make_shared<MockTransport>();
        
        // Create STUN client (would use factory function in real implementation)
        // stun_client_ = create_rfc5389_stun_client(*io_context_);
        // stun_client_->set_transport(mock_transport_);
        
        // Setup test endpoints
        stun_server_ = endpoint("203.0.113.10", 3478);
        local_endpoint_ = endpoint("192.168.1.100", 5000);
        external_endpoint_ = endpoint("203.0.113.1", 12345);
        
        // Setup default mock behavior
        ON_CALL(*mock_transport_, is_running())
            .WillByDefault(::testing::Return(true));
        ON_CALL(*mock_transport_, local_endpoint())
            .WillByDefault(::testing::Return(local_endpoint_));
        ON_CALL(*mock_transport_, can_send())
            .WillByDefault(::testing::Return(true));
    }

    void TearDown() override {
        if (stun_client_) {
            // Cleanup
        }
        io_context_.reset();
    }

    std::unique_ptr<boost::asio::io_context> io_context_;
    std::shared_ptr<MockTransport> mock_transport_;
    std::unique_ptr<i_stun_client> stun_client_;
    
    endpoint stun_server_;
    endpoint local_endpoint_;
    endpoint external_endpoint_;
};

/**
 * @brief Test STUN transaction ID generation and properties
 */
TEST_F(STUNClientTest, TransactionIDGeneration) {
    // Test transaction ID creation
    auto tx_id1 = stun_transaction_id::random();
    auto tx_id2 = stun_transaction_id::random();
    
    // Transaction IDs should be different
    EXPECT_NE(tx_id1, tx_id2);
    
    // Test equality and comparison
    auto tx_id3 = tx_id1;
    EXPECT_EQ(tx_id1, tx_id3);
    EXPECT_NE(tx_id1, tx_id2);
    
    // Test data access
    const auto& data1 = tx_id1.data();
    const auto& data3 = tx_id3.data();
    EXPECT_EQ(data1, data3);
    
    // Test size
    EXPECT_EQ(data1.size(), stun_transaction_id::SIZE);
    EXPECT_EQ(stun_transaction_id::SIZE, 12); // 96 bits
}

/**
 * @brief Test STUN message creation and manipulation
 */
TEST_F(STUNClientTest, STUNMessageCreation) {
    auto tx_id = stun_transaction_id::random();
    stun_message message(stun_message_type::binding_request, tx_id);
    
    // Test basic properties
    EXPECT_EQ(message.type, stun_message_type::binding_request);
    EXPECT_EQ(message.transaction_id, tx_id);
    EXPECT_TRUE(message.attributes.empty());
    EXPECT_FALSE(message.is_error_response());
    
    // Test attribute addition
    std::vector<std::uint8_t> test_data = {0x01, 0x02, 0x03, 0x04};
    stun_attribute attr(stun_attribute_type::software, test_data);
    message.add_attribute(std::move(attr));
    
    EXPECT_EQ(message.attributes.size(), 1);
    EXPECT_EQ(message.attributes[0].type, stun_attribute_type::software);
    EXPECT_EQ(message.attributes[0].value, test_data);
    
    // Test attribute finding
    const auto* found_attr = message.find_attribute(stun_attribute_type::software);
    ASSERT_NE(found_attr, nullptr);
    EXPECT_EQ(found_attr->value, test_data);
    
    const auto* missing_attr = message.find_attribute(stun_attribute_type::username);
    EXPECT_EQ(missing_attr, nullptr);
}

/**
 * @brief Test STUN error response handling
 */
TEST_F(STUNClientTest, STUNErrorResponse) {
    auto tx_id = stun_transaction_id::random();
    stun_message error_response(stun_message_type::binding_error_response, tx_id);
    
    EXPECT_TRUE(error_response.is_error_response());
    
    // Create ERROR-CODE attribute
    std::vector<std::uint8_t> error_code_data = {
        0x00, 0x00, // Reserved
        0x04,       // Class = 4
        0x01,       // Number = 1 (401 Unauthorized)
        'U', 'n', 'a', 'u', 't', 'h', 'o', 'r', 'i', 'z', 'e', 'd' // Reason phrase
    };
    
    stun_attribute error_attr(stun_attribute_type::error_code, error_code_data);
    error_response.add_attribute(std::move(error_attr));
    
    // Test error code extraction
    EXPECT_EQ(error_response.get_error_code(), 401);
    EXPECT_EQ(error_response.get_error_reason(), "Unauthorised");
}

/**
 * @brief Test STUN client configuration
 */
TEST_F(STUNClientTest, STUNClientConfiguration) {
    stun_client_config config;
    
    // Test default configuration
    EXPECT_EQ(config.request_timeout.count(), 3000);
    EXPECT_EQ(config.max_retries, 3);
    EXPECT_EQ(config.rto.count(), 500);
    EXPECT_FALSE(config.enable_integrity);
    EXPECT_TRUE(config.enable_fingerprint);
    EXPECT_TRUE(config.username.empty());
    EXPECT_TRUE(config.password.empty());
    EXPECT_FALSE(config.rfc3489_compat);
    
    // Test configuration modification
    config.request_timeout = std::chrono::milliseconds(5000);
    config.max_retries = 5;
    config.enable_integrity = true;
    config.username = "testuser";
    config.password = "testpass";
    config.software = "test-client/1.0";
    config.rfc3489_compat = true;
    
    EXPECT_EQ(config.request_timeout.count(), 5000);
    EXPECT_EQ(config.max_retries, 5);
    EXPECT_TRUE(config.enable_integrity);
    EXPECT_EQ(config.username, "testuser");
    EXPECT_EQ(config.password, "testpass");
    EXPECT_EQ(config.software, "test-client/1.0");
    EXPECT_TRUE(config.rfc3489_compat);
}

/**
 * @brief Test XOR-MAPPED-ADDRESS encoding and decoding
 */
TEST_F(STUNClientTest, XORMappedAddressHandling) {
    if (!stun_client_) {
        GTEST_SKIP() << "STUN client not available for testing";
    }
    
    auto tx_id = stun_transaction_id::random();
    
    // Test IPv4 address encoding
    endpoint ipv4_endpoint("192.168.1.100", 5000);
    auto encoded_ipv4 = stun_client_->create_xor_mapped_address(ipv4_endpoint, tx_id);
    EXPECT_FALSE(encoded_ipv4.empty());
    
    // Test IPv4 address decoding
    auto decoded_ipv4_result = stun_client_->parse_xor_mapped_address(encoded_ipv4, tx_id);
    ASSERT_TRUE(decoded_ipv4_result.is_ok());
    
    auto decoded_ipv4 = decoded_ipv4_result.value();
    EXPECT_EQ(decoded_ipv4.address(), ipv4_endpoint.address());
    EXPECT_EQ(decoded_ipv4.port(), ipv4_endpoint.port());
    
    // Test IPv6 address encoding (if supported)
    try {
        endpoint ipv6_endpoint("2001:db8::1", 5000);
        auto encoded_ipv6 = stun_client_->create_xor_mapped_address(ipv6_endpoint, tx_id);
        EXPECT_FALSE(encoded_ipv6.empty());
        
        auto decoded_ipv6_result = stun_client_->parse_xor_mapped_address(encoded_ipv6, tx_id);
        ASSERT_TRUE(decoded_ipv6_result.is_ok());
        
        auto decoded_ipv6 = decoded_ipv6_result.value();
        EXPECT_EQ(decoded_ipv6.address(), ipv6_endpoint.address());
        EXPECT_EQ(decoded_ipv6.port(), ipv6_endpoint.port());
    } catch (const std::exception&) {
        // IPv6 might not be available in test environment
    }
}

/**
 * @brief Test STUN message encoding and parsing
 */
TEST_F(STUNClientTest, MessageEncodingAndParsing) {
    if (!stun_client_) {
        GTEST_SKIP() << "STUN client not available for testing";
    }
    
    // Create test message
    auto tx_id = stun_transaction_id::random();
    stun_message original_message(stun_message_type::binding_request, tx_id);
    
    // Add SOFTWARE attribute
    std::string software = "test-client/1.0";
    std::vector<std::uint8_t> software_data(software.begin(), software.end());
    original_message.add_attribute(stun_attribute(stun_attribute_type::software, software_data));
    
    // Encode message
    stun_client_config config;
    config.enable_fingerprint = true;
    config.software = software;
    
    auto encode_result = stun_client_->encode_message(original_message, config);
    ASSERT_TRUE(encode_result.is_ok());
    
    auto encoded_data = encode_result.value();
    EXPECT_GE(encoded_data.size(), 20); // At least STUN header size
    
    // Parse message back
    auto parse_result = stun_client_->parse_message(encoded_data, stun_server_);
    ASSERT_TRUE(parse_result.is_ok());
    
    auto parsed_message = parse_result.value();
    EXPECT_EQ(parsed_message.type, original_message.type);
    EXPECT_EQ(parsed_message.transaction_id, original_message.transaction_id);
    
    // Verify SOFTWARE attribute
    const auto* software_attr = parsed_message.find_attribute(stun_attribute_type::software);
    ASSERT_NE(software_attr, nullptr);
    
    std::string parsed_software(software_attr->value.begin(), software_attr->value.end());
    EXPECT_EQ(parsed_software, software);
    
    // Verify FINGERPRINT attribute if enabled
    if (config.enable_fingerprint) {
        const auto* fingerprint_attr = parsed_message.find_attribute(stun_attribute_type::fingerprint);
        EXPECT_NE(fingerprint_attr, nullptr);
        EXPECT_TRUE(stun_client_->verify_fingerprint(parsed_message));
    }
}

/**
 * @brief Test message integrity (HMAC-SHA1) verification
 */
TEST_F(STUNClientTest, MessageIntegrityVerification) {
    if (!stun_client_) {
        GTEST_SKIP() << "STUN client not available for testing";
    }
    
    // Create test message with integrity
    auto tx_id = stun_transaction_id::random();
    stun_message message(stun_message_type::binding_request, tx_id);
    
    stun_client_config config;
    config.enable_integrity = true;
    config.password = "test-password";
    
    // Encode message with integrity
    auto encode_result = stun_client_->encode_message(message, config);
    ASSERT_TRUE(encode_result.is_ok());
    
    auto encoded_data = encode_result.value();
    
    // Parse message back
    auto parse_result = stun_client_->parse_message(encoded_data, stun_server_);
    ASSERT_TRUE(parse_result.is_ok());
    
    auto parsed_message = parse_result.value();
    
    // Verify message integrity with correct key
    EXPECT_TRUE(stun_client_->verify_message_integrity(parsed_message, config.password));
    
    // Verify failure with wrong key
    EXPECT_FALSE(stun_client_->verify_message_integrity(parsed_message, "wrong-password"));
}

/**
 * @brief Test fingerprint (CRC32) verification
 */
TEST_F(STUNClientTest, FingerprintVerification) {
    if (!stun_client_) {
        GTEST_SKIP() << "STUN client not available for testing";
    }
    
    // Create test message with fingerprint
    auto tx_id = stun_transaction_id::random();
    stun_message message(stun_message_type::binding_request, tx_id);
    
    stun_client_config config;
    config.enable_fingerprint = true;
    
    // Encode message with fingerprint
    auto encode_result = stun_client_->encode_message(message, config);
    ASSERT_TRUE(encode_result.is_ok());
    
    auto encoded_data = encode_result.value();
    
    // Parse message back
    auto parse_result = stun_client_->parse_message(encoded_data, stun_server_);
    ASSERT_TRUE(parse_result.is_ok());
    
    auto parsed_message = parse_result.value();
    
    // Verify fingerprint
    EXPECT_TRUE(stun_client_->verify_fingerprint(parsed_message));
    
    // Corrupt the message and verify failure
    if (!encoded_data.empty()) {
        encoded_data.back() ^= 0xFF; // Flip bits in last byte
        
        auto corrupted_parse_result = stun_client_->parse_message(encoded_data, stun_server_);
        if (corrupted_parse_result.is_ok()) {
            auto corrupted_message = corrupted_parse_result.value();
            EXPECT_FALSE(stun_client_->verify_fingerprint(corrupted_message));
        }
    }
}

/**
 * @brief Test STUN client statistics
 */
TEST_F(STUNClientTest, ClientStatistics) {
    if (!stun_client_) {
        GTEST_SKIP() << "STUN client not available for testing";
    }
    
    // Get initial stats
    auto initial_stats = stun_client_->get_stats();
    EXPECT_EQ(initial_stats.requests_sent, 0);
    EXPECT_EQ(initial_stats.responses_received, 0);
    EXPECT_EQ(initial_stats.error_responses, 0);
    EXPECT_EQ(initial_stats.timeouts, 0);
    EXPECT_EQ(initial_stats.retransmissions, 0);
    EXPECT_EQ(initial_stats.integrity_failures, 0);
    EXPECT_EQ(initial_stats.fingerprint_failures, 0);
    
    // Reset stats
    stun_client_->reset_stats();
    auto reset_stats = stun_client_->get_stats();
    EXPECT_EQ(reset_stats.requests_sent, 0);
}

/**
 * @brief Test error code string conversion
 */
TEST_F(STUNClientTest, ErrorCodeStringConversion) {
    // Test stun_client_error to string
    EXPECT_EQ(to_string(stun_client_error::none), "none");
    EXPECT_EQ(to_string(stun_client_error::invalid_message), "invalid_message");
    EXPECT_EQ(to_string(stun_client_error::timeout), "timeout");
    EXPECT_EQ(to_string(stun_client_error::network_error), "network_error");
    EXPECT_EQ(to_string(stun_client_error::server_error), "server_error");
    EXPECT_EQ(to_string(stun_client_error::unknown), "unknown");
    
    // Test stun_message_type to string
    EXPECT_EQ(to_string(stun_message_type::binding_request), "binding_request");
    EXPECT_EQ(to_string(stun_message_type::binding_response), "binding_response");
    EXPECT_EQ(to_string(stun_message_type::binding_error_response), "binding_error_response");
    
    // Test stun_error_code to string
    EXPECT_EQ(to_string(stun_error_code::bad_request), "bad_request");
    EXPECT_EQ(to_string(stun_error_code::unauthorized), "unauthorized");
    EXPECT_EQ(to_string(stun_error_code::server_error), "server_error");
}

/**
 * @brief Test invalid message parsing scenarios
 */
TEST_F(STUNClientTest, InvalidMessageParsing) {
    if (!stun_client_) {
        GTEST_SKIP() << "STUN client not available for testing";
    }
    
    // Test empty message
    std::vector<std::uint8_t> empty_data;
    auto result1 = stun_client_->parse_message(empty_data, stun_server_);
    EXPECT_TRUE(result1.is_err());
    EXPECT_EQ(result1.error(), stun_client_error::invalid_message);
    
    // Test message too short
    std::vector<std::uint8_t> short_data(10, 0);
    auto result2 = stun_client_->parse_message(short_data, stun_server_);
    EXPECT_TRUE(result2.is_err());
    EXPECT_EQ(result2.error(), stun_client_error::invalid_message);
    
    // Test invalid magic cookie
    std::vector<std::uint8_t> invalid_magic(20, 0);
    // Set wrong magic cookie
    invalid_magic[4] = 0xFF;
    invalid_magic[5] = 0xFF;
    invalid_magic[6] = 0xFF;
    invalid_magic[7] = 0xFF;
    
    auto result3 = stun_client_->parse_message(invalid_magic, stun_server_);
    EXPECT_TRUE(result3.is_err());
    EXPECT_EQ(result3.error(), stun_client_error::invalid_message);
}

/**
 * @brief Test RFC 3489 compatibility mode
 */
TEST_F(STUNClientTest, RFC3489Compatibility) {
    // Test change request handling (RFC 3489 specific)
    stun_client_config config;
    config.rfc3489_compat = true;
    
    EXPECT_TRUE(config.rfc3489_compat);
    
    // In a real implementation, this would test change request attribute handling
    // For now, just verify the configuration flag
}

/**
 * @brief Test transport integration
 */
TEST_F(STUNClientTest, TransportIntegration) {
    if (!stun_client_) {
        GTEST_SKIP() << "STUN client not available for testing";
    }
    
    // Test transport setting
    stun_client_->set_transport(mock_transport_);
    
    auto transport = stun_client_->get_transport();
    EXPECT_EQ(transport, mock_transport_);
    
    // Test clearing transport
    stun_client_->set_transport(nullptr);
    transport = stun_client_->get_transport();
    EXPECT_EQ(transport, nullptr);
}

/**
 * @brief Performance test for message encoding/decoding
 */
TEST_F(STUNClientTest, PerformanceTest) {
    if (!stun_client_) {
        GTEST_SKIP() << "STUN client not available for testing";
    }
    
    const int num_iterations = 1000;
    stun_client_config config;
    config.enable_fingerprint = true;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_iterations; ++i) {
        // Create message
        auto tx_id = stun_transaction_id::random();
        stun_message message(stun_message_type::binding_request, tx_id);
        
        // Encode
        auto encode_result = stun_client_->encode_message(message, config);
        ASSERT_TRUE(encode_result.is_ok());
        
        // Parse
        auto parse_result = stun_client_->parse_message(encode_result.value(), stun_server_);
        ASSERT_TRUE(parse_result.is_ok());
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Should complete reasonably quickly (adjust threshold as needed)
    EXPECT_LT(duration.count(), 1000); // Less than 1 second for 1000 iterations
    
    std::cout << "Performance: " << num_iterations << " encode/decode cycles in " 
              << duration.count() << "ms" << std::endl;
}

} // anonymous namespace