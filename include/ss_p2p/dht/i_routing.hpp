#pragma once

#include "../core/types.hpp"
#include "../core/result.hpp"
#include "../core/interfaces.hpp"

#include <memory>
#include <vector>
#include <chrono>
#include <functional>
#include <unordered_set>

namespace ss::dht {

/**
 * @brief Peer information stored in routing table
 * 
 * Contains network endpoint and metadata about a peer node in the DHT.
 * Includes liveness tracking and last seen timestamps.
 */
struct peer_info {
    /// Unique node identifier
    ss::core::node_id id;
    /// Network endpoint for communication
    ss::core::endpoint endpoint;
    /// Last successful communication timestamp
    std::chrono::steady_clock::time_point last_seen;
    /// Number of consecutive failed communications
    std::uint32_t failure_count = 0;
    /// Round-trip time for last successful communication
    std::chrono::microseconds rtt{0};
    
    /// Default constructor
    peer_info() = default;
    
    /// Constructor with id and endpoint
    peer_info(ss::core::node_id node_id, ss::core::endpoint ep) noexcept
        : id(std::move(node_id))
        , endpoint(std::move(ep))
        , last_seen(std::chrono::steady_clock::now()) {}
    
    /// Is this peer currently being pinged for liveness
    bool ping_pending = false;

    /**
     * @brief Check if peer is considered alive based on last contact
     * @param max_age Maximum age before considering peer stale
     * @return true if peer is alive, false otherwise
     */
    bool is_alive(std::chrono::milliseconds max_age) const noexcept {
        auto age = std::chrono::steady_clock::now() - last_seen;
        return std::chrono::duration_cast<std::chrono::milliseconds>(age) < max_age;
    }

    /**
     * @brief Update peer's liveness information
     * @param new_rtt Round-trip time for the communication
     */
    void update_liveness(std::chrono::microseconds new_rtt = {}) noexcept {
        last_seen = std::chrono::steady_clock::now();
        failure_count = 0;
        ping_pending = false;
        if (new_rtt.count() > 0) {
            rtt = new_rtt;
        }
    }

    /**
     * @brief Record communication failure
     */
    void record_failure() noexcept {
        ++failure_count;
        ping_pending = false;
    }

    /**
     * @brief Equality comparison based on node ID
     */
    bool operator==(const peer_info& other) const noexcept {
        return id == other.id;
    }

    /**
     * @brief Less-than comparison for ordering
     */
    bool operator<(const peer_info& other) const noexcept {
        return id < other.id;
    }
};

/**
 * @brief Find operation context and results
 * 
 * Contains parameters and results for find_node/find_value operations.
 * Used to track iterative lookup progress and accumulate results.
 */
struct find_context {
    /// Target ID we're looking for
    ss::core::node_id target;
    /// Maximum number of nodes to return
    std::uint32_t max_results;
    /// Already queried nodes (to avoid cycles)
    std::unordered_set<ss::core::node_id> queried_nodes;
    /// Best results found so far (closest to target)
    std::vector<peer_info> results;
    /// Timestamp when find operation started
    std::chrono::steady_clock::time_point start_time;
    /// Number of concurrent queries allowed
    std::uint32_t alpha;

    /**
     * @brief Constructor
     * @param target_id Target node ID to find
     * @param max_count Maximum results to return
     * @param concurrency Alpha parameter for concurrent queries
     */
    find_context(ss::core::node_id target_id, std::uint32_t max_count, std::uint32_t concurrency) noexcept
        : target(std::move(target_id))
        , max_results(max_count)
        , start_time(std::chrono::steady_clock::now())
        , alpha(concurrency) {}

    /**
     * @brief Add query result and maintain sorted order by distance
     * @param peers New peers to add to results
     */
    void add_results(const std::vector<peer_info>& peers) {
        for (const auto& peer : peers) {
            if (queried_nodes.find(peer.id) == queried_nodes.end()) {
                results.push_back(peer);
            }
        }
        
        // Sort by distance to target and limit results
        std::sort(results.begin(), results.end(), 
                 [this](const auto& a, const auto& b) {
                     return target.distance(a.id) < target.distance(b.id);
                 });
        
        if (results.size() > max_results) {
            results.resize(max_results);
        }
    }

    /**
     * @brief Mark node as queried
     * @param node_id Node ID that was queried
     */
    void mark_queried(const ss::core::node_id& node_id) {
        queried_nodes.insert(node_id);
    }

    /**
     * @brief Get duration since find operation started
     * @return Duration since start
     */
    std::chrono::milliseconds elapsed() const noexcept {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
    }
};

/**
 * @brief Routing statistics for monitoring and diagnostics
 */
struct routing_stats {
    /// Total number of peers in routing table
    std::uint32_t total_peers = 0;
    /// Number of buckets with at least one peer
    std::uint32_t active_buckets = 0;
    /// Total number of find operations performed
    std::uint64_t find_operations = 0;
    /// Total number of successful find operations
    std::uint64_t successful_finds = 0;
    /// Average time for find operations (microseconds)
    std::uint64_t avg_find_time_us = 0;
    /// Number of peers added since start
    std::uint64_t peers_added = 0;
    /// Number of peers removed since start
    std::uint64_t peers_removed = 0;
    /// Number of failed ping operations
    std::uint64_t failed_pings = 0;
    /// Timestamp of last routing table update
    std::chrono::steady_clock::time_point last_update{};
};

/**
 * @brief Interface for DHT routing table management
 * 
 * Provides operations for managing peer relationships in a distributed hash table.
 * Implementations should handle node discovery, closest node finding, and
 * peer liveness management according to specific DHT algorithms (e.g., Kademlia).
 */
class i_routing : public ss::core::i_component {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~i_routing() = default;

