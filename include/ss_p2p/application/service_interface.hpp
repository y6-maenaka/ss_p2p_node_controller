#pragma once

#include "../core/interfaces.hpp"
#include "../core/types.hpp"
#include "../core/result.hpp"

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <signal.h>
#include <boost/asio.hpp>
#include <json.hpp>

namespace ss::application {

/**
 * @brief Service configuration
 * 
 * Contains configuration parameters for service operation,
 * including daemon settings, logging, and system integration.
 */
struct service_config {
    /// Service name
    std::string service_name = "ss_p2p_service";
    
    /// Service display name
    std::string display_name = "SS P2P Service";
    
    /// Service description
    std::string description = "SS P2P Node Controller Service";
    
    /// Service version
    std::string version = "1.0.0";
    
    /// Run as daemon/service
    bool run_as_daemon = false;
    
    /// PID file path (for daemon mode)
    std::string pid_file_path = "/var/run/ss_p2p.pid";
    
    /// Log file path (for daemon mode)
    std::string log_file_path = "/var/log/ss_p2p.log";
    
    /// Working directory (for daemon mode)
    std::string working_directory = "/";
    
    /// User to run as (for daemon mode)
    std::string run_as_user = "";
    
    /// Group to run as (for daemon mode)
    std::string run_as_group = "";
    
    /// Enable stdin/stdout redirection for non-daemon mode
    bool enable_console_io = true;
    
    /// Configuration file path
    std::string config_file_path = "config.json";
    
    /// Auto-restart on failure
    bool auto_restart = true;
    
    /// Maximum restart attempts
    std::size_t max_restart_attempts = 3;
    
    /// Restart delay in seconds
    std::chrono::seconds restart_delay{5};
    
    /// Graceful shutdown timeout
    std::chrono::seconds shutdown_timeout{30};
    
    /// Health check interval
    std::chrono::seconds health_check_interval{30};
    
    /// Enable signal handlers
    bool enable_signal_handlers = true;
    
    /// Signals to handle for graceful shutdown
    std::vector<int> shutdown_signals = {SIGTERM, SIGINT};
    
    /// Signals to handle for reload
    std::vector<int> reload_signals = {SIGHUP};
    
    /**
     * @brief Validate configuration
     * @return Result indicating success or validation error
     */
    core::result<void, std::string> validate() const;
    
    /**
     * @brief Load configuration from JSON
     * @param config_json JSON configuration object
     * @return Result containing loaded config or error
     */
    static core::result<service_config, std::string> from_json(const nlohmann::json& config_json);
    
    /**
     * @brief Convert configuration to JSON
     * @return JSON representation of configuration
     */
    nlohmann::json to_json() const;
};

/**
 * @brief Service status information
 */
struct service_status {
    /// Service state
    enum class state {
        stopped,
        starting,
        running,
        stopping,
        failed,
        unknown
    } current_state = state::stopped;
    
    /// Service start time
    std::chrono::steady_clock::time_point start_time;
    
    /// Service uptime
    std::chrono::milliseconds uptime{0};
    
    /// Process ID
    pid_t process_id = 0;
    
    /// Last error message
    std::string last_error;
    
    /// Restart count
    std::uint32_t restart_count = 0;
    
    /// Health status
    bool healthy = false;
    
    /// Service version
    std::string version;
    
    /// Memory usage (bytes)
    std::size_t memory_usage = 0;
    
    /// CPU usage percentage
    double cpu_usage = 0.0;
    
