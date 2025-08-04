#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <ss_p2p/socket_manager.hpp>
#include <ss_p2p/core/types.hpp>

#include <chrono>
#include <thread>
#include <boost/asio.hpp>

namespace ss::test {

class SocketManagerAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        io_context_ = std::make_unique<ss::core::io_context>();
        
        // Create test endpoints
        udp_endpoint_ = boost::asio::ip::udp::endpoint(
            boost::asio::ip::address::from_string("127.0.0.1"), 0);
        tcp_endpoint_ = boost::asio::ip::tcp::endpoint(
            boost::asio::ip::address::from_string("127.0.0.1"), 0);
    }

    void TearDown() override {
        if (io_context_) {
            io_context_->stop();
        }
    }

    std::unique_ptr<ss::core::io_context> io_context_;
    boost::asio::ip::udp::endpoint udp_endpoint_;
    boost::asio::ip::tcp::endpoint tcp_endpoint_;
};

TEST_F(SocketManagerAdapterTest, UdpSocketManagerConstruction) {
    EXPECT_NO_THROW({
        udp_socket_manager manager(udp_endpoint_, *io_context_);
    });
}

TEST_F(SocketManagerAdapterTest, UdpSocketManagerBasicOperations) {
    udp_socket_manager manager(udp_endpoint_, *io_context_);
    
    // Test basic operations
    EXPECT_EQ(&manager.io_ctx(), io_context_.get());
    EXPECT_TRUE(manager.is_open());
    
    // Test socket access
    EXPECT_NO_THROW({
        auto& socket = manager.self_sock();
        EXPECT_TRUE(socket.is_open());
    });
    
    // Test local endpoint
    auto local_ep = manager.local_endpoint();
    EXPECT_EQ(local_ep.address(), udp_endpoint_.address());
}

TEST_F(SocketManagerAdapterTest, UdpSocketManagerStats) {
    udp_socket_manager manager(udp_endpoint_, *io_context_);
    
    // Get initial stats
    auto stats = manager.get_stats();
    EXPECT_GE(stats.bytes_sent, 0);
    EXPECT_GE(stats.bytes_received, 0);
    
    // Reset stats should not throw
    EXPECT_NO_THROW(manager.reset_stats());
}

TEST_F(SocketManagerAdapterTest, UdpSocketManagerClose) {
    udp_socket_manager manager(udp_endpoint_, *io_context_);
    
    EXPECT_TRUE(manager.is_open());
    
    manager.close();
    EXPECT_FALSE(manager.is_open());
    
    // Accessing socket after close should throw
    EXPECT_THROW({
        manager.self_sock();
    }, std::runtime_error);
}

TEST_F(SocketManagerAdapterTest, TcpSocketManagerConstruction) {
    EXPECT_NO_THROW({
        tcp_socket_manager manager(tcp_endpoint_, *io_context_);
    });
}

TEST_F(SocketManagerAdapterTest, TcpSocketManagerBasicOperations) {
    tcp_socket_manager manager(tcp_endpoint_, *io_context_);
    
    // Test basic operations
    EXPECT_EQ(&manager.io_ctx(), io_context_.get());
    EXPECT_TRUE(manager.is_open());
    
    // Test socket access
    EXPECT_NO_THROW({
        auto& socket = manager.self_sock();
        EXPECT_TRUE(socket.is_open());
    });
    
    // Test local endpoint
    auto local_ep = manager.local_endpoint();
    EXPECT_EQ(local_ep.address(), tcp_endpoint_.address());
}

TEST_F(SocketManagerAdapterTest, TcpSocketManagerClose) {
    tcp_socket_manager manager(tcp_endpoint_, *io_context_);
    
    EXPECT_TRUE(manager.is_open());
    
    manager.close();
    EXPECT_FALSE(manager.is_open());
    
    // Accessing socket after close should throw
    EXPECT_THROW({
        manager.self_sock();
    }, std::runtime_error);
}

