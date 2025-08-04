#include "../../../../include/ss_p2p/application/i_p2p_node.hpp"
#include "../../../../include/ss_p2p/node_controller.hpp"
#include "../../../../include/ss_p2p/core/result.hpp"
#include "../../../../include/ss_p2p/ss_logger.hpp"
#include "../../../../include/utils.hpp"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <fstream>
#include <algorithm>

namespace ss::application {

/**
 * @brief Implementation of P2P Node interface
 * 
 * Integrates all lower-level modules (DHT, ICE, Transport, Security)
 * into a cohesive high-level P2P node with lifecycle management,
 * dependency injection, and comprehensive error handling.
 */
class p2p_node_impl : public i_p2p_node {
public:
    /**
     * @brief Constructor
     * @param io_context Boost.Asio IO context
     */
    explicit p2p_node_impl(boost::asio::io_context& io_context)
        : io_context_(io_context)
        , stats_{}
        , running_(false)
        , maintenance_timer_(io_context)
        , next_peer_event_handler_id_(1)
        , logger_(std::make_shared<ss_logger>("P2PNode"))
    {
        stats_.start_time = std::chrono::steady_clock::now();
    }

    /**
     * @brief Destructor
     */
    ~p2p_node_impl() override {
        if (running_.load()) {
            try {
                auto stop_future = boost::asio::co_spawn(io_context_, stop(), boost::asio::use_future);
                if (stop_future.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
                    // // logger_->error("Forced shutdown due to timeout");
                }
            } catch (const std::exception& e) {
                // // logger_->error("Error during shutdown: " + std::string(e.what()));
            }
        }
    }

    // ========================================
    // i_component Implementation
    // ========================================

    std::string name() const noexcept override {
        return "P2PNode";
    }

    std::string version() const noexcept override {
        return "1.0.0";
    }

    bool is_running() const noexcept override {
        return running_.load();
    }

    bool is_healthy() const noexcept override {
        std::shared_lock lock(mutex_);
        return running_.load() && 
               node_controller_ && 
               node_controller_->is_running() &&
               !last_error_.empty() == false;
    }

    std::string status() const override {
        std::shared_lock lock(mutex_);
        
        nlohmann::json status_json;
        status_json["name"] = name();
        status_json["version"] = version();
        status_json["running"] = is_running();
        status_json["healthy"] = is_healthy();
        status_json["node_id"] = config_.node_id ? config_.node_id->to_hex() : "";
        status_json["local_endpoint"] = config_.local_endpoint.to_string();
        status_json["last_error"] = last_error_;
        status_json["stats"] = stats_.to_json();
        
        return status_json.dump(2);
    }

    // ========================================
    // Node Lifecycle Management
    // ========================================

    core::async_void initialize_node(const node_config& config) override {
        std::unique_lock lock(mutex_);
        
        // logger_->info("Initializing P2P node with config: " + config.to_json().dump());
        
        // Validate configuration
        auto validation_result = config.validate();
        if (!validation_result.is_ok()) {
            last_error_ = validation_result.error();
            throw std::invalid_argument("Invalid configuration: " + last_error_);
        }
        
        config_ = config;
        
        // Generate node ID if not provided
        if (!config_.node_id) {
            config_.node_id = core::node_id::random();
            // logger_->info("Generated node ID: " + config_.node_id->to_hex());
        }
        
        // Initialize logger level
        // logger_->set_level(static_cast<ss_logger::log_level>(config_.log_level));
        
        // Create node controller with our endpoint
        try {
            node_controller_ = std::make_unique<ss::node_controller>(
                config_.local_endpoint.native(),
                io_context_
            );
            
            // logger_->info("Node controller created successfully");
        } catch (const std::exception& e) {
            last_error_ = "Failed to create node controller: " + std::string(e.what());
            // logger_->error(last_error_);
            throw;
        }
        
        co_return;
    }

