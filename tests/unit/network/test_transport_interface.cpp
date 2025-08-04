#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ss_p2p/network/i_transport.hpp"
#include "ss_p2p/core/types.hpp"

#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <chrono>

namespace ss::network::test {

/**
 * @brief Mock implementation of i_transport for testing
 */
class mock_transport : public i_transport {
public:
    explicit mock_transport(ss::core::io_context& io_context)
        : io_context_(io_context)
        , running_(false)
        , bound_(false)
        , connected_(false) {
        stats_.max_message_size = 1024;
    }

    // i_component interface mocks
    std::string name() const noexcept override { return "mock_transport"; }
    std::string version() const noexcept override { return "1.0.0"; }
    bool is_healthy() const noexcept override { return running_; }
    std::string status() const override { return "{}"; }
    
    ss::core::async_void initialize() override { 
        initialized_ = true;
        co_return; 
    }
    
    ss::core::async_void cleanup() override { 
        cleaned_up_ = true;
        co_return; 
    }
    
    ss::core::async_void start() override { 
        running_ = true;
        co_return; 
    }
    
    ss::core::async_void stop() override { 
        running_ = false;
        co_return; 
    }
    
    bool is_running() const noexcept override { return running_; }

    // i_transport interface mocks
    MOCK_METHOD(ss::core::async_result<ss::core::result<void, transport_error>>,
                bind, (const ss::core::endpoint& local_endpoint), (override));
    
    MOCK_METHOD(ss::core::async_result<ss::core::result<ss::core::endpoint, transport_error>>,
                connect, (const ss::core::endpoint& remote_endpoint), (override));
    
    MOCK_METHOD(ss::core::async_result<ss::core::result<void, transport_error>>,
                send, (outgoing_message message), (override));
    
    MOCK_METHOD(void, send_async, 
                (outgoing_message message, send_completion_handler handler), (override));
    
    MOCK_METHOD(ss::core::async_result<ss::core::result<incoming_message, transport_error>>,
                receive, (std::optional<std::chrono::milliseconds> timeout), (override));
    
    MOCK_METHOD(void, set_message_handler, (message_handler handler), (override));
    MOCK_METHOD(void, clear_message_handler, (), (override));

    ss::core::endpoint local_endpoint() const noexcept override { return local_endpoint_; }
    ss::core::endpoint remote_endpoint() const noexcept override { return remote_endpoint_; }
    bool is_connection_based() const noexcept override { return is_connection_based_; }
    bool is_connected() const noexcept override { return connected_; }

    transport_stats get_stats() const noexcept override { return stats_; }
    void reset_stats() noexcept override { stats_ = transport_stats{}; }

    transport_config get_config() const noexcept override { return config_; }
    ss::core::result<void, transport_error> set_config(const transport_config& config) override {
        config_ = config;
        return ss::core::result<void, transport_error>::ok();
    }

    std::uint32_t max_message_size() const noexcept override { return stats_.max_message_size; }
    bool can_send() const noexcept override { return true; }
    std::uint32_t send_queue_size() const noexcept override { return 0; }
    std::uint32_t receive_queue_size() const noexcept override { return 0; }

    ss::core::async_void disconnect() override { 
        connected_ = false;
        co_return; 
    }
    
    ss::core::async_void shutdown(std::chrono::milliseconds timeout) override { 
        running_ = false;
        co_return; 
    }

    // Test helper methods
    void set_local_endpoint(const ss::core::endpoint& ep) { local_endpoint_ = ep; }
    void set_remote_endpoint(const ss::core::endpoint& ep) { remote_endpoint_ = ep; }
    void set_connection_based(bool connection_based) { is_connection_based_ = connection_based; }
    void set_connected(bool connected) { connected_ = connected; }
    void set_bound(bool bound) { bound_ = bound; }
    
    bool is_initialized() const { return initialized_; }
    bool is_cleaned_up() const { return cleaned_up_; }

private:
    ss::core::io_context& io_context_;
    transport_config config_;
    transport_stats stats_;
    ss::core::endpoint local_endpoint_;
    ss::core::endpoint remote_endpoint_;
    bool running_;
    bool bound_;
    bool connected_;
    bool is_connection_based_ = false;
    bool initialized_ = false;
    bool cleaned_up_ = false;
};

/**
 * @brief Test fixture for transport interface tests
 */
class TransportInterfaceTest : public ::testing::Test {
protected:
    void SetUp() override {
        transport_ = std::make_unique<mock_transport>(io_context_);
    }