    /**
     * @brief Update uptime based on start time
     */
    void update_uptime();
    
    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const;
};

/**
 * @brief Service event types
 */
enum class service_event {
    starting,
    started,
    stopping,
    stopped,
    reloading,
    reloaded,
    health_check_passed,
    health_check_failed,
    restart_requested,
    restart_completed,
    error_occurred
};

/**
 * @brief Service event handler callback type
 */
using service_event_handler = std::function<void(service_event event, const std::string& message)>;

/**
 * @brief Health check callback type
 */
using health_check_handler = std::function<bool()>;

/**
 * @brief Configuration reload callback type
 */
using config_reload_handler = std::function<core::result<void, std::string>()>;

/**
 * @brief Interface for system service operations
 * 
 * Provides service lifecycle management, daemon support, signal handling,
 * and system integration for P2P applications. Supports both standalone
 * and systemd service modes.
 */
class i_service : public core::i_component,
                  public core::i_configurable<service_config> {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~i_service() = default;

    // ========================================
    // Service Lifecycle Management
    // ========================================

    /**
     * @brief Initialize service with configuration
     * @param config Service configuration
     * @return Awaitable void result
     */
    virtual core::async_void initialize(const service_config& config) = 0;

    /**
     * @brief Start the service
     * @return Awaitable void result
     */
    virtual core::async_void start() override = 0;

    /**
     * @brief Stop the service gracefully
     * @return Awaitable void result
     */
    virtual core::async_void stop() override = 0;

    /**
     * @brief Restart the service
     * @return Awaitable void result
     */
    virtual core::async_void restart() = 0;

    /**
     * @brief Reload configuration without restart
     * @return Awaitable result with success indication
     */
    virtual core::async_result<bool> reload() = 0;

    // ========================================
    // Daemon Mode Operations
    // ========================================

    /**
     * @brief Daemonize the process
     * @return Result indicating success or error
     */
    virtual core::result<void, std::string> daemonize() = 0;

    /**
     * @brief Check if running as daemon
     * @return true if daemon, false otherwise
     */
    virtual bool is_daemon() const noexcept = 0;

    /**
     * @brief Create PID file
     * @param pid_file_path Path to PID file
     * @return Result indicating success or error
     */
    virtual core::result<void, std::string> create_pid_file(const std::string& pid_file_path = "") = 0;

    /**
     * @brief Remove PID file
     * @return Result indicating success or error
     */
    virtual core::result<void, std::string> remove_pid_file() = 0;

    /**
     * @brief Drop privileges to specified user/group
     * @param username Username to run as
     * @param groupname Group name to run as
     * @return Result indicating success or error
     */
    virtual core::result<void, std::string> drop_privileges(
        const std::string& username,
        const std::string& groupname = ""
    ) = 0;

    // ========================================
    // Signal Handling
    // ========================================

    /**
     * @brief Setup signal handlers
     * @return Result indicating success or error
     */
    virtual core::result<void, std::string> setup_signal_handlers() = 0;

    /**
     * @brief Add custom signal handler
     * @param signal Signal number
     * @param handler Handler function
     * @return true if added, false if signal already handled
     */
    virtual bool add_signal_handler(int signal, std::function<void(int)> handler) = 0;

    /**
     * @brief Remove signal handler
     * @param signal Signal number
     * @return true if removed, false if not found
     */
    virtual bool remove_signal_handler(int signal) = 0;

    /**
     * @brief Wait for shutdown signal
     * @return Awaitable void result
     */
    virtual core::async_void wait_for_shutdown_signal() = 0;

    // ========================================
    // Console I/O Management
    // ========================================

    /**
     * @brief Setup console I/O redirection
     * @param stdin_path Path for stdin redirection (empty to disable)
     * @param stdout_path Path for stdout redirection (empty to disable)
     * @param stderr_path Path for stderr redirection (empty to disable)
     * @return Result indicating success or error
     */
    virtual core::result<void, std::string> setup_console_io(
        const std::string& stdin_path = "",
        const std::string& stdout_path = "",
        const std::string& stderr_path = ""
    ) = 0;

    /**
     * @brief Read line from stdin (if enabled)
     * @param timeout Read timeout
     * @return Awaitable result with input line
     */
    virtual core::async_result<std::string> read_console_input(
        std::chrono::milliseconds timeout = std::chrono::milliseconds{0}
    ) = 0;

    /**
     * @brief Write output to console
     * @param message Message to write
     * @param to_stderr Write to stderr instead of stdout
     */
    virtual void write_console_output(const std::string& message, bool to_stderr = false) = 0;

    // ========================================
    // Status and Monitoring
    // ========================================

    /**
     * @brief Get current service status
     * @return Service status information
     */
    virtual service_status get_service_status() const = 0;

    /**
     * @brief Set health check handler
     * @param handler Health check function
     */
    virtual void set_health_check_handler(health_check_handler handler) = 0;

    /**
     * @brief Perform health check
     * @return true if healthy, false otherwise
     */
    virtual bool perform_health_check() = 0;

    /**
     * @brief Set configuration reload handler
     * @param handler Configuration reload function
     */
    virtual void set_config_reload_handler(config_reload_handler handler) = 0;

    /**
     * @brief Get system resource usage
     * @return JSON object with resource usage information
     */
    virtual nlohmann::json get_resource_usage() const = 0;

    // ========================================
    // Event Handling
    // ========================================

    /**
     * @brief Add service event handler
     * @param handler Event handler function
     * @return Handler ID for later removal
     */
    virtual std::size_t add_event_handler(service_event_handler handler) = 0;

    /**
     * @brief Remove service event handler
     * @param handler_id Handler ID returned by add_event_handler
     * @return true if removed, false if not found
     */
    virtual bool remove_event_handler(std::size_t handler_id) = 0;

    /**
     * @brief Emit service event
     * @param event Event type
     * @param message Event message
     */
    virtual void emit_event(service_event event, const std::string& message = "") = 0;

    // ========================================
    // Systemd Integration
    // ========================================

    /**
     * @brief Check if running under systemd
     * @return true if systemd, false otherwise
     */
    virtual bool is_systemd_service() const noexcept = 0;

    /**
     * @brief Notify systemd of service readiness
     * @return Result indicating success or error
     */
    virtual core::result<void, std::string> notify_systemd_ready() = 0;

    /**
     * @brief Send status update to systemd
     * @param status Status message
     * @return Result indicating success or error
     */
    virtual core::result<void, std::string> notify_systemd_status(const std::string& status) = 0;

    /**
     * @brief Send watchdog ping to systemd
     * @return Result indicating success or error
     */
    virtual core::result<void, std::string> notify_systemd_watchdog() = 0;

    /**
     * @brief Get systemd watchdog interval
     * @return Watchdog interval, zero if not enabled
     */
    virtual std::chrono::microseconds get_systemd_watchdog_interval() const = 0;

    // ========================================
    // Configuration Management
    // ========================================

    /**
     * @brief Configure service
     * @param config New configuration
     * @return Result indicating success or error
     */
    virtual core::result<void, std::string> configure(const service_config& config) override = 0;

    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    virtual service_config get_config() const override = 0;

    /**
     * @brief Load configuration from file
     * @param config_path Configuration file path
     * @return Result indicating success or error
     */
    virtual core::result<void, std::string> load_config_file(const std::string& config_path) = 0;

    /**
     * @brief Save configuration to file
     * @param config_path Configuration file path
     * @return Result indicating success or error
     */
    virtual core::result<void, std::string> save_config_file(const std::string& config_path = "") = 0;

protected:
    /**
     * @brief Protected default constructor
     */
    i_service() = default;

    /**
     * @brief Protected copy constructor
     */
    i_service(const i_service&) = default;

    /**
     * @brief Protected move constructor
     */
    i_service(i_service&&) = default;

    /**
     * @brief Protected copy assignment
     */
    i_service& operator=(const i_service&) = default;

    /**
     * @brief Protected move assignment
     */
    i_service& operator=(i_service&&) = default;
};

/**
 * @brief Factory function for creating service instances
 * @param io_context Boost.Asio IO context
 * @param config Service configuration
 * @return Unique pointer to service instance
 */
std::unique_ptr<i_service> create_service(
    boost::asio::io_context& io_context,
    const service_config& config = {}
);

/**
 * @brief Builder class for service configuration
 * 
 * Provides a fluent interface for configuring service with
 * validation and sensible defaults.
 */
class service_builder {
public:
    /**
     * @brief Constructor
     */
    explicit service_builder(boost::asio::io_context& io_context);

