#include "../../../../include/ss_p2p/dht/i_routing.hpp"
#include "../../../../include/ss_p2p/dht/node_id.hpp"
#include "../../../../include/ss_p2p/dht/kademlia_config.hpp"
#include "../../../../include/ss_p2p/core/result.hpp"

#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <boost/asio.hpp>

namespace ss::dht::impl {

/**
 * @brief Thread-safe routing table factory and management
 * 
 * Provides factory functions for creating routing table instances
 * and manages shared resources across multiple routing tables.
 */
class routing_table_factory {
public:
    /**
     * @brief Create a new Kademlia routing table instance
     * @param local_id Local node identifier
     * @param config Configuration parameters
     * @param io_context IO context for async operations
     * @return Shared pointer to routing table instance
     */
    static std::shared_ptr<i_routing> create_kademlia_routing_table(
        [[maybe_unused]] node_id local_id,
        [[maybe_unused]] kademlia_config config,
        [[maybe_unused]] boost::asio::io_context& io_context) {
        
        // The kademlia_routing_table is defined in kademlia_routing.cpp
        // For now, create a dummy implementation that returns nullptr
        // This should be replaced with proper factory pattern
        return nullptr;
        
        // TODO: Implement proper factory pattern or include header with class definition
        // return std::make_shared<kademlia_routing_table>(
        //     std::move(local_id), 
        //     std::move(config), 
        //     io_context);
    }

    /**
     * @brief Create a routing table with default configuration
     * @param local_id Local node identifier
     * @param io_context IO context for async operations
     * @return Shared pointer to routing table instance
     */
    static std::shared_ptr<i_routing> create_default_routing_table(
        node_id local_id,
        boost::asio::io_context& io_context) {
        
        return create_kademlia_routing_table(
            std::move(local_id),
            kademlia_config{}, // Default configuration
            io_context);
    }

    /**
     * @brief Create a routing table optimized for testing
     * @param local_id Local node identifier
     * @param io_context IO context for async operations
     * @return Shared pointer to routing table instance
     */
    static std::shared_ptr<i_routing> create_testing_routing_table(
        node_id local_id,
        boost::asio::io_context& io_context) {
        
        return create_kademlia_routing_table(
            std::move(local_id),
            kademlia_config::testing_config(),
            io_context);
    }

    /**
     * @brief Create a routing table optimized for production
     * @param local_id Local node identifier
     * @param io_context IO context for async operations
     * @return Shared pointer to routing table instance
     */
    static std::shared_ptr<i_routing> create_production_routing_table(
        node_id local_id,
        boost::asio::io_context& io_context) {
        
        return create_kademlia_routing_table(
            std::move(local_id),
            kademlia_config::production_config(),
            io_context);
    }

    /**
     * @brief Create a routing table optimized for mobile/embedded devices
     * @param local_id Local node identifier
     * @param io_context IO context for async operations
     * @return Shared pointer to routing table instance
     */
    static std::shared_ptr<i_routing> create_mobile_routing_table(
        node_id local_id,
        boost::asio::io_context& io_context) {
        
        return create_kademlia_routing_table(
            std::move(local_id),
            kademlia_config::mobile_config(),
            io_context);
    }

private:
    // Static factory class - no instances allowed
    routing_table_factory() = delete;
    ~routing_table_factory() = delete;
    routing_table_factory(const routing_table_factory&) = delete;
    routing_table_factory& operator=(const routing_table_factory&) = delete;
};

/**
 * @brief Routing table manager for coordinating multiple routing instances
 * 
 * Manages lifecycle and coordination between multiple routing table instances.
 * Provides shared services like peer discovery coordination and load balancing.
 */
class routing_table_manager {
private:
    /// Registered routing table instances
    std::unordered_map<node_id, std::weak_ptr<i_routing>> routing_tables_;
    /// Mutex for thread-safe access to routing tables map
    mutable std::shared_mutex tables_mutex_;
    /// IO context for async operations
    [[maybe_unused]] boost::asio::io_context& io_context_;
    /// Manager running state
    std::atomic<bool> running_{false};

public:
    /**
     * @brief Constructor
     * @param io_context IO context for async operations
     */
    explicit routing_table_manager(boost::asio::io_context& io_context)
        : io_context_(io_context) {}

