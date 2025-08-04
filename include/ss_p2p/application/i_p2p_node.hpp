#pragma once

#include "../core/interfaces.hpp"
#include "../core/types.hpp"
#include "../core/result.hpp"
#include "../network/i_transport.hpp"
#include "../dht/i_routing.hpp"
#include "../ice/i_nat_traversal.hpp"
#include "../security/i_crypto.hpp"
#include "../message.hpp"

#include <memory>
#include <vector>
#include <functional>
#include <optional>
#include <unordered_set>
#include <chrono>
#include <boost/asio.hpp>
#include <json.hpp>

namespace ss::application {

/**
 * @brief Configuration for P2P node initialization
 * 
 * Contains all necessary configuration parameters for node startup,
 * including network settings, DHT parameters, and security options.
 */
struct node_config {
    /// Local endpoint for the node
    core::endpoint local_endpoint;
    
    /// Bootstrap nodes for initial network join
    std::vector<core::endpoint> bootstrap_nodes;
    
    /// Node identifier (optional, auto-generated if not provided)
    std::optional<core::node_id> node_id;
    
    /// Maximum number of connections
    std::size_t max_connections = 1000;
    
    /// Connection timeout in milliseconds
    std::chrono::milliseconds connection_timeout{5000};
    
    /// DHT refresh interval in milliseconds
    std::chrono::milliseconds dht_refresh_interval{60000};
    
    /// Enable NAT traversal
    bool enable_nat_traversal = true;
    
    /// Enable encryption
    bool enable_encryption = true;
    
    /// Network timeout settings
    std::chrono::milliseconds ping_timeout{2000};
    std::chrono::milliseconds find_node_timeout{5000};
    
    /// Logging level
    core::i_logger::level log_level = core::i_logger::level::info;
    
    /// Application-specific configuration
    nlohmann::json app_config;
    
    /**
     * @brief Validate configuration parameters
     * @return Result indicating success or validation error
     */
    core::result<void, std::string> validate() const;
    
    /**
     * @brief Load configuration from JSON
     * @param config_json JSON configuration object
     * @return Result containing loaded config or error
     */
    static core::result<node_config, std::string> from_json(const nlohmann::json& config_json);
    
    /**
     * @brief Convert configuration to JSON
     * @return JSON representation of configuration
     */
    nlohmann::json to_json() const;
};

/**
 * @brief Node statistics and metrics
 * 
 * Provides comprehensive statistics about node operation,
 * including connection counts, message statistics, and performance metrics.
 */
struct node_stats {
    /// Current number of active connections
    std::size_t active_connections = 0;
    
    /// Total number of peers in routing table
    std::size_t routing_table_size = 0;
    
    /// Messages sent/received counters
    std::size_t messages_sent = 0;
    std::size_t messages_received = 0;
    std::size_t messages_failed = 0;
    
    /// Data transfer statistics (bytes)
    std::size_t bytes_sent = 0;
    std::size_t bytes_received = 0;
    
    /// Timing statistics
    std::chrono::steady_clock::time_point start_time;
    std::chrono::milliseconds uptime{0};
    std::chrono::milliseconds avg_ping_latency{0};
    
    /// NAT traversal statistics
    std::size_t nat_success_count = 0;
    std::size_t nat_failure_count = 0;
    
    /// DHT operation statistics
    std::size_t dht_lookups_performed = 0;
    std::size_t dht_lookups_successful = 0;
    
    /**
     * @brief Convert statistics to JSON
     * @return JSON representation of statistics
     */
    nlohmann::json to_json() const;
    
    /**
     * @brief Update uptime based on start time
     */
    void update_uptime();
};

/**
 * @brief Peer information structure
 * 
 * Contains comprehensive information about a peer node,
 * including connection status, performance metrics, and capabilities.
 */
struct peer_info {
    /// Peer's node identifier
    core::node_id id;
    
    /// Peer's network endpoint
    core::endpoint endpoint;
    
