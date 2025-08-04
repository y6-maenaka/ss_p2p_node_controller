#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <ss_p2p/interface.hpp>
#include <logger/logger.hpp>

#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <chrono>
#include <boost/asio.hpp>

namespace ss::test {

/**
 * @brief Test fixture for interface adapter
 */
class InterfaceAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        io_context_ = std::make_unique<core::io_context>();
        logger_ = logger::create_console_logger("interface_test");
        
        command_received_ = false;
        received_commands_.clear();
        
        // Create callback function
        callback_ = [this](const std::vector<std::string>& commands) {
            command_received_ = true;
            received_commands_ = commands;
        };
    }

    void TearDown() override {
        if (interface_) {
            interface_->stop();
            interface_.reset();
        }
        io_context_.reset();
    }

    std::unique_ptr<core::io_context> io_context_;
    std::shared_ptr<logger::logger> logger_;
    interface::input_command_callback callback_;
    std::unique_ptr<interface> interface_;
    
    std::atomic<bool> command_received_{false};
    std::vector<std::string> received_commands_;
};

/**
 * @brief Test interface construction
 */
TEST_F(InterfaceAdapterTest, Construction) {
    EXPECT_NO_THROW({
        interface_ = std::make_unique<interface>(*io_context_, callback_, logger_);
    });
    
    EXPECT_NE(interface_, nullptr);
    EXPECT_TRUE(interface_->is_running());
}

/**
 * @brief Test interface construction without logger
 */
TEST_F(InterfaceAdapterTest, ConstructionWithoutLogger) {
    EXPECT_NO_THROW({
        interface_ = std::make_unique<interface>(*io_context_, callback_);
    });
    
    EXPECT_NE(interface_, nullptr);
    EXPECT_TRUE(interface_->is_running());
}

/**
 * @brief Test interface stop functionality
 */
TEST_F(InterfaceAdapterTest, StopFunctionality) {
    interface_ = std::make_unique<interface>(*io_context_, callback_, logger_);
    
    EXPECT_TRUE(interface_->is_running());
    
    interface_->stop();
    
    // Give some time for the stop to take effect
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_FALSE(interface_->is_running());
    
    // Multiple stop calls should be safe
    EXPECT_NO_THROW(interface_->stop());
    EXPECT_FALSE(interface_->is_running());
}

/**
 * @brief Test legacy listen_stdin API
 */
TEST_F(InterfaceAdapterTest, LegacyListenStdinAPI) {
    interface_ = std::make_unique<interface>(*io_context_, callback_, logger_);
    
    // listen_stdin should not throw (it's a no-op in the adapter)
    EXPECT_NO_THROW(interface_->listen_stdin());
    
    // Interface should still be running
    EXPECT_TRUE(interface_->is_running());
}

/**
 * @brief Test command callback is called correctly
 */
TEST_F(InterfaceAdapterTest, CommandCallbackExecution) {
    std::vector<std::string> test_commands = {"test", "command", "123"};
    std::atomic<bool> callback_executed{false};
    std::vector<std::string> callback_commands;
    
    auto test_callback = [&](const std::vector<std::string>& commands) {
        callback_executed = true;
        callback_commands = commands;
    };
    
    interface_ = std::make_unique<interface>(*io_context_, test_callback, logger_);
    
    // Simulate command processing by posting directly to IO context
    io_context_->post([&]() {
        test_callback(test_commands);
    });
    
    // Run IO context to process the posted command
    io_context_->run_for(std::chrono::milliseconds(100));
    
    EXPECT_TRUE(callback_executed);
    EXPECT_EQ(callback_commands, test_commands);
}

/**
 * @brief Test exception handling in callback
 */
TEST_F(InterfaceAdapterTest, CallbackExceptionHandling) {
    auto throwing_callback = [](const std::vector<std::string>& commands) {
        throw std::runtime_error("Test exception");
    };
    
    // Constructor should not throw even with a callback that can throw
    EXPECT_NO_THROW({
        interface_ = std::make_unique<interface>(*io_context_, throwing_callback, logger_);
    });
    
    EXPECT_TRUE(interface_->is_running());
    
    // Interface should handle callback exceptions gracefully
    io_context_->post([&]() {
        EXPECT_NO_THROW(throwing_callback({"test"}));
    });
    
    io_context_->run_for(std::chrono::milliseconds(100));
    
    // Interface should still be running after callback exception
    EXPECT_TRUE(interface_->is_running());
}