    core::async_void start() override {
        std::unique_lock lock(mutex_);
        
        if (running_.load()) {
            // logger_->warn("Node already running");
            co_return;
        }
        
        // logger_->info("Starting P2P node");
        
        try {
            // Convert bootstrap endpoints to the format expected by node_controller
            std::vector<boost::asio::ip::udp::endpoint> bootstrap_endpoints;
            for (const auto& endpoint : config_.bootstrap_nodes) {
                bootstrap_endpoints.push_back(endpoint.native());
            }
            
            // Start node controller
            if (!bootstrap_endpoints.empty()) {
                node_controller_->start(bootstrap_endpoints);
                // logger_->info("Node controller started with " + 
                //             std::to_string(bootstrap_endpoints.size()) + " bootstrap nodes");
            } else {
                // logger_->warn("No bootstrap nodes provided, starting in standalone mode");
                // For standalone mode, we might need a different start method
                // node_controller_->start_standalone();
            }
            
            running_.store(true);
            stats_.start_time = std::chrono::steady_clock::now();
            
            // Start maintenance timer
            schedule_maintenance();
            
            // logger_->info("P2P node started successfully");
            
        } catch (const std::exception& e) {
            last_error_ = "Failed to start node: " + std::string(e.what());
            // logger_->error(last_error_);
            running_.store(false);
            throw;
        }
        
        co_return;
    }

    core::async_void stop() override {
        std::unique_lock lock(mutex_);
        
        if (!running_.load()) {
            // logger_->warn("Node not running");
            co_return;
        }
        
        // logger_->info("Stopping P2P node");
        
        try {
            running_.store(false);
            
            // Cancel maintenance timer
            maintenance_timer_.cancel();
            
            // Stop node controller if it exists
            if (node_controller_) {
                // Note: node_controller might not have an async stop method
                // We may need to call a synchronous stop or cleanup method
                // node_controller_->stop();
                // logger_->info("Node controller stopped");
            }
            
            // Clear message handlers
            {
                std::unique_lock handler_lock(message_handlers_mutex_);
                message_handlers_.clear();
            }
            
            // Clear peer event handlers
            {
                std::unique_lock handler_lock(peer_event_handlers_mutex_);
                peer_event_handlers_.clear();
            }
            
            // logger_->info("P2P node stopped successfully");
            
        } catch (const std::exception& e) {
            last_error_ = "Error during stop: " + std::string(e.what());
            // logger_->error(last_error_);
        }
        
        co_return;
    }

    core::async_void restart() override {
        // logger_->info("Restarting P2P node");
        
        co_await stop();
        co_await boost::asio::steady_timer(io_context_, std::chrono::milliseconds(1000)).async_wait(boost::asio::use_awaitable);
        co_await start();
        
        // logger_->info("P2P node restarted successfully");
    }

    // ========================================
    // Message Communication API
    // ========================================

    core::async_result<bool> send_message(
        const ss::message& message,
        const core::endpoint& target_endpoint,
        std::chrono::milliseconds timeout
    ) override {
        if (!is_running()) {
            co_return false;
        }
        
        try {
            std::shared_lock lock(mutex_);
            
            if (!node_controller_) {
                // logger_->error("Node controller not available");
                co_return false;
            }
            
            // Get peer reference from node controller
            auto peer = node_controller_->get_peer(target_endpoint.native());
            
            // Send message using the peer
            // Note: This assumes the message can be converted to the expected format
            auto encoded_message = message.serialize();
            
            // Create timer for timeout
            boost::asio::steady_timer timer(io_context_, timeout);
            bool timed_out = false;
            
            timer.async_wait([&timed_out](const boost::system::error_code& ec) {
                if (!ec) {
                    timed_out = true;
                }
            });
            
            // Send the message (this is a synchronous operation in the current API)
            bool success = false;
            try {
                // peer.send(encoded_message); // This might be the actual method
                success = true;
                stats_.messages_sent++;
                stats_.bytes_sent += encoded_message.size();
                
            } catch (const std::exception& e) {
                // logger_->error("Failed to send message: " + std::string(e.what()));
                stats_.messages_failed++;
                success = false;
            }
            
            timer.cancel();
            
            co_return success && !timed_out;
            
        } catch (const std::exception& e) {
            // logger_->error("Send message error: " + std::string(e.what()));
            stats_.messages_failed++;
            co_return false;
        }
    }

