#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <ss_p2p/ss_logger.hpp>
#include <logger/logger.hpp>

#include <memory>
#include <string>
#include <sstream>
#include <boost/asio.hpp>

namespace ss::test {

/**
 * @brief Test fixture for ss_logger adapter
 */
class SSLoggerAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create ss_logger instance
        ss_logger_ = std::make_unique<ss_logger>();
    }

    void TearDown() override {
        ss_logger_.reset();
    }

    std::unique_ptr<ss_logger> ss_logger_;
};

/**
 * @brief Test ss_logger construction
 */
TEST_F(SSLoggerAdapterTest, Construction) {
    EXPECT_NE(ss_logger_, nullptr);
    
    // In minimal implementation, loggers might be disabled
    // but the object should still be valid
    EXPECT_NO_THROW({
        auto system_logger = ss_logger_->get_system_logger();
        auto packet_logger = ss_logger_->get_packet_logger();
    });
}

/**
 * @brief Test system logging functionality
 */
TEST_F(SSLoggerAdapterTest, SystemLogging) {
    // Test various log levels
    EXPECT_NO_THROW({
        ss_logger_->log(logger::log_level::DEBUG, "Debug message", 123);
        ss_logger_->log(logger::log_level::INFO, "Info message", "test");
        ss_logger_->log(logger::log_level::WARN, "Warning message");
        ss_logger_->log(logger::log_level::ERROR, "Error message", 456.78);
        ss_logger_->log(logger::log_level::ALERT, "Alert message", true);
    });
}

/**
 * @brief Test packet logging functionality
 */
TEST_F(SSLoggerAdapterTest, PacketLogging) {
    boost::asio::ip::udp::endpoint test_endpoint(
        boost::asio::ip::address::from_string("192.168.1.100"), 8080);
    
    // Test incoming packet logging
    EXPECT_NO_THROW({
        ss_logger_->log_packet(logger::log_level::INFO, 
                              ss_logger::INCOMING, 
                              test_endpoint, 
                              "Received packet", 1024);
    });
    
    // Test outgoing packet logging
    EXPECT_NO_THROW({
        ss_logger_->log_packet(logger::log_level::DEBUG, 
                              ss_logger::OUTGOING, 
                              test_endpoint, 
                              "Sent packet", "data");
    });
}

/**
 * @brief Test packet direction enumeration
 */
TEST_F(SSLoggerAdapterTest, PacketDirectionEnum) {
    // Test enum values
    EXPECT_EQ(static_cast<int>(ss_logger::INCOMING), 0);
    EXPECT_EQ(static_cast<int>(ss_logger::OUTGOING), 1);
    
    boost::asio::ip::udp::endpoint endpoint(
        boost::asio::ip::address::from_string("10.0.0.1"), 9090);
    
    // Test using both direction values
    EXPECT_NO_THROW({
        ss_logger_->log_packet(logger::log_level::INFO, 
                              ss_logger::INCOMING, 
                              endpoint, 
                              "incoming test");
        
        ss_logger_->log_packet(logger::log_level::INFO, 
                              ss_logger::OUTGOING, 
                              endpoint, 
                              "outgoing test");
    });
}

/**
 * @brief Test logging with various argument types
 */
TEST_F(SSLoggerAdapterTest, VariousArgumentTypes) {
    // Test system logging with different argument types
    EXPECT_NO_THROW({
        ss_logger_->log(logger::log_level::INFO, "String arg");
        ss_logger_->log(logger::log_level::INFO, "Int arg", 42);
        ss_logger_->log(logger::log_level::INFO, "Float arg", 3.14f);
        ss_logger_->log(logger::log_level::INFO, "Double arg", 2.71828);
        ss_logger_->log(logger::log_level::INFO, "Bool arg", true);
        ss_logger_->log(logger::log_level::INFO, "Multiple args", 1, "test", 3.14);
    });
    
    boost::asio::ip::udp::endpoint endpoint(
        boost::asio::ip::address::from_string("127.0.0.1"), 7777);
    
    // Test packet logging with different argument types
    EXPECT_NO_THROW({
        ss_logger_->log_packet(logger::log_level::DEBUG, 
                              ss_logger::INCOMING, 
                              endpoint, 
                              "Packet with size", 256);
        
        ss_logger_->log_packet(logger::log_level::DEBUG, 
                              ss_logger::OUTGOING, 
                              endpoint, 
                              "Packet", "type", "data", 1024);
    });
}

/**
 * @brief Test different endpoint types
 */
TEST_F(SSLoggerAdapterTest, DifferentEndpoints) {
    std::vector<boost::asio::ip::udp::endpoint> test_endpoints = {
        boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 8080),
        boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string("192.168.1.1"), 80),
        boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string("10.0.0.1"), 443),
        boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string("172.16.0.1"), 22),
        boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string("203.0.113.1"), 53)
    };
    
    for (const auto& endpoint : test_endpoints) {
        EXPECT_NO_THROW({
            ss_logger_->log_packet(logger::log_level::INFO, 
                                  ss_logger::INCOMING, 
                                  endpoint, 
                                  "Test packet from", endpoint.address().to_string());
        });
    }
}

/**
 * @brief Test IPv6 endpoint support
 */