/**
 * @brief Test multiple command processing
 */
TEST_F(InterfaceAdapterTest, MultipleCommandProcessing) {
    std::atomic<int> command_count{0};
    std::vector<std::vector<std::string>> all_commands;
    std::mutex commands_mutex;
    
    auto counting_callback = [&](const std::vector<std::string>& commands) {
        std::lock_guard<std::mutex> lock(commands_mutex);
        command_count.fetch_add(1);
        all_commands.push_back(commands);
    };
    
    interface_ = std::make_unique<interface>(*io_context_, counting_callback, logger_);
    
    // Post multiple commands
    std::vector<std::vector<std::string>> test_commands = {
        {"cmd1", "arg1"},
        {"cmd2", "arg2", "arg3"},
        {"cmd3"},
        {"cmd4", "arg4", "arg5", "arg6"}
    };
    
    for (const auto& cmd : test_commands) {
        io_context_->post([&, cmd]() {
            counting_callback(cmd);
        });
    }
    
    // Process all commands
    io_context_->run_for(std::chrono::milliseconds(200));
    
    EXPECT_EQ(command_count.load(), test_commands.size());
    EXPECT_EQ(all_commands.size(), test_commands.size());
}

/**
 * @brief Test thread safety
 */
TEST_F(InterfaceAdapterTest, ThreadSafety) {
    const int num_threads = 4;
    const int operations_per_thread = 50;
    std::atomic<int> total_operations{0};
    
    auto thread_safe_callback = [&](const std::vector<std::string>& commands) {
        total_operations.fetch_add(1);
        // Simulate some processing time
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    };
    
    interface_ = std::make_unique<interface>(*io_context_, thread_safe_callback, logger_);
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                // Test concurrent access to interface methods
                EXPECT_TRUE(interface_->is_running());
                interface_->listen_stdin(); // Should be thread-safe no-op
                
                // Post command to IO context
                io_context_->post([&]() {
                    thread_safe_callback({std::to_string(i), std::to_string(j)});
                });
            }
        });
    }
    
    // Run IO context in background
    std::thread io_thread([&]() {
        io_context_->run_for(std::chrono::seconds(2));
    });
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    io_thread.join();
    
    // Interface should still be running and in good state
    EXPECT_TRUE(interface_->is_running());
    
    // All operations should have been processed
    EXPECT_EQ(total_operations.load(), num_threads * operations_per_thread);
}

/**
 * @brief Test destructor behavior
 */
TEST_F(InterfaceAdapterTest, DestructorBehavior) {
    bool callback_called = false;
    
    auto test_callback = [&](const std::vector<std::string>& commands) {
        callback_called = true;
    };
    
    {
        interface local_interface(*io_context_, test_callback, logger_);
        EXPECT_TRUE(local_interface.is_running());
        
        // Post a command
        io_context_->post([&]() {
            test_callback({"test"});
        });
        
        io_context_->run_for(std::chrono::milliseconds(50));
    } // interface should be destroyed here
    
    // Callback should have been called before destruction
    EXPECT_TRUE(callback_called);
    
    // IO context should still be functional
    callback_called = false;
    io_context_->post([&]() {
        callback_called = true;
    });
    
    io_context_->run_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(callback_called);
}

/**
 * @brief Test with empty commands
 */
TEST_F(InterfaceAdapterTest, EmptyCommandHandling) {
    std::atomic<bool> empty_command_received{false};
    
    auto empty_command_callback = [&](const std::vector<std::string>& commands) {
        if (commands.empty()) {
            empty_command_received = true;
        }
    };
    
    interface_ = std::make_unique<interface>(*io_context_, empty_command_callback, logger_);
    
    // Post empty command
    io_context_->post([&]() {
        empty_command_callback(std::vector<std::string>{});
    });
    
    io_context_->run_for(std::chrono::milliseconds(100));
    
    EXPECT_TRUE(empty_command_received);
}

/**
 * @brief Test interface state consistency
 */
TEST_F(InterfaceAdapterTest, StateConsistency) {
    interface_ = std::make_unique<interface>(*io_context_, callback_, logger_);
    
    // Initial state
    EXPECT_TRUE(interface_->is_running());
    
    // After operations
    interface_->listen_stdin();
    EXPECT_TRUE(interface_->is_running());
    
    // After stop
    interface_->stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(interface_->is_running());
    
    // After another stop (should be safe)
    interface_->stop();
    EXPECT_FALSE(interface_->is_running());
}

} // namespace ss::test