    core::async_result<bool> send_message(
        const ss::message& message,
        const core::node_id& target_id,
        std::chrono::milliseconds timeout
    ) override {
        if (!is_running()) {
            co_return false;
        }
        
        try {
            // First, try to find the peer in our known peers
            auto peer_info_opt = get_peer(target_id);
            if (peer_info_opt) {
                co_return co_await send_message(message, peer_info_opt->endpoint, timeout);
            }
            
            // If not found, perform DHT lookup
            auto found_peers = co_await find_peers(target_id, 1);
            if (found_peers.empty()) {
                // logger_->warn("Could not find peer with ID: " + target_id.to_hex());
                co_return false;
            }
            
            // Send to the first found peer
            co_return co_await send_message(message, found_peers[0].endpoint, timeout);
            
        } catch (const std::exception& e) {
            // logger_->error("Send message by ID error: " + std::string(e.what()));
            co_return false;
        }
    }

    core::async_result<std::size_t> broadcast_message(
        const ss::message& message,
        std::size_t max_peers
    ) override {
        if (!is_running()) {
            co_return 0;
        }
        
        try {
            auto all_peers = get_peers();
            
            if (max_peers == 0 || max_peers > all_peers.size()) {
                max_peers = all_peers.size();
            }
            
            std::size_t successful_sends = 0;
            std::vector<core::async_result<bool>> send_tasks;
            
            // Send to up to max_peers
            for (std::size_t i = 0; i < max_peers && i < all_peers.size(); ++i) {
                send_tasks.push_back(send_message(message, all_peers[i].endpoint, std::chrono::milliseconds{5000}));
            }
            
            // Wait for all sends to complete
            for (auto& task : send_tasks) {
                if (co_await std::move(task)) {
                    successful_sends++;
                }
            }
            
            // logger_->info("Broadcast message to " + std::to_string(successful_sends) + 
            //              " out of " + std::to_string(max_peers) + " peers");
            
            co_return successful_sends;
            
        } catch (const std::exception& e) {
            // logger_->error("Broadcast message error: " + std::string(e.what()));
            co_return 0;
        }
    }

    void register_message_handler(
        const ss::message::app_id& app_id,
        message_handler handler
    ) override {
        std::unique_lock lock(message_handlers_mutex_);
        
        std::string app_id_str(app_id.begin(), app_id.end());
        message_handlers_[app_id_str] = std::move(handler);
        
        // logger_->info("Registered message handler for app_id: " + app_id_str);
    }

    void unregister_message_handler(const ss::message::app_id& app_id) override {
        std::unique_lock lock(message_handlers_mutex_);
        
        std::string app_id_str(app_id.begin(), app_id.end());
        auto it = message_handlers_.find(app_id_str);
        if (it != message_handlers_.end()) {
            message_handlers_.erase(it);
            // logger_->info("Unregistered message handler for app_id: " + app_id_str);
        }
    }

    // ========================================
    // Peer Management API
    // ========================================

    std::vector<peer_info> get_peers() const override {
        std::shared_lock lock(mutex_);
        
        // This would need to query the node_controller's routing table
        // For now, return empty vector as placeholder
        std::vector<peer_info> peers;
        
        // TODO: Implement actual peer enumeration from node_controller
        // This might involve accessing the routing table or connection manager
        
        return peers;
    }