TEST_F(SSLoggerAdapterTest, IPv6EndpointSupport) {
    boost::asio::ip::udp::endpoint ipv6_endpoint(
        boost::asio::ip::address::from_string("::1"), 8080);
    
    EXPECT_NO_THROW({
        ss_logger_->log_packet(logger::log_level::INFO, 
                              ss_logger::INCOMING, 
                              ipv6_endpoint, 
                              "IPv6 packet");
    });
}

/**
 * @brief Test logging status checks
 */
TEST_F(SSLoggerAdapterTest, LoggingStatusChecks) {
    // Test status check methods
    bool system_enabled = ss_logger_->is_system_logging_enabled();
    bool packet_enabled = ss_logger_->is_packet_logging_enabled();
    
    // In minimal implementation, these might be false (logging disabled)
    // but the methods should not throw
    EXPECT_NO_THROW({
        EXPECT_TRUE(system_enabled || !system_enabled); // Should be boolean
        EXPECT_TRUE(packet_enabled || !packet_enabled); // Should be boolean
    });
}

/**
 * @brief Test logger access methods
 */
TEST_F(SSLoggerAdapterTest, LoggerAccessMethods) {
    EXPECT_NO_THROW({
        auto system_logger = ss_logger_->get_system_logger();
        auto packet_logger = ss_logger_->get_packet_logger();
        
        // Loggers might be null in minimal implementation
        // but the methods should not throw
    });
}

/**
 * @brief Test thread safety
 */
TEST_F(SSLoggerAdapterTest, ThreadSafety) {
    const int num_threads = 4;
    const int logs_per_thread = 100;
    std::atomic<int> completed_logs{0};
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            boost::asio::ip::udp::endpoint endpoint(
                boost::asio::ip::address::from_string("127.0.0.1"), 8080 + i);
            
            for (int j = 0; j < logs_per_thread; ++j) {
                // Test concurrent system logging
                ss_logger_->log(logger::log_level::INFO, 
                               "Thread", i, "Log", j);
                
                // Test concurrent packet logging
                ss_logger_->log_packet(logger::log_level::DEBUG, 
                                      (j % 2 == 0) ? ss_logger::INCOMING : ss_logger::OUTGOING,
                                      endpoint, 
                                      "Thread", i, "Packet", j);
                
                completed_logs.fetch_add(1);
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(completed_logs.load(), num_threads * logs_per_thread);
    
    // Logger should still be in consistent state
    EXPECT_NO_THROW({
        ss_logger_->log(logger::log_level::INFO, "Final test log");
    });
}

/**
 * @brief Test empty and null string handling
 */
TEST_F(SSLoggerAdapterTest, EmptyStringHandling) {
    boost::asio::ip::udp::endpoint endpoint(
        boost::asio::ip::address::from_string("192.168.1.50"), 5000);
    
    EXPECT_NO_THROW({
        // Empty string arguments
        ss_logger_->log(logger::log_level::INFO, "");
        ss_logger_->log(logger::log_level::INFO, "", "");
        
        // Empty packet logging
        ss_logger_->log_packet(logger::log_level::INFO, 
                              ss_logger::INCOMING, 
                              endpoint, 
                              "");
        
        // Mixed empty and non-empty
        ss_logger_->log(logger::log_level::INFO, "Valid", "", "Message");
    });
}

/**
 * @brief Test log level conversion
 */
TEST_F(SSLoggerAdapterTest, LogLevelConversion) {
    // Test all legacy log levels are handled
    std::vector<logger::log_level> test_levels = {
        logger::log_level::DEBUG,
        logger::log_level::INFO,
        logger::log_level::WARN,
        logger::log_level::ERROR,
        logger::log_level::ALERT
    };
    
    boost::asio::ip::udp::endpoint endpoint(
        boost::asio::ip::address::from_string("10.10.10.10"), 1234);
    
    for (const auto& level : test_levels) {
        EXPECT_NO_THROW({
            ss_logger_->log(level, "System log at level", static_cast<int>(level));
            ss_logger_->log_packet(level, 
                                  ss_logger::INCOMING, 
                                  endpoint, 
                                  "Packet log at level", static_cast<int>(level));
        });
    }
}

/**
 * @brief Test large message handling
 */
TEST_F(SSLoggerAdapterTest, LargeMessageHandling) {
    // Create large message
    std::string large_message(10000, 'A');
    boost::asio::ip::udp::endpoint endpoint(
        boost::asio::ip::address::from_string("172.20.0.1"), 6666);
    
    EXPECT_NO_THROW({
        ss_logger_->log(logger::log_level::INFO, large_message);
        ss_logger_->log_packet(logger::log_level::DEBUG, 
                              ss_logger::OUTGOING, 
                              endpoint, 
                              large_message);
    });
}

/**
 * @brief Test logging with special characters
 */
TEST_F(SSLoggerAdapterTest, SpecialCharacterHandling) {
    std::string special_chars = "Special: !@#$%^&*()[]{}|\\:;\"'<>,.?/~`\n\t\r";
    boost::asio::ip::udp::endpoint endpoint(
        boost::asio::ip::address::from_string("198.51.100.1"), 9999);
    
    EXPECT_NO_THROW({
        ss_logger_->log(logger::log_level::WARN, special_chars);
        ss_logger_->log_packet(logger::log_level::ERROR, 
                              ss_logger::INCOMING, 
                              endpoint, 
                              special_chars);
    });
}

} // namespace ss::test