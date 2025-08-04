#pragma once

#include "concepts.hpp"
#include "types.hpp"
#include "result.hpp"

#include <memory>
#include <string>
#include <chrono>
#include <functional>
#include <boost/asio.hpp>

namespace ss::core {

/**
 * @brief Interface for objects that manage their lifecycle asynchronously
 * 
 * Provides standardized start/stop operations for components that need
 * to initialize/cleanup resources asynchronously.
 */
class i_lifecycle {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~i_lifecycle() = default;

    /**
     * @brief Start the component asynchronously
     * @return Awaitable void result
     */
    virtual async_void start() = 0;

    /**
     * @brief Stop the component asynchronously
     * @return Awaitable void result
     */
    virtual async_void stop() = 0;

    /**
     * @brief Check if the component is currently running
     * @return true if running, false otherwise
     */
    virtual bool is_running() const noexcept = 0;

protected:
    /**
     * @brief Protected default constructor
     */
    i_lifecycle() = default;

    /**
     * @brief Protected copy constructor
     */
    i_lifecycle(const i_lifecycle&) = default;

    /**
     * @brief Protected move constructor
     */
    i_lifecycle(i_lifecycle&&) = default;

    /**
     * @brief Protected copy assignment
     */
    i_lifecycle& operator=(const i_lifecycle&) = default;

    /**
     * @brief Protected move assignment
     */
    i_lifecycle& operator=(i_lifecycle&&) = default;
};

/**
 * @brief Interface for system components
 * 
 * Extends i_lifecycle with component identification and error handling.
 * All major system components should implement this interface.
 */
class i_component : public i_lifecycle {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~i_component() = default;

    /**
     * @brief Get the component name
     * @return Component name string
     */
    virtual std::string name() const noexcept = 0;

    /**
     * @brief Get the component version
     * @return Version string
     */
    virtual std::string version() const noexcept { return "1.0.0"; }

    /**
     * @brief Check component health status
     * @return true if healthy, false otherwise
     */
    virtual bool is_healthy() const noexcept { return is_running(); }

    /**
     * @brief Get component statistics or status information
     * @return Status information as string (e.g., JSON)
     */
    virtual std::string status() const { return "{}"; }

    /**
     * @brief Initialize the component with configuration
     * @return Awaitable void result
     */
    virtual async_void initialize() { co_return; }

    /**
     * @brief Cleanup component resources
     * @return Awaitable void result
     */
    virtual async_void cleanup() { co_return; }

protected:
    /**
     * @brief Protected default constructor
     */
    i_component() = default;

    /**
     * @brief Protected copy constructor
     */
    i_component(const i_component&) = default;

    /**
     * @brief Protected move constructor
     */
    i_component(i_component&&) = default;

    /**
     * @brief Protected copy assignment
     */
    i_component& operator=(const i_component&) = default;

    /**
     * @brief Protected move assignment
     */
    i_component& operator=(i_component&&) = default;
};

/**
 * @brief Interface for serializable objects
 * 
 * Provides standardized serialization/deserialization methods for
 * objects that need to be transmitted over the network or stored.
 */
template<typename T>
class i_serializable {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~i_serializable() = default;

    /**
     * @brief Serialize object to byte vector
     * @return Serialized data
     */
    virtual std::vector<std::uint8_t> serialize() const = 0;

    /**
     * @brief Deserialize object from byte vector
     * @param data Serialized data
     * @return Deserialized object or error
     */
    static result<T, std::string> deserialize(const std::vector<std::uint8_t>& data);

    /**
     * @brief Get serialized size estimate
     * @return Estimated size in bytes
     */
    virtual std::size_t serialized_size() const noexcept { return serialize().size(); }

protected:
    /**
     * @brief Protected default constructor
     */
    i_serializable() = default;

    /**
     * @brief Protected copy constructor
     */
    i_serializable(const i_serializable&) = default;

    /**
     * @brief Protected move constructor
     */
    i_serializable(i_serializable&&) = default;

    /**
     * @brief Protected copy assignment
     */
    i_serializable& operator=(const i_serializable&) = default;