    /**
     * @brief Set service information
     * @param name Service name
     * @param display_name Service display name
     * @param description Service description
     * @param version Service version
     * @return Reference to builder for chaining
     */
    service_builder& with_service_info(
        const std::string& name,
        const std::string& display_name,
        const std::string& description,
        const std::string& version
    );

    /**
     * @brief Enable daemon mode
     * @param enabled True to enable, false to disable
     * @param pid_file_path PID file path
     * @param log_file_path Log file path
     * @param working_directory Working directory
     * @return Reference to builder for chaining
     */
    service_builder& with_daemon_mode(
        bool enabled,
        const std::string& pid_file_path = "",
        const std::string& log_file_path = "",
        const std::string& working_directory = ""
    );

    /**
     * @brief Set user/group to run as
     * @param username Username
     * @param groupname Group name
     * @return Reference to builder for chaining
     */
    service_builder& with_user_group(
        const std::string& username,
        const std::string& groupname = ""
    );

    /**
     * @brief Enable console I/O
     * @param enabled True to enable, false to disable
     * @return Reference to builder for chaining
     */
    service_builder& with_console_io(bool enabled);

    /**
     * @brief Set restart settings
     * @param auto_restart Enable auto-restart
     * @param max_attempts Maximum restart attempts
     * @param delay Restart delay
     * @return Reference to builder for chaining
     */
    service_builder& with_restart_settings(
        bool auto_restart,
        std::size_t max_attempts,
        std::chrono::seconds delay
    );