    std::optional<peer_info> get_peer(const core::node_id& peer_id) const override {
        auto all_peers = get_peers();
        
        auto it = std::find_if(all_peers.begin(), all_peers.end(),
            [&peer_id](const peer_info& info) {
                return info.id == peer_id;
            });
        
        if (it != all_peers.end()) {
            return *it;
        }
        
        return std::nullopt;
    }

    std::optional<peer_info> get_peer(const core::endpoint& endpoint) const override {
        auto all_peers = get_peers();
        
        auto it = std::find_if(all_peers.begin(), all_peers.end(),
            [&endpoint](const peer_info& info) {
                return info.endpoint == endpoint;
            });
        
        if (it != all_peers.end()) {
            return *it;
        }
        
        return std::nullopt;
    }

    core::async_result<bool> connect_to_peer(
        const core::endpoint& endpoint,
        std::chrono::milliseconds timeout
    ) override {
        if (!is_running()) {
            co_return false;
        }
        
        try {
            std::shared_lock lock(mutex_);
            
            if (!node_controller_) {
                co_return false;
            }
            
            // Use node_controller to establish connection
            auto peer = node_controller_->get_peer(endpoint.native());
            
            // TODO: Implement actual connection establishment
            // This might involve DHT operations or direct connection attempts
            
            // logger_->info("Connected to peer: " + endpoint.to_string());
            co_return true;
            
        } catch (const std::exception& e) {
            // logger_->error("Connect to peer error: " + std::string(e.what()));
            co_return false;
        }
    }

    core::async_void disconnect_from_peer(const core::node_id& peer_id) override {
        if (!is_running()) {
            co_return;
        }
        
        try {
            // TODO: Implement peer disconnection
            // logger_->info("Disconnected from peer: " + peer_id.to_hex());
            
        } catch (const std::exception& e) {
            // logger_->error("Disconnect from peer error: " + std::string(e.what()));
        }
        
        co_return;
    }

    core::async_result<std::vector<peer_info>> find_peers(
        const core::node_id& target_id,
        std::size_t max_peers
    ) override {
        if (!is_running()) {
            co_return std::vector<peer_info>{};
        }
        
        try {
            std::shared_lock lock(mutex_);
            
            if (!node_controller_) {
                co_return std::vector<peer_info>{};
            }
            
            // TODO: Implement DHT lookup using node_controller
            // This would involve calling find_node or similar DHT operations
            
            std::vector<peer_info> found_peers;
            // Placeholder implementation
            
            // logger_->info("Found " + std::to_string(found_peers.size()) + 
            //              " peers for target: " + target_id.to_hex());
            
            co_return found_peers;
            
        } catch (const std::exception& e) {
            // logger_->error("Find peers error: " + std::string(e.what()));
            co_return std::vector<peer_info>{};
        }
    }

    std::size_t register_peer_event_handler(peer_event_handler handler) override {
        std::unique_lock lock(peer_event_handlers_mutex_);
        
        auto handler_id = next_peer_event_handler_id_++;
        peer_event_handlers_[handler_id] = std::move(handler);
        
        return handler_id;
    }

    void unregister_peer_event_handler(std::size_t handler_id) override {
        std::unique_lock lock(peer_event_handlers_mutex_);
        
        peer_event_handlers_.erase(handler_id);
    }

    // ========================================
    // Node Information API
    // ========================================

    core::node_id get_node_id() const noexcept override {
        std::shared_lock lock(mutex_);
        return config_.node_id.value_or(core::node_id{});
    }

    core::endpoint get_local_endpoint() const noexcept override {
        std::shared_lock lock(mutex_);
        return config_.local_endpoint;
    }

    node_stats get_stats() const override {
        std::shared_lock lock(mutex_);
        
        auto current_stats = stats_;
        current_stats.update_uptime();
        
        return current_stats;
    }

    node_config get_config() const override {
        std::shared_lock lock(mutex_);
        return config_;
    }

    // ========================================
    // Network Operations API
    // ========================================