    /**
     * @brief Protected move assignment
     */
    i_serializable& operator=(i_serializable&&) = default;
};

/**
 * @brief Interface for message handlers
 * 
 * Defines the contract for objects that can process incoming messages.
 */
template<typename MessageType>
class i_message_handler {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~i_message_handler() = default;

    /**
     * @brief Handle an incoming message
     * @param message The message to handle
     * @param sender_endpoint The endpoint that sent the message
     * @return Awaitable void result
     */
    virtual async_void handle_message(const MessageType& message, 
                                     const endpoint& sender_endpoint) = 0;

    /**
     * @brief Check if this handler can process the given message type
     * @param message The message to check
     * @return true if handler can process, false otherwise
     */
    virtual bool can_handle(const MessageType& message) const noexcept = 0;

    /**
     * @brief Get handler priority (higher values = higher priority)
     * @return Priority value
     */
    virtual std::uint32_t priority() const noexcept { return 0; }

protected:
    /**
     * @brief Protected default constructor
     */
    i_message_handler() = default;

    /**
     * @brief Protected copy constructor
     */
    i_message_handler(const i_message_handler&) = default;

    /**
     * @brief Protected move constructor
     */
    i_message_handler(i_message_handler&&) = default;

    /**
     * @brief Protected copy assignment
     */
    i_message_handler& operator=(const i_message_handler&) = default;

    /**
     * @brief Protected move assignment
     */
    i_message_handler& operator=(i_message_handler&&) = default;
};

/**
 * @brief Interface for observable objects (Subject in Observer pattern)
 * 
 * Allows objects to notify observers about events.
 */
template<typename EventType>
class i_observable {
public:
    using observer_id = std::uint64_t;
    using observer_func = std::function<void(const EventType&)>;

    /**
     * @brief Virtual destructor
     */
    virtual ~i_observable() = default;

    /**
     * @brief Add an observer
     * @param observer Observer function
     * @return Observer ID for later removal
     */
    virtual observer_id add_observer(observer_func observer) = 0;

    /**
     * @brief Remove an observer
     * @param id Observer ID returned by add_observer
     * @return true if removed, false if not found
     */
    virtual bool remove_observer(observer_id id) = 0;

    /**
     * @brief Remove all observers
     */
    virtual void clear_observers() = 0;

    /**
     * @brief Get number of registered observers
     * @return Observer count
     */
    virtual std::size_t observer_count() const noexcept = 0;

protected:
    /**
     * @brief Notify all observers of an event
     * @param event The event to notify about
     */
    virtual void notify_observers(const EventType& event) = 0;

    /**
     * @brief Protected default constructor
     */
    i_observable() = default;

    /**
     * @brief Protected copy constructor
     */
    i_observable(const i_observable&) = default;

    /**
     * @brief Protected move constructor
     */
    i_observable(i_observable&&) = default;

    /**
     * @brief Protected copy assignment
     */
    i_observable& operator=(const i_observable&) = default;

    /**
     * @brief Protected move assignment
     */
    i_observable& operator=(i_observable&&) = default;
};

/**
 * @brief Interface for configurable objects
 * 
 * Provides standardized configuration management.
 */
template<typename ConfigType>
class i_configurable {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~i_configurable() = default;

    /**
     * @brief Configure the object
     * @param config Configuration object
     * @return Result indicating success or error
     */
    virtual result<void, std::string> configure(const ConfigType& config) = 0;

    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    virtual ConfigType get_config() const = 0;

    /**
     * @brief Validate configuration
     * @param config Configuration to validate
     * @return Result indicating validation success or error
     */
    virtual result<void, std::string> validate_config(const ConfigType& config) const = 0;

protected:
    /**
     * @brief Protected default constructor
     */
    i_configurable() = default;

    /**
     * @brief Protected copy constructor
     */
    i_configurable(const i_configurable&) = default;

    /**
     * @brief Protected move constructor
     */
    i_configurable(i_configurable&&) = default;

    /**
     * @brief Protected copy assignment
     */
    i_configurable& operator=(const i_configurable&) = default;