    /**
     * @brief Destructor
     */
    ~routing_table_manager() {
        stop();
    }

    /**
     * @brief Start the routing table manager
     */
    void start() {
        running_.store(true);
    }

    /**
     * @brief Stop the routing table manager
     */
    void stop() {
        if (!running_.exchange(false)) {
            return; // Already stopped
        }

        std::unique_lock lock(tables_mutex_);
        routing_tables_.clear();
    }

    /**
     * @brief Register a routing table instance
     * @param routing_table Routing table to register
     * @return true if registered successfully
     */
    bool register_routing_table(std::shared_ptr<i_routing> routing_table) {
        if (!routing_table || !running_.load()) {
            return false;
        }

        std::unique_lock lock(tables_mutex_);
        auto local_id = routing_table->local_node_id();
        routing_tables_[local_id] = routing_table;
        return true;
    }

    /**
     * @brief Unregister a routing table instance
     * @param local_id Local node ID of routing table to unregister
     * @return true if unregistered successfully
     */
    bool unregister_routing_table(const node_id& local_id) {
        std::unique_lock lock(tables_mutex_);
        return routing_tables_.erase(local_id) > 0;
    }

    /**
     * @brief Get routing table by local node ID
     * @param local_id Local node ID
     * @return Shared pointer to routing table or nullptr if not found
     */
    std::shared_ptr<i_routing> get_routing_table(const node_id& local_id) const {
        std::shared_lock lock(tables_mutex_);
        auto it = routing_tables_.find(local_id);
        if (it != routing_tables_.end()) {
            return it->second.lock();
        }
        return nullptr;
    }

    /**
     * @brief Get all active routing table instances
     * @return Vector of active routing table instances
     */
    std::vector<std::shared_ptr<i_routing>> get_all_routing_tables() const {
        std::shared_lock lock(tables_mutex_);
        std::vector<std::shared_ptr<i_routing>> tables;
        
        for (const auto& [id, weak_table] : routing_tables_) {
            if (auto table = weak_table.lock()) {
                tables.push_back(table);
            }
            // Note: We don't remove expired pointers in const method
        }
        
        return tables;
    }

    /**
     * @brief Get number of active routing tables
     * @return Number of active routing table instances
     */
    std::size_t active_table_count() const {
        std::shared_lock lock(tables_mutex_);
        std::size_t count = 0;
        
        for (const auto& [id, weak_table] : routing_tables_) {
            if (weak_table.lock()) {
                ++count;
            }
            // Note: We don't remove expired pointers in const method
        }
        
        return count;
    }

    /**
     * @brief Perform maintenance on all routing tables
     * @return Number of tables that performed maintenance
     */
    boost::asio::awaitable<std::size_t> perform_global_maintenance() {
        auto tables = get_all_routing_tables();
        std::size_t maintenance_count = 0;

        for (auto& table : tables) {
            if (table && table->is_running()) {
                try {
                    co_await table->perform_maintenance();
                    ++maintenance_count;
                } catch (const std::exception&) {
                    // Log error and continue with other tables
                }
            }
        }

        co_return maintenance_count;
    }

    /**
     * @brief Coordinate peer discovery across multiple routing tables
     * @param discovered_peer Peer information discovered by one table
     * @return Number of tables that added the peer
     */
    boost::asio::awaitable<std::size_t> propagate_peer_discovery(const peer_info& discovered_peer) {
        auto tables = get_all_routing_tables();
        std::size_t propagation_count = 0;

        for (auto& table : tables) {
            if (table && table->is_running()) {
                // Don't add peer to its own routing table
                if (table->local_node_id() == discovered_peer.id) {
                    continue;
                }

                try {
                    auto result = co_await table->add_peer(discovered_peer);
                    if (result.is_ok()) {
                        ++propagation_count;
                    }
                } catch (const std::exception&) {
                    // Log error and continue with other tables
                }
            }
        }

        co_return propagation_count;
    }

