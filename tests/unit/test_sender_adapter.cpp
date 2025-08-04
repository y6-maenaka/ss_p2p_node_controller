#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <ss_p2p/sender.hpp>
#include <ss_p2p/network/i_transport.hpp>
#include <ss_p2p/message.hpp>

#include <memory>
#include <chrono>
#include <future>

namespace ss::test {

/**
 * @brief Mock transport for testing sender adapter
 */
class mock_transport : public network::i_transport {
public:
    MOCK_METHOD(core::async_result<core::result<void, network::transport_error>>,
                bind, (const core::endpoint& local_endpoint), (override));
    
    MOCK_METHOD(core::async_result<core::result<core::endpoint, network::transport_error>>,
                connect, (const core::endpoint& remote_endpoint), (override));
    
    MOCK_METHOD(core::async_result<core::result<void, network::transport_error>>,
                send, (network::outgoing_message message), (override));
    
    MOCK_METHOD(void, send_async, 
                (network::outgoing_message message, network::send_completion_handler handler), 
                (override));
    
    MOCK_METHOD(core::async_result<core::result<network::incoming_message, network::transport_error>>,
                receive, (std::optional<std::chrono::milliseconds> timeout), (override));
    
    MOCK_METHOD(void, set_message_handler, (network::message_handler handler), (override));
    MOCK_METHOD(void, clear_message_handler, (), (override));
    
    MOCK_METHOD(core::endpoint, local_endpoint, (), (const, noexcept, override));
    MOCK_METHOD(core::endpoint, remote_endpoint, (), (const, noexcept, override));
    MOCK_METHOD(bool, is_connection_based, (), (const, noexcept, override));
    MOCK_METHOD(bool, is_connected, (), (const, noexcept, override));
    
    MOCK_METHOD(network::transport_stats, get_stats, (), (const, noexcept, override));
    MOCK_METHOD(void, reset_stats, (), (noexcept, override));
    
    MOCK_METHOD(network::transport_config, get_config, (), (const, noexcept, override));
    MOCK_METHOD(core::result<void, network::transport_error>, 
                set_config, (const network::transport_config& config), (override));
    
    MOCK_METHOD(std::uint32_t, max_message_size, (), (const, noexcept, override));
    MOCK_METHOD(bool, can_send, (), (const, noexcept, override));
    MOCK_METHOD(std::uint32_t, send_queue_size, (), (const, noexcept, override));
    MOCK_METHOD(std::uint32_t, receive_queue_size, (), (const, noexcept, override));
    
    MOCK_METHOD(core::async_void, disconnect, (), (override));
    MOCK_METHOD(core::async_void, shutdown, (std::chrono::milliseconds timeout), (override));
    
    // i_component interface
    MOCK_METHOD(core::async_result<core::result<void, std::error_code>>, 
                start, (), (override));
    MOCK_METHOD(void, stop, (), (override));
    MOCK_METHOD(bool, is_running, (), (const, noexcept, override));
};

/**
 * @brief Test fixture for sender adapter
 */
class SenderAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_transport_ = std::make_shared<mock_transport>();
        app_id_ = message::app_id(42);
        sender_ = std::make_unique<sender>(mock_transport_, app_id_);
    }

    void TearDown() override {
        sender_.reset();
        mock_transport_.reset();
    }

    std::shared_ptr<mock_transport> mock_transport_;
    message::app_id app_id_;
    std::unique_ptr<sender> sender_;
};

/**
 * @brief Test sender construction
 */
TEST_F(SenderAdapterTest, Construction) {
    EXPECT_NE(sender_, nullptr);
    EXPECT_EQ(sender_->get_transport(), mock_transport_);
}

/**
 * @brief Test sender construction with null transport
 */
TEST(SenderAdapterDeathTest, ConstructionWithNullTransport) {
    message::app_id app_id(42);
    EXPECT_THROW(sender(nullptr, app_id), std::invalid_argument);
}

/**
 * @brief Test async send with JSON payload
 */
TEST_F(SenderAdapterTest, AsyncSendWithJsonPayload) {
    boost::asio::ip::udp::endpoint dest_ep(
        boost::asio::ip::address::from_string("127.0.0.1"), 8080);
    std::string param = "test_param";
    nlohmann::json payload = {{"key", "value"}, {"number", 42}};
    
    bool handler_called = false;
    boost::system::error_code result_ec;
    std::size_t bytes_transferred = 0;
    
    // Set up mock expectation
    EXPECT_CALL(*mock_transport_, send_async(::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](network::outgoing_message msg, 
                                     network::send_completion_handler handler) {
            // Simulate successful send
            handler(core::result<void, network::transport_error>::success());
        }));
    
    sender_->async_send(dest_ep, param, payload, 
        [&](const boost::system::error_code& ec, std::size_t bytes) {
            handler_called = true;
            result_ec = ec;
            bytes_transferred = bytes;
        });
    
    EXPECT_TRUE(handler_called);
    EXPECT_FALSE(result_ec);
    EXPECT_EQ(bytes_transferred, 0); // Mock doesn't set actual bytes
}

/**
 * @brief Test async send with message object
 */
TEST_F(SenderAdapterTest, AsyncSendWithMessage) {
    boost::asio::ip::udp::endpoint dest_ep(
        boost::asio::ip::address::from_string("192.168.1.100"), 9090);
    
    message msg(app_id_);
    msg.set_param("test_message", nlohmann::json{{"data", "test"}});
    
    bool handler_called = false;
    boost::system::error_code result_ec;
    
    // Set up mock expectation
    EXPECT_CALL(*mock_transport_, send_async(::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](network::outgoing_message msg, 
                                     network::send_completion_handler handler) {
            // Simulate successful send
            handler(core::result<void, network::transport_error>::success());
        }));
    
    sender_->async_send(dest_ep, msg, 
        [&](const boost::system::error_code& ec, std::size_t bytes) {
            handler_called = true;
            result_ec = ec;
        });
    
    EXPECT_TRUE(handler_called);
    EXPECT_FALSE(result_ec);
}