    /**
     * @brief Set signal handling
     * @param enabled Enable signal handlers
     * @param shutdown_signals Signals for shutdown
     * @param reload_signals Signals for reload
     * @return Reference to builder for chaining
     */
    service_builder& with_signal_handling(
        bool enabled,
        const std::vector<int>& shutdown_signals = {SIGTERM, SIGINT},
        const std::vector<int>& reload_signals = {SIGHUP}
    );

    /**
     * @brief Set timeouts
     * @param shutdown_timeout Graceful shutdown timeout
     * @param health_check_interval Health check interval
     * @return Reference to builder for chaining
     */
    service_builder& with_timeouts(
        std::chrono::seconds shutdown_timeout,
        std::chrono::seconds health_check_interval
    );

    /**
     * @brief Load configuration from JSON file
     * @param config_path Path to configuration file
     * @return Reference to builder for chaining
     */
    service_builder& with_config_file(const std::string& config_path);

    /**
     * @brief Build and create the service
     * @return Unique pointer to created service
     * @throws std::invalid_argument if configuration is invalid
     */
    std::unique_ptr<i_service> build();

private:
    boost::asio::io_context& io_context_;
    service_config config_;
};

/**
 * @brief Service runner utility class
 * 
 * Provides high-level service execution with automatic setup and
 * lifecycle management. Handles daemon mode, signal handling, and
 * error recovery.
 */
class service_runner {
public:
    /**
     * @brief Constructor
     * @param service Service instance to run
     */
    explicit service_runner(std::unique_ptr<i_service> service);

    /**
     * @brief Destructor
     */
    ~service_runner();

    /**
     * @brief Run service with specified configuration
     * @param config Service configuration
     * @return Exit code (0 for success, non-zero for error)
     */
    int run(const service_config& config);

    /**
     * @brief Run service from command line arguments
     * @param argc Argument count
     * @param argv Argument values
     * @return Exit code (0 for success, non-zero for error)
     */
    int run(int argc, char* argv[]);

    /**
     * @brief Request service shutdown
     */
    void shutdown();

    /**
     * @brief Check if service is running
     * @return true if running, false otherwise
     */
    bool is_running() const noexcept;

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};

/**
 * @brief Main entry point for service applications
 * 
 * Template function that provides a standard main entry point for
 * service applications with proper error handling and logging.
 * 
 * @tparam ServiceType Type of service to create
 * @param argc Command line argument count
 * @param argv Command line arguments
 * @param default_config Default service configuration
 * @return Exit code
 */
template<typename ServiceType>
int service_main(int argc, char* argv[], const service_config& default_config = {}) {
    try {
        boost::asio::io_context io_context;
        auto service = std::make_unique<ServiceType>(io_context);
        service_runner runner(std::move(service));
        return runner.run(argc, argv);
    }
    catch (const std::exception& e) {
        std::cerr << "Service error: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "Unknown service error" << std::endl;
        return 2;
    }
}

} // namespace ss::application