    /**
     * @brief Add or update a peer in the routing table
     * @param peer Peer information to add/update
     * @return Awaitable result indicating success or error
     */
    virtual ss::core::async_result<ss::core::result<void, std::string>>
    add_peer(peer_info peer) = 0;

    /**
     * @brief Remove a peer from the routing table
     * @param node_id Node ID of peer to remove
     * @return Awaitable result indicating if peer was removed
     */
    virtual ss::core::async_result<ss::core::result<bool, std::string>>
    remove_peer(const ss::core::node_id& node_id) = 0;

    /**
     * @brief Find closest peers to a target ID
     * @param target Target node ID
     * @param count Maximum number of peers to return (default: k-bucket size)
     * @return Awaitable result with list of closest peers
     */
    virtual ss::core::async_result<ss::core::result<std::vector<peer_info>, std::string>>
    find_closest_peers(const ss::core::node_id& target, std::uint32_t count = 0) = 0;

    /**
     * @brief Perform iterative find_node operation
     * @param target Target node ID to find
     * @param max_results Maximum number of results to collect
     * @return Awaitable result with closest nodes found
     */
    virtual ss::core::async_result<ss::core::result<std::vector<peer_info>, std::string>>
    find_node(const ss::core::node_id& target, std::uint32_t max_results = 20) = 0;

    /**
     * @brief Get peer information by node ID
     * @param node_id Node ID to look up
     * @return Awaitable result with peer info or error if not found
     */
    virtual ss::core::async_result<ss::core::result<peer_info, std::string>>
    get_peer(const ss::core::node_id& node_id) = 0;

    /**
     * @brief Check if a peer exists in the routing table
     * @param node_id Node ID to check
     * @return true if peer exists, false otherwise
     */
    virtual bool has_peer(const ss::core::node_id& node_id) const noexcept = 0;

    /**
     * @brief Get all peers from the routing table
     * @return Vector of all peer information
     */
    virtual std::vector<peer_info> get_all_peers() const = 0;

    /**
     * @brief Get peers from a specific bucket index
     * @param bucket_index Bucket index (0-159 for 160-bit key space)
     * @return Vector of peers in the specified bucket
     */
    virtual std::vector<peer_info> get_bucket_peers(std::uint32_t bucket_index) const = 0;

    /**
     * @brief Update peer liveness information
     * @param node_id Node ID of peer that responded
     * @param rtt Round-trip time for the communication
     * @return Awaitable result indicating success or error
     */
    virtual ss::core::async_result<ss::core::result<void, std::string>>
    update_peer_liveness(const ss::core::node_id& node_id, 
                        std::chrono::microseconds rtt = {}) = 0;

    /**
     * @brief Record communication failure for a peer
     * @param node_id Node ID of peer that failed to respond
     * @return Awaitable result indicating if peer was removed due to failures
     */
    virtual ss::core::async_result<ss::core::result<bool, std::string>>
    record_peer_failure(const ss::core::node_id& node_id) = 0;

    /**
     * @brief Perform maintenance operations (refresh buckets, ping stale peers)
     * @return Awaitable void result
     */
    virtual ss::core::async_void perform_maintenance() = 0;

    /**
     * @brief Get routing statistics
     * @return Current routing statistics
     */
    virtual routing_stats get_stats() const noexcept = 0;

    /**
     * @brief Reset routing statistics
     */
    virtual void reset_stats() noexcept = 0;

    /**
     * @brief Get the local node ID
     * @return Local node's identifier
     */
    virtual const ss::core::node_id& local_node_id() const noexcept = 0;

    /**
     * @brief Calculate bucket index for a given node ID
     * @param node_id Node ID to calculate bucket for
     * @return Bucket index (0-159 for 160-bit IDs)
     */
    virtual std::uint32_t bucket_index(const ss::core::node_id& node_id) const noexcept = 0;

    /**
     * @brief Get number of peers in routing table
     * @return Total peer count
     */
    virtual std::uint32_t peer_count() const noexcept = 0;

    /**
     * @brief Get number of active buckets (buckets with at least one peer)
     * @return Active bucket count
     */
    virtual std::uint32_t active_bucket_count() const noexcept = 0;

    /**
     * @brief Export routing table state for persistence
     * @return Serialized routing table data
     */
    virtual std::vector<std::uint8_t> export_state() const = 0;

    /**
     * @brief Import routing table state from serialized data
     * @param data Serialized routing table data
     * @return Awaitable result indicating success or error
     */
    virtual ss::core::async_result<ss::core::result<void, std::string>>
    import_state(const std::vector<std::uint8_t>& data) = 0;

protected:
    /**
     * @brief Protected default constructor
     */
    i_routing() = default;

    /**
     * @brief Protected copy constructor (deleted)
     */
    i_routing(const i_routing&) = delete;

    /**
     * @brief Protected move constructor (deleted)
     */
    i_routing(i_routing&&) = delete;

    /**
     * @brief Protected copy assignment (deleted)
     */
    i_routing& operator=(const i_routing&) = delete;

    /**
     * @brief Protected move assignment (deleted)
     */
    i_routing& operator=(i_routing&&) = delete;
};

/**
 * @brief Smart pointer type for routing instances
 */
using routing_ptr = std::shared_ptr<i_routing>;

/**
 * @brief Weak pointer type for routing instances
 */
using routing_weak_ptr = std::weak_ptr<i_routing>;

} // namespace ss::dht