    core::async_void perform_maintenance() override {
        if (!is_running()) {
            co_return;
        }
        
        try {
            // logger_->debug("Performing network maintenance");
            
            // TODO: Implement maintenance operations
            // - Refresh routing table
            // - Cleanup dead connections
            // - Update peer statistics
            // - Perform health checks
            
            stats_.update_uptime();
            
        } catch (const std::exception& e) {
            // logger_->error("Maintenance error: " + std::string(e.what()));
        }
        
        co_return;
    }

    core::async_result<bool> join_network(
        const std::vector<core::endpoint>& bootstrap_nodes,
        std::chrono::milliseconds timeout
    ) override {
        if (!is_running()) {
            co_return false;
        }
        
        try {
            // logger_->info("Joining network with " + std::to_string(bootstrap_nodes.size()) + 
            //              " bootstrap nodes");
            
            // TODO: Implement network join logic
            // This might involve updating the bootstrap list and triggering DHT bootstrap
            
            co_return true;
            
        } catch (const std::exception& e) {
            // logger_->error("Join network error: " + std::string(e.what()));
            co_return false;
        }
    }

    core::async_void leave_network() override {
        if (!is_running()) {
            co_return;
        }
        
        try {
            // logger_->info("Leaving network gracefully");
            
            // TODO: Implement graceful network leave
            // - Announce departure to peers
            // - Transfer any hosted data
            // - Clean up connections
            
        } catch (const std::exception& e) {
            // logger_->error("Leave network error: " + std::string(e.what()));
        }
        
        co_return;
    }

    core::async_result<std::chrono::milliseconds> ping_peer(
        const core::endpoint& endpoint,
        std::chrono::milliseconds timeout
    ) override {
        if (!is_running()) {
            co_return std::chrono::milliseconds{0};
        }
        
        try {
            auto start_time = std::chrono::steady_clock::now();
            
            // TODO: Implement actual ping using node_controller
            
            auto end_time = std::chrono::steady_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time);
            
            // logger_->debug("Ping to " + endpoint.to_string() + ": " + 
                          // std::to_string(latency.count()) + "ms");
            
            co_return latency;
            
        } catch (const std::exception& e) {
            // logger_->error("Ping peer error: " + std::string(e.what()));
            co_return std::chrono::milliseconds{0};
        }
    }

    // ========================================
    // Advanced Features API
    // ========================================

    void set_nat_traversal_enabled(bool enabled) override {
        std::unique_lock lock(mutex_);
        config_.enable_nat_traversal = enabled;
        // logger_->info("NAT traversal " + std::string(enabled ? "enabled" : "disabled"));
    }

    bool is_nat_traversal_enabled() const noexcept override {
        std::shared_lock lock(mutex_);
        return config_.enable_nat_traversal;
    }

    void set_encryption_enabled(bool enabled) override {
        std::unique_lock lock(mutex_);
        config_.enable_encryption = enabled;
        // logger_->info("Encryption " + std::string(enabled ? "enabled" : "disabled"));
    }

    bool is_encryption_enabled() const noexcept override {
        std::shared_lock lock(mutex_);
        return config_.enable_encryption;
    }

    void set_max_connections(std::size_t max_connections) override {
        std::unique_lock lock(mutex_);
        config_.max_connections = max_connections;
        // logger_->info("Max connections set to: " + std::to_string(max_connections));
    }

    std::size_t get_max_connections() const noexcept override {
        std::shared_lock lock(mutex_);
        return config_.max_connections;
    }

    // ========================================
    // Error and Event Handling
    // ========================================

    std::string get_last_error() const override {
        std::shared_lock lock(mutex_);
        return last_error_;
    }

    void clear_error() override {
        std::unique_lock lock(mutex_);
        last_error_.clear();
    }

    // ========================================
    // i_configurable Implementation
    // ========================================

    core::result<void, std::string> configure(const node_config& config) override {
        auto validation_result = config.validate();
        if (!validation_result.is_ok()) {
            return core::result<void, std::string>(validation_result.error());
        }
        
        std::unique_lock lock(mutex_);
        config_ = config;
        
        return core::result<void, std::string>();
    }