    /// Connection status
    enum class status {
        unknown,
        connecting,
        connected,
        disconnected,
        failed
    } connection_status = status::unknown;
    
    /// Last seen timestamp
    std::chrono::steady_clock::time_point last_seen;
    
    /// Average latency to this peer
    std::chrono::milliseconds latency{0};
    
    /// Peer capabilities (application-specific)
    std::unordered_set<std::string> capabilities;
    
    /// Connection quality score (0.0 - 1.0)
    double quality_score = 0.0;
    
    /**
     * @brief Check if peer is considered alive
     * @param timeout Maximum time since last contact
     * @return true if peer is alive, false otherwise
     */
    bool is_alive(std::chrono::milliseconds timeout = std::chrono::milliseconds{30000}) const;
    
    /**
     * @brief Convert peer info to JSON
     * @return JSON representation of peer info
     */
    nlohmann::json to_json() const;
};

/**
 * @brief Message handler callback type
 */
using message_handler = std::function<core::async_void(const ss::message&, const core::endpoint&)>;

/**
 * @brief Peer event callback type
 */
using peer_event_handler = std::function<void(const peer_info&, const std::string& event)>;

/**
 * @brief Interface for P2P Node operations
 * 
 * This interface provides the main API for P2P node operations, including
 * lifecycle management, peer communication, and network management.
 * It integrates all lower-level modules (DHT, ICE, Transport, Security)
 * into a cohesive high-level interface.
 */
class i_p2p_node : public core::i_component,
                   public core::i_configurable<node_config>,
                   public core::i_observable<peer_info> {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~i_p2p_node() = default;

    // ========================================
    // Node Lifecycle Management
    // ========================================

    /**
     * @brief Initialize node with configuration
     * @param config Node configuration
     * @return Awaitable void result
     */
    virtual core::async_void initialize_node(const node_config& config) = 0;

    /**
     * @brief Start the P2P node
     * @return Awaitable void result
     */
    virtual core::async_void start() override = 0;

    /**
     * @brief Stop the P2P node gracefully
     * @return Awaitable void result
     */
    virtual core::async_void stop() override = 0;

    /**
     * @brief Restart the node with current configuration
     * @return Awaitable void result
     */
    virtual core::async_void restart() = 0;

    // ========================================
    // Message Communication API
    // ========================================

    /**
     * @brief Send message to a specific peer
     * @param message Message to send
     * @param target_endpoint Target peer endpoint
     * @param timeout Operation timeout
     * @return Awaitable result with success indication
     */
    virtual core::async_result<bool> send_message(
        const ss::message& message,
        const core::endpoint& target_endpoint,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{5000}
    ) = 0;

    /**
     * @brief Send message to peer by node ID (using DHT lookup)
     * @param message Message to send
     * @param target_id Target peer node ID
     * @param timeout Operation timeout
     * @return Awaitable result with success indication
     */
    virtual core::async_result<bool> send_message(
        const ss::message& message,
        const core::node_id& target_id,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{10000}
    ) = 0;

    /**
     * @brief Broadcast message to all connected peers
     * @param message Message to broadcast
     * @param max_peers Maximum number of peers to send to (0 = all)
     * @return Awaitable result with number of successful sends
     */
    virtual core::async_result<std::size_t> broadcast_message(
        const ss::message& message,
        std::size_t max_peers = 0
    ) = 0;

    /**
     * @brief Register message handler for specific message types
     * @param app_id Application ID to handle
     * @param handler Handler function
     */
    virtual void register_message_handler(
        const ss::message::app_id& app_id,
        message_handler handler
    ) = 0;

    /**
     * @brief Unregister message handler
     * @param app_id Application ID to unregister
     */
    virtual void unregister_message_handler(const ss::message::app_id& app_id) = 0;

    // ========================================
    // Peer Management API
    // ========================================

    /**
     * @brief Get information about all known peers
     * @return Vector of peer information
     */
    virtual std::vector<peer_info> get_peers() const = 0;

    /**
     * @brief Get information about a specific peer
     * @param peer_id Peer node ID
     * @return Peer information if found
     */
    virtual std::optional<peer_info> get_peer(const core::node_id& peer_id) const = 0;

    /**
     * @brief Get information about a peer by endpoint
     * @param endpoint Peer endpoint
     * @return Peer information if found
     */
    virtual std::optional<peer_info> get_peer(const core::endpoint& endpoint) const = 0;

    /**
     * @brief Connect to a specific peer
     * @param endpoint Peer endpoint
     * @param timeout Connection timeout
     * @return Awaitable result with success indication
     */
    virtual core::async_result<bool> connect_to_peer(
        const core::endpoint& endpoint,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{10000}
    ) = 0;

    /**
     * @brief Disconnect from a peer
     * @param peer_id Peer node ID
     * @return Awaitable void result
     */
    virtual core::async_void disconnect_from_peer(const core::node_id& peer_id) = 0;

    /**
     * @brief Find peers near a specific node ID
     * @param target_id Target node ID
     * @param max_peers Maximum number of peers to return
     * @return Awaitable result with found peers
     */
    virtual core::async_result<std::vector<peer_info>> find_peers(
        const core::node_id& target_id,
        std::size_t max_peers = 20
    ) = 0;

    /**
     * @brief Register peer event handler
     * @param handler Event handler function
     * @return Handler ID for later removal
     */
    virtual std::size_t register_peer_event_handler(peer_event_handler handler) = 0;

    /**
     * @brief Unregister peer event handler
     * @param handler_id Handler ID returned by register_peer_event_handler
     */
    virtual void unregister_peer_event_handler(std::size_t handler_id) = 0;

    // ========================================
    // Node Information API
    // ========================================

    /**
     * @brief Get this node's ID
     * @return Node identifier
     */
    virtual core::node_id get_node_id() const noexcept = 0;

    /**
     * @brief Get this node's local endpoint
     * @return Local endpoint
     */
    virtual core::endpoint get_local_endpoint() const noexcept = 0;

    /**
     * @brief Get current node statistics
     * @return Node statistics
     */
    virtual node_stats get_stats() const = 0;

    /**
     * @brief Get current configuration
     * @return Current node configuration
     */
    virtual node_config get_config() const override = 0;

    // ========================================
    // Network Operations API
    // ========================================

    /**
     * @brief Perform network maintenance (refresh routing table, cleanup connections)
     * @return Awaitable void result
     */
    virtual core::async_void perform_maintenance() = 0;

    /**
     * @brief Join the P2P network using bootstrap nodes
     * @param bootstrap_nodes List of bootstrap node endpoints
     * @param timeout Join operation timeout
     * @return Awaitable result with success indication
     */
    virtual core::async_result<bool> join_network(
        const std::vector<core::endpoint>& bootstrap_nodes,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{30000}
    ) = 0;

    /**
     * @brief Leave the P2P network gracefully
     * @return Awaitable void result
     */
    virtual core::async_void leave_network() = 0;

    /**
     * @brief Ping a specific peer to check connectivity
     * @param endpoint Peer endpoint
     * @param timeout Ping timeout
     * @return Awaitable result with latency (0 if failed)
     */
    virtual core::async_result<std::chrono::milliseconds> ping_peer(
        const core::endpoint& endpoint,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{5000}
    ) = 0;

    // ========================================
    // Advanced Features API
    // ========================================

    /**
     * @brief Enable/disable NAT traversal
     * @param enabled True to enable, false to disable
     */
    virtual void set_nat_traversal_enabled(bool enabled) = 0;

    /**
     * @brief Check if NAT traversal is enabled
     * @return true if enabled, false otherwise
     */
    virtual bool is_nat_traversal_enabled() const noexcept = 0;

    /**
     * @brief Enable/disable encryption
     * @param enabled True to enable, false to disable
     */
    virtual void set_encryption_enabled(bool enabled) = 0;

    /**
     * @brief Check if encryption is enabled
     * @return true if enabled, false otherwise
     */
    virtual bool is_encryption_enabled() const noexcept = 0;

    /**
     * @brief Set maximum number of connections
     * @param max_connections Maximum connection count
     */
    virtual void set_max_connections(std::size_t max_connections) = 0;

    /**
     * @brief Get maximum number of connections
     * @return Maximum connection count
     */
    virtual std::size_t get_max_connections() const noexcept = 0;

    // ========================================
    // Error and Event Handling
    // ========================================

    /**
     * @brief Get last error message
     * @return Last error message (empty if no error)
     */
    virtual std::string get_last_error() const = 0;

    /**
     * @brief Clear error state
     */
    virtual void clear_error() = 0;

protected:
    /**
     * @brief Protected default constructor
     */
    i_p2p_node() = default;

    /**
     * @brief Protected copy constructor
     */
    i_p2p_node(const i_p2p_node&) = default;

    /**
     * @brief Protected move constructor
     */
    i_p2p_node(i_p2p_node&&) = default;

    /**
     * @brief Protected copy assignment
     */
    i_p2p_node& operator=(const i_p2p_node&) = default;

    /**
     * @brief Protected move assignment
     */
    i_p2p_node& operator=(i_p2p_node&&) = default;
};

/**
 * @brief Factory function for creating P2P node instances
 * @param io_context Boost.Asio IO context
 * @param config Node configuration
 * @return Unique pointer to P2P node instance
 */
std::unique_ptr<i_p2p_node> create_p2p_node(
    boost::asio::io_context& io_context,
    const node_config& config = {}
);

/**
 * @brief Builder class for P2P node configuration
 * 
 * Provides a fluent interface for configuring P2P nodes with
 * validation and sensible defaults.
 */
class node_builder {
public:
    /**
     * @brief Constructor
     */
    explicit node_builder(boost::asio::io_context& io_context);