    /**
     * @brief Protected move assignment
     */
    i_configurable& operator=(i_configurable&&) = default;
};

/**
 * @brief Interface for timeout manageable operations
 * 
 * Provides standardized timeout handling for async operations.
 */
class i_timeout_managed {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~i_timeout_managed() = default;

    /**
     * @brief Set operation timeout
     * @param timeout Timeout duration
     */
    virtual void set_timeout(std::chrono::milliseconds timeout) noexcept = 0;

    /**
     * @brief Get current timeout setting
     * @return Current timeout duration
     */
    virtual std::chrono::milliseconds get_timeout() const noexcept = 0;

    /**
     * @brief Check if operation has timed out
     * @return true if timed out, false otherwise
     */
    virtual bool is_timed_out() const noexcept = 0;

    /**
     * @brief Cancel operation due to timeout
     * @return Awaitable void result
     */
    virtual async_void cancel() = 0;

protected:
    /**
     * @brief Protected default constructor
     */
    i_timeout_managed() = default;

    /**
     * @brief Protected copy constructor
     */
    i_timeout_managed(const i_timeout_managed&) = default;

    /**
     * @brief Protected move constructor
     */
    i_timeout_managed(i_timeout_managed&&) = default;

    /**
     * @brief Protected copy assignment
     */
    i_timeout_managed& operator=(const i_timeout_managed&) = default;

    /**
     * @brief Protected move assignment
     */
    i_timeout_managed& operator=(i_timeout_managed&&) = default;
};

/**
 * @brief Interface for logger objects
 * 
 * Provides standardized logging interface for components.
 */
class i_logger {
public:
    /**
     * @brief Log levels
     */
    enum class level {
        trace = 0,
        debug = 1,
        info = 2,
        warn = 3,
        error = 4,
        fatal = 5
    };

    /**
     * @brief Virtual destructor
     */
    virtual ~i_logger() = default;

    /**
     * @brief Log a message
     * @param lvl Log level
     * @param message Message to log
     */
    virtual void log(level lvl, const std::string& message) = 0;

    /**
     * @brief Log trace message
     * @param message Message to log
     */
    void trace(const std::string& message) { log(level::trace, message); }

    /**
     * @brief Log debug message
     * @param message Message to log
     */
    void debug(const std::string& message) { log(level::debug, message); }

    /**
     * @brief Log info message
     * @param message Message to log
     */
    void info(const std::string& message) { log(level::info, message); }

    /**
     * @brief Log warning message
     * @param message Message to log
     */
    void warn(const std::string& message) { log(level::warn, message); }

    /**
     * @brief Log error message
     * @param message Message to log
     */
    void error(const std::string& message) { log(level::error, message); }

    /**
     * @brief Log fatal message
     * @param message Message to log
     */
    void fatal(const std::string& message) { log(level::fatal, message); }

    /**
     * @brief Check if log level is enabled
     * @param lvl Log level to check
     * @return true if enabled, false otherwise
     */
    virtual bool is_enabled(level lvl) const noexcept = 0;

    /**
     * @brief Set minimum log level
     * @param lvl Minimum log level
     */
    virtual void set_level(level lvl) noexcept = 0;

protected:
    /**
     * @brief Protected default constructor
     */
    i_logger() = default;

    /**
     * @brief Protected copy constructor
     */
    i_logger(const i_logger&) = default;

    /**
     * @brief Protected move constructor
     */
    i_logger(i_logger&&) = default;

    /**
     * @brief Protected copy assignment
     */
    i_logger& operator=(const i_logger&) = default;

    /**
     * @brief Protected move assignment
     */
    i_logger& operator=(i_logger&&) = default;
};

// Type aliases for commonly used interface combinations

/**
 * @brief Component with message handling capability
 */
template<typename MessageType>
using message_component = i_component;

/**
 * @brief Component with observation capability
 */
template<typename EventType>
using observable_component = i_component;

/**
 * @brief Configurable component
 */
template<typename ConfigType>
using configurable_component = i_component;

} // namespace ss::core