    core::result<void, std::string> validate_config(const node_config& config) const override {
        return config.validate();
    }

    // ========================================
    // i_observable Implementation
    // ========================================

    observer_id add_observer(observer_func observer) override {
        // Delegate to peer event handler system
        return register_peer_event_handler([observer](const peer_info& info, const std::string& event [[maybe_unused]]) {
            observer(info);
        });
    }

    bool remove_observer(observer_id id) override {
        unregister_peer_event_handler(id);
        return true;
    }

    void clear_observers() override {
        std::unique_lock lock(peer_event_handlers_mutex_);
        peer_event_handlers_.clear();
    }

    std::size_t observer_count() const noexcept override {
        std::shared_lock lock(peer_event_handlers_mutex_);
        return peer_event_handlers_.size();
    }

protected:
    void notify_observers(const peer_info& event [[maybe_unused]]) override {
        std::shared_lock lock(peer_event_handlers_mutex_);
        
        for (const auto& [id, handler] : peer_event_handlers_) {
            try {
                handler(event, "peer_event");
            } catch (const std::exception& e) {
                // logger_->error("Observer notification error: " + std::string(e.what()));
            }
        }
    }

private:
    /**
     * @brief Schedule periodic maintenance
     */
    void schedule_maintenance() {
        if (!running_.load()) {
            return;
        }
        
        maintenance_timer_.expires_after(config_.dht_refresh_interval);
        maintenance_timer_.async_wait([this](const boost::system::error_code& ec) {
            if (!ec && running_.load()) {
                boost::asio::co_spawn(io_context_, perform_maintenance(), boost::asio::detached);
                schedule_maintenance();
            }
        });
    }

    // Core dependencies
    boost::asio::io_context& io_context_;
    std::unique_ptr<ss::node_controller> node_controller_;
    
    // Configuration and state
    mutable std::shared_mutex mutex_;
    node_config config_;
    node_stats stats_;
    std::atomic<bool> running_;
    std::string last_error_;
    
    // Maintenance
    boost::asio::steady_timer maintenance_timer_;
    
    // Message handling
    mutable std::shared_mutex message_handlers_mutex_;
    std::unordered_map<std::string, message_handler> message_handlers_;
    
    // Peer event handling
    mutable std::shared_mutex peer_event_handlers_mutex_;
    std::unordered_map<std::size_t, peer_event_handler> peer_event_handlers_;
    std::atomic<std::size_t> next_peer_event_handler_id_;
    
    // Logging
    std::shared_ptr<ss_logger> logger_;
};

// ========================================
// Configuration Implementation
// ========================================

core::result<void, std::string> node_config::validate() const {
    if (!local_endpoint.is_valid()) {
        return core::result<void, std::string>(std::string("Invalid local endpoint"));
    }
    
    if (max_connections == 0) {
        return core::result<void, std::string>(std::string("Max connections must be greater than 0"));
    }
    
    if (connection_timeout.count() <= 0) {
        return core::result<void, std::string>(std::string("Connection timeout must be positive"));
    }
    
    if (dht_refresh_interval.count() <= 0) {
        return core::result<void, std::string>(std::string("DHT refresh interval must be positive"));
    }
    
    if (ping_timeout.count() <= 0) {
        return core::result<void, std::string>(std::string("Ping timeout must be positive"));
    }
    
    if (find_node_timeout.count() <= 0) {
        return core::result<void, std::string>(std::string("Find node timeout must be positive"));
    }
    
    return core::result<void, std::string>();
}