    /**
     * @brief Set local endpoint
     * @param endpoint Local endpoint
     * @return Reference to builder for chaining
     */
    node_builder& with_local_endpoint(const core::endpoint& endpoint);

    /**
     * @brief Add bootstrap node
     * @param endpoint Bootstrap node endpoint
     * @return Reference to builder for chaining
     */
    node_builder& with_bootstrap_node(const core::endpoint& endpoint);

    /**
     * @brief Set node ID
     * @param id Node identifier
     * @return Reference to builder for chaining
     */
    node_builder& with_node_id(const core::node_id& id);

    /**
     * @brief Set maximum connections
     * @param max_connections Maximum connection count
     * @return Reference to builder for chaining
     */
    node_builder& with_max_connections(std::size_t max_connections);

    /**
     * @brief Enable/disable NAT traversal
     * @param enabled True to enable, false to disable
     * @return Reference to builder for chaining
     */
    node_builder& with_nat_traversal(bool enabled);

    /**
     * @brief Enable/disable encryption
     * @param enabled True to enable, false to disable
     * @return Reference to builder for chaining
     */
    node_builder& with_encryption(bool enabled);

    /**
     * @brief Set log level
     * @param level Log level
     * @return Reference to builder for chaining
     */
    node_builder& with_log_level(core::i_logger::level level);

    /**
     * @brief Set timeout values
     * @param connection_timeout Connection timeout
     * @param ping_timeout Ping timeout
     * @return Reference to builder for chaining
     */
    node_builder& with_timeouts(
        std::chrono::milliseconds connection_timeout,
        std::chrono::milliseconds ping_timeout
    );

    /**
     * @brief Load configuration from JSON file
     * @param config_path Path to configuration file
     * @return Reference to builder for chaining
     */
    node_builder& with_config_file(const std::string& config_path);

    /**
     * @brief Build and create the P2P node
     * @return Unique pointer to created node
     * @throws std::invalid_argument if configuration is invalid
     */
    std::unique_ptr<i_p2p_node> build();

private:
    boost::asio::io_context& io_context_;
    node_config config_;
};

} // namespace ss::application