#pragma once

#include <logger/logger.hpp>
#include <ss_p2p/core/types.hpp>

#include <memory>
#include <string>
#include <mutex>
#include <sstream>
#include <boost/asio.hpp>

// Legacy logging macros for backward compatibility
#ifndef SS_LOGGING_DISABLE
#define SS_LOGGER_SYSTEM_OUTFILE_NAME "ss_system"
#define SS_LOGGER_PACKET_OUTFILE_NAME "ss_packet"
#endif

namespace ss {

/**
 * @brief Legacy logger adapter (deprecated - minimal implementation)
 * 
 * This class provides backward compatibility with the original ss_logger API
 * while internally using the new logger system. Thread-safe for concurrent operations.
 */
class ss_logger {
public:
    /**
     * @brief Packet direction enumeration (legacy compatibility)
     */
    enum packet_direction {
        INCOMING,  // Incoming packet
        OUTGOING   // Outgoing packet
    };

    /**
     * @brief Constructor with modern logger backend
     */
    ss_logger();

    /**
     * @brief Destructor
     */
    ~ss_logger() = default;

    /**
     * @brief Log general system message (legacy API)
     * @param ll Log level from old logger system
     * @param args Message arguments
     */
    template<typename... Args>
    void log(const logger::log_level& ll, Args&&... args) {
        std::lock_guard<std::mutex> lock(_mutex);
        
        if (!_system_logger) {
            return; // Logging disabled
        }

        // Convert legacy log level to new logger level and log
        auto level = convert_log_level(ll);
        std::string message = format_args(std::forward<Args>(args)...);
        _system_logger->log(level, message);
    }

    /**
     * @brief Log packet message (legacy API)
     * @param ll Log level from old logger system
     * @param pd Packet direction
     * @param ep Network endpoint
     * @param args Message arguments
     */
    template<typename... Args>
    void log_packet(const logger::log_level& ll, 
                   const packet_direction& pd, 
                   const boost::asio::ip::udp::endpoint& ep, 
                   Args&&... args) {
        std::lock_guard<std::mutex> lock(_mutex);
        
        if (!_packet_logger) {
            return; // Packet logging disabled
        }

        // Convert legacy log level and format packet message
        auto level = convert_log_level(ll);
        std::string direction = (pd == INCOMING) ? "(receive)" : "(send)";
        std::string endpoint_str = ep.address().to_string() + ":" + std::to_string(ep.port());
        std::string message = format_args(std::forward<Args>(args)...);
        
        _packet_logger->log(level, "{}: {} {}", direction, endpoint_str, message);
    }

    /**
     * @brief Check if system logging is enabled
     * @return true if enabled, false otherwise
     */
    bool is_system_logging_enabled() const noexcept {
        std::lock_guard<std::mutex> lock(_mutex);
        return _system_logger != nullptr;
    }

    /**
     * @brief Check if packet logging is enabled
     * @return true if enabled, false otherwise
     */
    bool is_packet_logging_enabled() const noexcept {
        std::lock_guard<std::mutex> lock(_mutex);
        return _packet_logger != nullptr;
    }

    /**
     * @brief Get underlying system logger (for advanced usage)
     * @return Shared pointer to system logger
     */
    std::shared_ptr<logger> get_system_logger() const noexcept {
        std::lock_guard<std::mutex> lock(_mutex);
        return _system_logger;
    }

    /**
     * @brief Get underlying packet logger (for advanced usage)
     * @return Shared pointer to packet logger
     */
    std::shared_ptr<logger> get_packet_logger() const noexcept {
        std::lock_guard<std::mutex> lock(_mutex);
        return _packet_logger;
    }

private:
    /// System logger instance
    std::shared_ptr<logger> _system_logger;
    /// Packet logger instance
    std::shared_ptr<logger> _packet_logger;
    /// Thread safety mutex
    mutable std::mutex _mutex;

    /**
     * @brief Convert legacy log level to new logger level
     * @param ll Legacy log level
     * @return New logger level
     */
    logger::log_level convert_log_level(const logger::log_level& ll) const;

    /**
     * @brief Format variadic arguments into string
     * @param args Arguments to format
     * @return Formatted string
     */
    template<typename... Args>
    std::string format_args(Args&&... args) const {
        std::ostringstream oss;
        ((oss << args << " "), ...);
        std::string result = oss.str();
        if (!result.empty() && result.back() == ' ') {
            result.pop_back(); // Remove trailing space
        }
        return result;
    }
};

} // namespace ss