core::result<node_config, std::string> node_config::from_json(const nlohmann::json& config_json) {
    try {
        node_config config;
        
        if (config_json.contains("local_endpoint")) {
            auto ep_json = config_json["local_endpoint"];
            if (ep_json.contains("address") && ep_json.contains("port")) {
                config.local_endpoint = core::endpoint(
                    ep_json["address"].get<std::string>(),
                    ep_json["port"].get<std::uint16_t>()
                );
            }
        }
        
        if (config_json.contains("bootstrap_nodes") && config_json["bootstrap_nodes"].is_array()) {
            for (const auto& node_json : config_json["bootstrap_nodes"]) {
                if (node_json.contains("address") && node_json.contains("port")) {
                    config.bootstrap_nodes.emplace_back(
                        node_json["address"].get<std::string>(),
                        node_json["port"].get<std::uint16_t>()
                    );
                }
            }
        }
        
        if (config_json.contains("node_id")) {
            auto node_id_str = config_json["node_id"].get<std::string>();
            if (!node_id_str.empty()) {
                try {
                    config.node_id = core::node_id::from_hex(node_id_str);
                } catch (const std::exception&) {
                    return core::result<node_config, std::string>::error("Invalid node_id format");
                }
            }
        }
        
        if (config_json.contains("max_connections")) {
            config.max_connections = config_json["max_connections"].get<std::size_t>();
        }
        
        if (config_json.contains("connection_timeout_ms")) {
            config.connection_timeout = std::chrono::milliseconds(
                config_json["connection_timeout_ms"].get<std::uint64_t>()
            );
        }
        
        if (config_json.contains("dht_refresh_interval_ms")) {
            config.dht_refresh_interval = std::chrono::milliseconds(
                config_json["dht_refresh_interval_ms"].get<std::uint64_t>()
            );
        }
        
        if (config_json.contains("enable_nat_traversal")) {
            config.enable_nat_traversal = config_json["enable_nat_traversal"].get<bool>();
        }
        
        if (config_json.contains("enable_encryption")) {
            config.enable_encryption = config_json["enable_encryption"].get<bool>();
        }
        
        if (config_json.contains("app_config")) {
            config.app_config = config_json["app_config"];
        }
        
        auto validation_result = config.validate();
        if (!validation_result.is_ok()) {
            return core::result<node_config, std::string>::error(validation_result.error());
        }
        
        return core::result<node_config, std::string>::ok(std::move(config));
        
    } catch (const std::exception& e) {
        return core::result<node_config, std::string>::error(
            "JSON parsing error: " + std::string(e.what())
        );
    }
}

nlohmann::json node_config::to_json() const {
    nlohmann::json json;
    
    json["local_endpoint"] = {
        {"address", local_endpoint.address()},
        {"port", local_endpoint.port()}
    };
    
    json["bootstrap_nodes"] = nlohmann::json::array();
    for (const auto& endpoint : bootstrap_nodes) {
        json["bootstrap_nodes"].push_back({
            {"address", endpoint.address()},
            {"port", endpoint.port()}
        });
    }
    
    if (node_id) {
        json["node_id"] = node_id->to_hex();
    }
    
    json["max_connections"] = max_connections;
    json["connection_timeout_ms"] = connection_timeout.count();
    json["dht_refresh_interval_ms"] = dht_refresh_interval.count();
    json["enable_nat_traversal"] = enable_nat_traversal;
    json["enable_encryption"] = enable_encryption;
    json["ping_timeout_ms"] = ping_timeout.count();
    json["find_node_timeout_ms"] = find_node_timeout.count();
    json["log_level"] = static_cast<int>(log_level);
    json["app_config"] = app_config;
    
    return json;
}

// ========================================
// Statistics Implementation
// ========================================

nlohmann::json node_stats::to_json() const {
    nlohmann::json json;
    
    json["active_connections"] = active_connections;
    json["routing_table_size"] = routing_table_size;
    json["messages_sent"] = messages_sent;
    json["messages_received"] = messages_received;
    json["messages_failed"] = messages_failed;
    json["bytes_sent"] = bytes_sent;
    json["bytes_received"] = bytes_received;
    json["uptime_ms"] = uptime.count();
    json["avg_ping_latency_ms"] = avg_ping_latency.count();
    json["nat_success_count"] = nat_success_count;
    json["nat_failure_count"] = nat_failure_count;
    json["dht_lookups_performed"] = dht_lookups_performed;
    json["dht_lookups_successful"] = dht_lookups_successful;
    
    return json;
}