TEST_F(SocketManagerAdapterTest, GenericSocketManagerUdp) {
    udp_socket_manager udp_manager(udp_endpoint_, *io_context_);
    socket_manager manager(udp_manager);
    
    // Test type detection
    EXPECT_EQ(manager.get_sock_type(), socket_manager::sock_type::udp);
    EXPECT_TRUE(manager.is_udp());
    EXPECT_FALSE(manager.is_tcp());
    
    // Test type casting
    EXPECT_NO_THROW({
        auto& udp_ref = manager.as_udp();
        EXPECT_TRUE(udp_ref.is_open());
    });
    
    // Casting to wrong type should throw
    EXPECT_THROW({
        manager.as_tcp();
    }, std::bad_variant_access);
}

TEST_F(SocketManagerAdapterTest, GenericSocketManagerTcp) {
    tcp_socket_manager tcp_manager(tcp_endpoint_, *io_context_);
    socket_manager manager(tcp_manager);
    
    // Test type detection
    EXPECT_EQ(manager.get_sock_type(), socket_manager::sock_type::tcp);
    EXPECT_FALSE(manager.is_udp());
    EXPECT_TRUE(manager.is_tcp());
    
    // Test type casting
    EXPECT_NO_THROW({
        auto& tcp_ref = manager.as_tcp();
        EXPECT_TRUE(tcp_ref.is_open());
    });
    
    // Casting to wrong type should throw
    EXPECT_THROW({
        manager.as_udp();
    }, std::bad_variant_access);
}

TEST_F(SocketManagerAdapterTest, GenericSocketManagerConst) {
    udp_socket_manager udp_manager(udp_endpoint_, *io_context_);
    const socket_manager manager(udp_manager);
    
    // Test const operations
    EXPECT_EQ(manager.get_sock_type(), socket_manager::sock_type::udp);
    EXPECT_TRUE(manager.is_udp());
    EXPECT_FALSE(manager.is_tcp());
    
    // Test const type casting
    EXPECT_NO_THROW({
        const auto& udp_ref = manager.as_udp();
        EXPECT_TRUE(udp_ref.is_open());
    });
}

TEST_F(SocketManagerAdapterTest, UdpSocketManagerThreadSafety) {
    udp_socket_manager manager(udp_endpoint_, *io_context_);
    
    std::atomic<bool> stop_threads{false};
    std::vector<std::thread> threads;
    
    // Create multiple threads that access manager methods
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&manager, &stop_threads]() {
            while (!stop_threads.load()) {
                manager.is_open();
                manager.local_endpoint();
                manager.get_stats();
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

TEST_F(SocketManagerAdapterTest, UdpSocketManagerDestructorCleanup) {
    {
        udp_socket_manager manager(udp_endpoint_, *io_context_);
        EXPECT_TRUE(manager.is_open());
        // Destructor should clean up automatically
    }
    
    // If we reach here without hanging, the destructor worked correctly
    SUCCEED();
}

TEST_F(SocketManagerAdapterTest, TcpSocketManagerDestructorCleanup) {
    {
        tcp_socket_manager manager(tcp_endpoint_, *io_context_);
        EXPECT_TRUE(manager.is_open());
        // Destructor should clean up automatically
    }
    
    // If we reach here without hanging, the destructor worked correctly
    SUCCEED();
}

TEST_F(SocketManagerAdapterTest, AllowedSocketManagerTypesConcept) {
    // Test that the concept works correctly
    static_assert(AllowedSocketManagerTypes<udp_socket_manager>);
    static_assert(AllowedSocketManagerTypes<tcp_socket_manager>);
    static_assert(!AllowedSocketManagerTypes<int>);
    static_assert(!AllowedSocketManagerTypes<std::string>);
    
    SUCCEED();
}

TEST_F(SocketManagerAdapterTest, AllowedSocketTypesConcept) {
    // Test that the concept works correctly
    static_assert(AllowedSocketTypes<boost::asio::ip::udp::socket>);
    static_assert(AllowedSocketTypes<boost::asio::ip::tcp::socket>);
    static_assert(!AllowedSocketTypes<int>);
    static_assert(!AllowedSocketTypes<std::string>);
    
    SUCCEED();
}

} // namespace ss::test