    /**
     * @brief Get aggregated statistics from all routing tables
     * @return Combined routing statistics
     */
    routing_stats get_aggregated_stats() const {
        auto tables = get_all_routing_tables();
        routing_stats aggregated{};

        for (const auto& table : tables) {
            if (table && table->is_running()) {
                auto stats = table->get_stats();
                aggregated.total_peers += stats.total_peers;
                aggregated.active_buckets += stats.active_buckets;
                aggregated.find_operations += stats.find_operations;
                aggregated.successful_finds += stats.successful_finds;
                aggregated.peers_added += stats.peers_added;
                aggregated.peers_removed += stats.peers_removed;
                aggregated.failed_pings += stats.failed_pings;
                
                // Average the timing statistics
                if (stats.avg_find_time_us > 0) {
                    aggregated.avg_find_time_us = 
                        (aggregated.avg_find_time_us + stats.avg_find_time_us) / 2;
                }
                
                // Keep the most recent update time
                if (stats.last_update > aggregated.last_update) {
                    aggregated.last_update = stats.last_update;
                }
            }
        }

        return aggregated;
    }

    /**
     * @brief Check if manager is running
     * @return true if running, false otherwise
     */
    bool is_running() const noexcept {
        return running_.load();
    }

    // Note: routing_tables_ is already declared as a member variable above
};

/**
 * @brief Global routing table manager instance
 * 
 * Provides singleton access to routing table management services.
 */
class global_routing_manager {
private:
    static std::unique_ptr<routing_table_manager> instance_;
    static std::mutex instance_mutex_;

public:
    /**
     * @brief Get or create global routing manager instance
     * @param io_context IO context for async operations
     * @return Reference to global routing manager
     */
    static routing_table_manager& get_instance(boost::asio::io_context& io_context) {
        std::lock_guard lock(instance_mutex_);
        if (!instance_) {
            instance_ = std::make_unique<routing_table_manager>(io_context);
            instance_->start();
        }
        return *instance_;
    }

    /**
     * @brief Destroy global routing manager instance
     */
    static void destroy_instance() {
        std::lock_guard lock(instance_mutex_);
        instance_.reset();
    }

    /**
     * @brief Check if global instance exists
     * @return true if instance exists, false otherwise
     */
    static bool has_instance() {
        std::lock_guard lock(instance_mutex_);
        return instance_ != nullptr;
    }

private:
    // Static singleton class
    global_routing_manager() = delete;
    ~global_routing_manager() = delete;
    global_routing_manager(const global_routing_manager&) = delete;
    global_routing_manager& operator=(const global_routing_manager&) = delete;
};

// Static member definitions
std::unique_ptr<routing_table_manager> global_routing_manager::instance_;
std::mutex global_routing_manager::instance_mutex_;

/**
 * @brief Utility functions for routing table operations
 */
namespace routing_utils {

    /**
     * @brief Create and initialize a routing table with bootstrap peers
     * @param local_id Local node identifier
     * @param bootstrap_peers Initial peers to add to routing table
     * @param config Configuration parameters
     * @param io_context IO context for async operations
     * @return Initialized routing table
     */
    boost::asio::awaitable<std::shared_ptr<i_routing>> create_and_bootstrap_routing_table(
        node_id local_id,
        const std::vector<peer_info>& bootstrap_peers,
        const kademlia_config& config,
        boost::asio::io_context& io_context) {
        
        auto routing_table = routing_table_factory::create_kademlia_routing_table(
            local_id, config, io_context);

        // Start the routing table
        co_await routing_table->start();

        // Add bootstrap peers
        for (const auto& peer : bootstrap_peers) {
            auto result = co_await routing_table->add_peer(peer);
            // Continue even if some peers fail to add
        }

        // Register with global manager
        auto& manager = global_routing_manager::get_instance(io_context);
        manager.register_routing_table(routing_table);

        co_return routing_table;
    }

