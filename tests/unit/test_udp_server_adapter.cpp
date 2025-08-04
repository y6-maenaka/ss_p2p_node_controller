#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <ss_p2p/udp_server.hpp>
#include <ss_p2p/core/types.hpp>
#include <logger/logger.hpp>

#include <chrono>
#include <thread>
#include <atomic>
#include <boost/asio.hpp>

namespace ss::test {

class UdpServerAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        io_context_ = std::make_unique<ss::core::io_context>();
        logger_ = std::make_unique<logger>();
        
        // Create a local endpoint for testing
        local_endpoint_ = boost::asio::ip::udp::endpoint(
            boost::asio::ip::address::from_string("127.0.0.1"), 0);
    }

    void TearDown() override {
        if (io_context_) {
            io_context_->stop();
        }
    }

    std::unique_ptr<ss::core::io_context> io_context_;
    std::unique_ptr<logger> logger_;
    boost::asio::ip::udp::endpoint local_endpoint_;
};

TEST_F(UdpServerAdapterTest, Construction) {
    std::atomic<int> message_count{0};
    
    auto handler = [&message_count](std::vector<std::uint8_t> data, 
                                   boost::asio::ip::udp::endpoint& endpoint) {
        message_count.fetch_add(1);
    };

    EXPECT_NO_THROW({
        udp_server server(local_endpoint_, *io_context_, handler, logger_.get());
    });
}

TEST_F(UdpServerAdapterTest, ConstructionWithNullHandler) {
    udp_server::recv_packet_handler null_handler;
    
    EXPECT_THROW({
        udp_server server(local_endpoint_, *io_context_, null_handler, logger_.get());
    }, std::invalid_argument);
}

TEST_F(UdpServerAdapterTest, ComponentInterface) {
    std::atomic<int> message_count{0};
    
    auto handler = [&message_count](std::vector<std::uint8_t> data, 
                                   boost::asio::ip::udp::endpoint& endpoint) {
        message_count.fetch_add(1);
    };

    udp_server server(local_endpoint_, *io_context_, handler, logger_.get());
    
    // Test component interface
    EXPECT_EQ(server.name(), "udp_server_legacy_adapter");
    EXPECT_EQ(server.version(), "2.0.0");
    EXPECT_FALSE(server.is_running());
    EXPECT_FALSE(server.is_healthy()); // Not running, so not healthy
    
    // Status should be valid JSON-like string
    std::string status = server.status();
    EXPECT_FALSE(status.empty());
    EXPECT_NE(status.find("running"), std::string::npos);
}

TEST_F(UdpServerAdapterTest, LegacyStartStop) {
    std::atomic<int> message_count{0};
    
    auto handler = [&message_count](std::vector<std::uint8_t> data, 
                                   boost::asio::ip::udp::endpoint& endpoint) {
        message_count.fetch_add(1);
    };

    udp_server server(local_endpoint_, *io_context_, handler, logger_.get());
    
    // Test legacy start/stop
    EXPECT_TRUE(server.start_legacy());
    
    // Give some time for async operations
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Server should be running after start
    EXPECT_TRUE(server.is_running());
    EXPECT_TRUE(server.is_healthy());
    
    // Test stop
    server.stop_legacy();
    
    // Give some time for async operations
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_FALSE(server.is_running());
}

TEST_F(UdpServerAdapterTest, GetStats) {
    std::atomic<int> message_count{0};
    
    auto handler = [&message_count](std::vector<std::uint8_t> data, 
                                   boost::asio::ip::udp::endpoint& endpoint) {
        message_count.fetch_add(1);
    };

    udp_server server(local_endpoint_, *io_context_, handler, logger_.get());
    
    // Get initial stats
    auto stats = server.get_stats();
    EXPECT_EQ(stats.bytes_sent, 0);
    EXPECT_EQ(stats.bytes_received, 0);
    EXPECT_EQ(stats.messages_sent, 0);
    EXPECT_EQ(stats.messages_received, 0);
    
    // Reset stats should not throw
    EXPECT_NO_THROW(server.reset_stats());
}

TEST_F(UdpServerAdapterTest, LocalEndpoint) {
    std::atomic<int> message_count{0};
    
    auto handler = [&message_count](std::vector<std::uint8_t> data, 
                                   boost::asio::ip::udp::endpoint& endpoint) {
        message_count.fetch_add(1);
    };

    udp_server server(local_endpoint_, *io_context_, handler, logger_.get());
    
    // Local endpoint should be accessible
    auto endpoint = server.local_endpoint();
    EXPECT_EQ(endpoint.address(), local_endpoint_.address());
    // Port might be different if original port was 0 (auto-assigned)
}

TEST_F(UdpServerAdapterTest, CanReceive) {
    std::atomic<int> message_count{0};
    
    auto handler = [&message_count](std::vector<std::uint8_t> data, 
                                   boost::asio::ip::udp::endpoint& endpoint) {
        message_count.fetch_add(1);
    };

    udp_server server(local_endpoint_, *io_context_, handler, logger_.get());
    
    // Initially should not be able to receive
    EXPECT_FALSE(server.can_receive());
    
    // After starting, should be able to receive
    server.start_legacy();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_TRUE(server.can_receive());
    
    server.stop_legacy();
}

TEST_F(UdpServerAdapterTest, ReceiveQueueSize) {
    std::atomic<int> message_count{0};
    
    auto handler = [&message_count](std::vector<std::uint8_t> data, 
                                   boost::asio::ip::udp::endpoint& endpoint) {
        message_count.fetch_add(1);
    };

    udp_server server(local_endpoint_, *io_context_, handler, logger_.get());
    
    // Queue size should be accessible
    EXPECT_GE(server.receive_queue_size(), 0);
}

TEST_F(UdpServerAdapterTest, DestructorCleanup) {
    std::atomic<int> message_count{0};
    
    auto handler = [&message_count](std::vector<std::uint8_t> data, 
                                   boost::asio::ip::udp::endpoint& endpoint) {
        message_count.fetch_add(1);
    };

    {
        udp_server server(local_endpoint_, *io_context_, handler, logger_.get());
        server.start_legacy();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        EXPECT_TRUE(server.is_running());
        // Destructor should clean up automatically
    }
    
    // Give time for cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // If we reach here without hanging, the destructor worked correctly
    SUCCEED();
}

TEST_F(UdpServerAdapterTest, ThreadSafety) {
    std::atomic<int> message_count{0};
    
    auto handler = [&message_count](std::vector<std::uint8_t> data, 
                                   boost::asio::ip::udp::endpoint& endpoint) {
        message_count.fetch_add(1);
    };

    udp_server server(local_endpoint_, *io_context_, handler, logger_.get());
    
    std::atomic<bool> stop_threads{false};
    std::vector<std::thread> threads;
    
    // Create multiple threads that access server methods
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&server, &stop_threads]() {
            while (!stop_threads.load()) {
                server.is_running();
                server.is_healthy();
                server.get_stats();
                server.local_endpoint();
                server.can_receive();
                server.receive_queue_size();
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }
    
    // Let threads run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Stop threads
    stop_threads.store(true);
    for (auto& thread : threads) {
        thread.join();
    }
    
    // If we reach here without deadlock, thread safety is working
    SUCCEED();
}

} // namespace ss::test