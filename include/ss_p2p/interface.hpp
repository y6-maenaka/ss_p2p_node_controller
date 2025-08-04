#pragma once

#include <ss_p2p/core/types.hpp>
#include <logger/logger.hpp>

#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>
#include <boost/asio.hpp>

namespace ss {

/**
 * @brief Legacy command line interface (deprecated - minimal implementation)
 * 
 * This class provides minimal backward compatibility for legacy test interfaces.
 * New code should use the application layer's service interfaces.
 */
class interface {
public:
    using input_command_callback = std::function<void(std::vector<std::string>)>;

    /**
     * @brief Constructor
     * @param io_ctx IO context for posting commands
     * @param notify_func Callback for handling input commands
     * @param logger_ptr Logger instance (optional)
     */
    interface(core::io_context& io_ctx, 
              const input_command_callback& notify_func, 
              std::shared_ptr<logger::logger> logger_ptr = nullptr);

    /**
     * @brief Destructor
     */
    ~interface();

    /**
     * @brief Start listening to standard input (deprecated)
     * 
     * In minimal implementation, this does nothing.
     * Use application layer interfaces instead.
     */
    void listen_stdin();

    /**
     * @brief Stop the interface
     */
    void stop();

    /**
     * @brief Check if interface is running
     * @return true if active, false otherwise
     */
    bool is_running() const noexcept;

private:
    /// Background thread for input processing
    std::thread _input_thread;
    /// IO context reference
    core::io_context& _io_ctx;
    /// Command callback function
    input_command_callback _notify_func;
    /// Logger instance
    std::shared_ptr<logger::logger> _logger;
    /// Thread safety mutex
    mutable std::mutex _mutex;
    /// Running flag
    std::atomic<bool> _running{false};
    /// Stop flag
    std::atomic<bool> _stop_requested{false};

    /**
     * @brief Internal input processing loop
     */
    void input_processing_loop();
};

} // namespace ss 