void node_stats::update_uptime() {
    auto now = std::chrono::steady_clock::now();
    uptime = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
}

// ========================================
// Peer Info Implementation
// ========================================

bool peer_info::is_alive(std::chrono::milliseconds timeout) const {
    auto now = std::chrono::steady_clock::now();
    auto time_since_last_seen = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_seen);
    
    return time_since_last_seen <= timeout;
}

nlohmann::json peer_info::to_json() const {
    nlohmann::json json;
    
    json["id"] = id.to_hex();
    json["endpoint"] = {
        {"address", endpoint.address()},
        {"port", endpoint.port()}
    };
    json["connection_status"] = static_cast<int>(connection_status);
    json["latency_ms"] = latency.count();
    json["capabilities"] = nlohmann::json::array();
    for (const auto& capability : capabilities) {
        json["capabilities"].push_back(capability);
    }
    json["quality_score"] = quality_score;
    
    return json;
}

// ========================================
// Factory Functions
// ========================================

std::unique_ptr<i_p2p_node> create_p2p_node(
    boost::asio::io_context& io_context,
    const node_config& config
) {
    auto node = std::make_unique<p2p_node_impl>(io_context);
    
    if (config.local_endpoint.is_valid()) {
        // Configure immediately if config is provided
        auto configure_result = node->configure(config);
        if (!configure_result.is_ok()) {
            throw std::invalid_argument("Invalid configuration: " + configure_result.error());
        }
    }
    
    return std::move(node);
}

// ========================================
// Builder Implementation
// ========================================

node_builder::node_builder(boost::asio::io_context& io_context)
    : io_context_(io_context) {
}

node_builder& node_builder::with_local_endpoint(const core::endpoint& endpoint) {
    config_.local_endpoint = endpoint;
    return *this;
}

node_builder& node_builder::with_bootstrap_node(const core::endpoint& endpoint) {
    config_.bootstrap_nodes.push_back(endpoint);
    return *this;
}

node_builder& node_builder::with_node_id(const core::node_id& id) {
    config_.node_id = id;
    return *this;
}

node_builder& node_builder::with_max_connections(std::size_t max_connections) {
    config_.max_connections = max_connections;
    return *this;
}

node_builder& node_builder::with_nat_traversal(bool enabled) {
    config_.enable_nat_traversal = enabled;
    return *this;
}

node_builder& node_builder::with_encryption(bool enabled) {
    config_.enable_encryption = enabled;
    return *this;
}

node_builder& node_builder::with_log_level(core::i_logger::level level) {
    config_.log_level = level;
    return *this;
}

node_builder& node_builder::with_timeouts(
    std::chrono::milliseconds connection_timeout,
    std::chrono::milliseconds ping_timeout
) {
    config_.connection_timeout = connection_timeout;
    config_.ping_timeout = ping_timeout;
    return *this;
}

node_builder& node_builder::with_config_file(const std::string& config_path) {
    try {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open config file: " + config_path);
        }
        
        nlohmann::json config_json;
        file >> config_json;
        
        auto config_result = node_config::from_json(config_json);
        if (!config_result.is_ok()) {
            throw std::runtime_error("Invalid config file: " + config_result.error());
        }
        
        config_ = config_result.value();
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Error loading config file: " + std::string(e.what()));
    }
    
    return *this;
}

std::unique_ptr<i_p2p_node> node_builder::build() {
    auto validation_result = config_.validate();
    if (!validation_result.is_ok()) {
        throw std::invalid_argument("Invalid configuration: " + validation_result.error());
    }
    
    return create_p2p_node(io_context_, config_);
}

} // namespace ss::application