/**
 * @brief Test async send with transport error
 */
TEST_F(SenderAdapterTest, AsyncSendWithTransportError) {
    boost::asio::ip::udp::endpoint dest_ep(
        boost::asio::ip::address::from_string("10.0.0.1"), 7777);
    std::string param = "error_test";
    nlohmann::json payload = {{"test", true}};
    
    bool handler_called = false;
    boost::system::error_code result_ec;
    
    // Set up mock expectation for transport error
    EXPECT_CALL(*mock_transport_, send_async(::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](network::outgoing_message msg, 
                                     network::send_completion_handler handler) {
            // Simulate transport error
            handler(core::result<void, network::transport_error>::failure(
                network::transport_error::network_failure));
        }));
    
    sender_->async_send(dest_ep, param, payload, 
        [&](const boost::system::error_code& ec, std::size_t bytes) {
            handler_called = true;
            result_ec = ec;
        });
    
    EXPECT_TRUE(handler_called);
    EXPECT_TRUE(result_ec); // Should have error
    EXPECT_EQ(result_ec, boost::asio::error::network_down);
}

/**
 * @brief Test synchronous send success
 */
TEST_F(SenderAdapterTest, SyncSendSuccess) {
    boost::asio::ip::udp::endpoint dest_ep(
        boost::asio::ip::address::from_string("172.16.0.1"), 5555);
    
    message msg(app_id_);
    msg.set_param("sync_test", nlohmann::json{{"sync", true}});
    
    // Set up mock expectation
    EXPECT_CALL(*mock_transport_, send_async(::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](network::outgoing_message msg, 
                                     network::send_completion_handler handler) {
            // Simulate successful send with some delay to test synchronization
            std::thread([handler]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                handler(core::result<void, network::transport_error>::success());
            }).detach();
        }));
    
    bool result = sender_->sync_send(dest_ep, msg);
    EXPECT_TRUE(result);
}

/**
 * @brief Test synchronous send failure
 */
TEST_F(SenderAdapterTest, SyncSendFailure) {
    boost::asio::ip::udp::endpoint dest_ep(
        boost::asio::ip::address::from_string("203.0.113.1"), 3333);
    std::string param = "sync_fail_test";
    nlohmann::json payload = {{"should_fail", true}};
    
    // Set up mock expectation for failure
    EXPECT_CALL(*mock_transport_, send_async(::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](network::outgoing_message msg, 
                                     network::send_completion_handler handler) {
            handler(core::result<void, network::transport_error>::failure(
                network::transport_error::timeout));
        }));
    
    bool result = sender_->sync_send(dest_ep, param, payload);
    EXPECT_FALSE(result);
}

/**
 * @brief Test is_ready status
 */
TEST_F(SenderAdapterTest, IsReady) {
    // Test when transport is connected
    EXPECT_CALL(*mock_transport_, is_connected())
        .WillOnce(::testing::Return(true));
    
    EXPECT_TRUE(sender_->is_ready());
    
    // Test when transport is not connected
    EXPECT_CALL(*mock_transport_, is_connected())
        .WillOnce(::testing::Return(false));
    
    EXPECT_FALSE(sender_->is_ready());
}

/**
 * @brief Test error code conversion
 */
TEST_F(SenderAdapterTest, ErrorCodeConversion) {
    // Test various transport errors are converted correctly
    struct ErrorTestCase {
        network::transport_error transport_error;
        boost::system::error_code expected_boost_error;
    };
    
    std::vector<ErrorTestCase> test_cases = {
        {network::transport_error::none, boost::system::error_code{}},
        {network::transport_error::network_failure, boost::asio::error::network_down},
        {network::transport_error::connection_refused, boost::asio::error::connection_refused},
        {network::transport_error::timeout, boost::asio::error::timed_out},
        {network::transport_error::host_unreachable, boost::asio::error::host_unreachable}
    };
    
    for (const auto& test_case : test_cases) {
        boost::asio::ip::udp::endpoint dest_ep(
            boost::asio::ip::address::from_string("127.0.0.1"), 8080);
        std::string param = "error_test";
        nlohmann::json payload = {{"error_type", static_cast<int>(test_case.transport_error)}};
        
        boost::system::error_code result_ec;
        bool handler_called = false;
        
        EXPECT_CALL(*mock_transport_, send_async(::testing::_, ::testing::_))
            .WillOnce(::testing::Invoke([test_case](network::outgoing_message msg, 
                                       network::send_completion_handler handler) {
                if (test_case.transport_error == network::transport_error::none) {
                    handler(core::result<void, network::transport_error>::success());
                } else {
                    handler(core::result<void, network::transport_error>::failure(
                        test_case.transport_error));
                }
            }));
        
        sender_->async_send(dest_ep, param, payload, 
            [&](const boost::system::error_code& ec, std::size_t bytes) {
                handler_called = true;
                result_ec = ec;
            });
        
        EXPECT_TRUE(handler_called);
        if (test_case.transport_error == network::transport_error::none) {
            EXPECT_FALSE(result_ec);
        } else {
            EXPECT_EQ(result_ec, test_case.expected_boost_error);
        }
    }
}

} // namespace ss::test