    /**
     * @brief Calculate optimal k-bucket size based on network conditions
     * @param expected_network_size Expected number of nodes in network
     * @param churn_rate Expected node churn rate (nodes leaving/joining per minute)
     * @return Recommended k-bucket size
     */
    std::uint32_t calculate_optimal_k_bucket_size(
        std::uint64_t expected_network_size,
        double churn_rate) {
        
        // Base k-bucket size from Kademlia paper
        std::uint32_t base_k = 20;
        
        // Adjust based on network size
        if (expected_network_size < 1000) {
            base_k = 10; // Smaller networks need smaller buckets
        } else if (expected_network_size > 1000000) {
            base_k = 30; // Very large networks benefit from larger buckets
        }
        
        // Adjust based on churn rate
        if (churn_rate > 10.0) {
            base_k = static_cast<std::uint32_t>(base_k * 1.5); // High churn needs more redundancy
        } else if (churn_rate < 1.0) {
            base_k = static_cast<std::uint32_t>(base_k * 0.8); // Low churn can use fewer peers
        }
        
        // Ensure reasonable bounds
        return std::clamp(base_k, 5u, 50u);
    }

    /**
     * @brief Estimate network size from routing table state
     * @param routing_table Routing table to analyze
     * @return Estimated total network size
     */
    std::uint64_t estimate_network_size(const i_routing& routing_table) {
        auto stats = routing_table.get_stats();
        
        // Simple estimation based on active buckets and peer count
        // More sophisticated methods could use statistical sampling
        if (stats.active_buckets == 0) {
            return stats.total_peers;
        }
        
        // Estimate based on bucket utilization
        std::uint64_t estimated_size = static_cast<std::uint64_t>(
            stats.total_peers * std::pow(2.0, stats.active_buckets));
        
        // Apply reasonable bounds
        return std::clamp(estimated_size, 
                         static_cast<std::uint64_t>(stats.total_peers),
                         static_cast<std::uint64_t>(1000000000)); // 1 billion max
    }

    /**
     * @brief Validate routing table health and suggest optimizations
     * @param routing_table Routing table to analyze
     * @return Vector of health check results and recommendations
     */
    std::vector<std::string> health_check(const i_routing& routing_table) {
        std::vector<std::string> recommendations;
        auto stats = routing_table.get_stats();
        
        // Check peer count
        if (stats.total_peers < 10) {
            recommendations.push_back("Low peer count - consider more aggressive bootstrapping");
        }
        
        // Check bucket distribution
        if (stats.active_buckets < 5) {
            recommendations.push_back("Few active buckets - network diversity may be limited");
        }
        
        // Check success rate
        if (stats.find_operations > 0) {
            double success_rate = static_cast<double>(stats.successful_finds) / stats.find_operations;
            if (success_rate < 0.8) {
                recommendations.push_back("Low find operation success rate - check network connectivity");
            }
        }
        
        // Check failure rate
        if (stats.failed_pings > stats.total_peers * 2) {
            recommendations.push_back("High ping failure rate - consider adjusting timeout settings");
        }
        
        // Check maintenance frequency
        auto time_since_update = std::chrono::steady_clock::now() - stats.last_update;
        if (time_since_update > std::chrono::minutes{30}) {
            recommendations.push_back("Long time since last update - ensure maintenance is running");
        }
        
        if (recommendations.empty()) {
            recommendations.push_back("Routing table health is good");
        }
        
        return recommendations;
    }

} // namespace routing_utils

} // namespace ss::dht::impl