    void TearDown() override {
        transport_.reset();
    }

    boost::asio::io_context io_context_;
    std::unique_ptr<mock_transport> transport_;
};

// Basic interface tests

TEST_F(TransportInterfaceTest, ComponentInterface) {
    EXPECT_EQ(transport_->name(), "mock_transport");
    EXPECT_EQ(transport_->version(), "1.0.0");
    EXPECT_FALSE(transport_->is_running());
    EXPECT_FALSE(transport_->is_healthy());
}

TEST_F(TransportInterfaceTest, LifecycleManagement) {
    // Test initialization
    boost::asio::co_spawn(io_context_, 
        transport_->initialize(),
        boost::asio::detached
    );
    io_context_.run_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(transport_->is_initialized());

    // Test start
    boost::asio::co_spawn(io_context_, 
        transport_->start(),
        boost::asio::detached
    );
    io_context_.run_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(transport_->is_running());
    EXPECT_TRUE(transport_->is_healthy());

    // Test stop
    boost::asio::co_spawn(io_context_, 
        transport_->stop(),
        boost::asio::detached
    );
    io_context_.run_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(transport_->is_running());

    // Test cleanup
    boost::asio::co_spawn(io_context_, 
        transport_->cleanup(),
        boost::asio::detached
    );
    io_context_.run_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(transport_->is_cleaned_up());
}

TEST_F(TransportInterfaceTest, EndpointManagement) {
    ss::core::endpoint local_ep("127.0.0.1", 8080);
    ss::core::endpoint remote_ep("192.168.1.100", 9090);

    transport_->set_local_endpoint(local_ep);
    transport_->set_remote_endpoint(remote_ep);

    EXPECT_EQ(transport_->local_endpoint(), local_ep);
    EXPECT_EQ(transport_->remote_endpoint(), remote_ep);
}

TEST_F(TransportInterfaceTest, ConnectionState) {
    EXPECT_FALSE(transport_->is_connected());
    
    transport_->set_connected(true);
    EXPECT_TRUE(transport_->is_connected());
    
    // Test disconnect
    boost::asio::co_spawn(io_context_, 
        transport_->disconnect(),
        boost::asio::detached
    );
    io_context_.run_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(transport_->is_connected());
}

TEST_F(TransportInterfaceTest, Configuration) {
    transport_config config;
    config.send_buffer_size = 32768;
    config.receive_buffer_size = 32768;
    config.max_concurrent_ops = 500;
    config.default_timeout = std::chrono::milliseconds{3000};

    auto result = transport_->set_config(config);
    ASSERT_TRUE(result.is_ok());

    auto retrieved_config = transport_->get_config();
    EXPECT_EQ(retrieved_config.send_buffer_size, config.send_buffer_size);
    EXPECT_EQ(retrieved_config.receive_buffer_size, config.receive_buffer_size);
    EXPECT_EQ(retrieved_config.max_concurrent_ops, config.max_concurrent_ops);
    EXPECT_EQ(retrieved_config.default_timeout, config.default_timeout);
}

TEST_F(TransportInterfaceTest, Statistics) {
    auto stats = transport_->get_stats();
    
    // Initial stats should be zero
    EXPECT_EQ(stats.bytes_sent, 0);
    EXPECT_EQ(stats.bytes_received, 0);
    EXPECT_EQ(stats.messages_sent, 0);
    EXPECT_EQ(stats.messages_received, 0);
    EXPECT_EQ(stats.send_errors, 0);
    EXPECT_EQ(stats.receive_errors, 0);

    // Test reset
    transport_->reset_stats();
    stats = transport_->get_stats();
    EXPECT_EQ(stats.bytes_sent, 0);
    EXPECT_EQ(stats.bytes_received, 0);
}

TEST_F(TransportInterfaceTest, MessageSizeConstraints) {
    EXPECT_GT(transport_->max_message_size(), 0);
    EXPECT_TRUE(transport_->can_send());
    EXPECT_EQ(transport_->send_queue_size(), 0);
    EXPECT_EQ(transport_->receive_queue_size(), 0);
}

// Error handling tests

TEST_F(TransportInterfaceTest, ErrorCodeConversion) {
    EXPECT_NE(to_string(transport_error::none), "");
    EXPECT_NE(to_string(transport_error::network_failure), "");
    EXPECT_NE(to_string(transport_error::connection_refused), "");
    EXPECT_NE(to_string(transport_error::timeout), "");
    EXPECT_NE(to_string(transport_error::host_unreachable), "");
}

// Message structure tests

TEST_F(TransportInterfaceTest, OutgoingMessageStructure) {
    std::vector<std::uint8_t> data = {1, 2, 3, 4, 5};
    ss::core::endpoint target("127.0.0.1", 8080);
    
    outgoing_message msg(data, target);
    
    EXPECT_EQ(msg.data, data);
    EXPECT_EQ(msg.target, target);
    EXPECT_EQ(msg.priority, 0);
    EXPECT_FALSE(msg.timeout.has_value());
    EXPECT_TRUE(msg.metadata.empty());
}

TEST_F(TransportInterfaceTest, IncomingMessageStructure) {
    std::vector<std::uint8_t> data = {1, 2, 3, 4, 5};
    ss::core::endpoint sender("192.168.1.100", 9090);
    
    incoming_message msg(data, sender);
    
    EXPECT_EQ(msg.data, data);
    EXPECT_EQ(msg.sender, sender);
    EXPECT_NE(msg.received_at, std::chrono::steady_clock::time_point{});
    EXPECT_TRUE(msg.metadata.empty());
}

// Transport configuration tests

TEST_F(TransportInterfaceTest, TransportConfig) {
    transport_config config;
    
    // Test default values
    EXPECT_GT(config.max_concurrent_ops, 0);
    EXPECT_GT(config.send_buffer_size, 0);
    EXPECT_GT(config.receive_buffer_size, 0);
    EXPECT_GT(config.default_timeout.count(), 0);
    EXPECT_GT(config.keep_alive_interval.count(), 0);
}

TEST_F(TransportInterfaceTest, TransportStats) {
    transport_stats stats;
    
    // Test default values
    EXPECT_EQ(stats.bytes_sent, 0);
    EXPECT_EQ(stats.bytes_received, 0);
    EXPECT_EQ(stats.messages_sent, 0);
    EXPECT_EQ(stats.messages_received, 0);
    EXPECT_EQ(stats.send_errors, 0);
    EXPECT_EQ(stats.receive_errors, 0);
    EXPECT_EQ(stats.avg_send_latency_us, 0);
    EXPECT_EQ(stats.avg_receive_latency_us, 0);
    EXPECT_EQ(stats.active_connections, 0);
    EXPECT_EQ(stats.max_message_size, 0);
}

// Mock interaction tests

TEST_F(TransportInterfaceTest, MockBindExpectation) {
    ss::core::endpoint local_ep("0.0.0.0", 8080);
    
    EXPECT_CALL(*transport_, bind(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(boost::asio::co_spawn(io_context_, 
            []() -> ss::core::async_result<ss::core::result<void, transport_error>> {
                co_return ss::core::result<void, transport_error>::ok();
            }(), boost::asio::use_awaitable)));

    boost::asio::co_spawn(io_context_,
        [this, &local_ep]() -> ss::core::async_void {
            auto result = co_await transport_->bind(local_ep);
            EXPECT_TRUE(result.is_ok());
        },
        boost::asio::detached
    );
    
    io_context_.run_for(std::chrono::milliseconds(100));
}

TEST_F(TransportInterfaceTest, MockSendAsyncExpectation) {
    std::vector<std::uint8_t> data = {1, 2, 3, 4, 5};
    ss::core::endpoint target("127.0.0.1", 8080);
    outgoing_message msg(data, target);
    
    bool callback_called = false;
    
    EXPECT_CALL(*transport_, send_async(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke([&callback_called](outgoing_message, send_completion_handler handler) {
            if (handler) {
                handler(ss::core::result<void, transport_error>::ok());
                callback_called = true;
            }
        }));

    transport_->send_async(std::move(msg), [&callback_called](auto result) {
        EXPECT_TRUE(result.is_ok());
        callback_called = true;
    });
    
    EXPECT_TRUE(callback_called);
}

} // namespace ss::network::test