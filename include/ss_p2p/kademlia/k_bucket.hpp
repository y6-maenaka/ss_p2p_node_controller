#pragma once

#include "./k_node.hpp"
#include "../dht/i_routing.hpp"

#include <vector>
#include <memory>
#include <shared_mutex>
#include <condition_variable>
#include <algorithm>
#include <chrono>

namespace ss::kademlia {

/**
 * @brief Legacy k_bucket adapter - wraps modern bucket functionality
 * 
 * This adapter provides backward compatibility for the legacy Kademlia k_bucket class
 * by maintaining a local cache of k_nodes while delegating operations to the underlying
 * modern DHT routing implementation. The adapter ensures thread-safety and maintains
 * the original k_bucket API surface.
 * 
 * RAII Principles:
 * - Automatic resource management with smart pointers
 * - Exception-safe operations
 * - Proper locking and synchronization
 */
class k_bucket {
public:
    using k_nodes = std::vector<k_node>;
    static constexpr unsigned short DEFAULT_K = 4;

    /**
     * @brief Update state enumeration (legacy compatibility)
     */
    enum update_state {
        added_back,     ///< Node was added to the back of bucket
        moved_back,     ///< Existing node was moved to back
        not_found,      ///< Node not found in bucket
        overflow,       ///< Bucket is full, cannot add
        error          ///< Error occurred during operation
    };

    /**
     * @brief Default constructor
     */
    k_bucket();

    /**
     * @brief Constructor with routing table reference
     * @param routing Routing table for delegated operations
     * @param bucket_index Index of this bucket in routing table
     */
    k_bucket(std::shared_ptr<ss::dht::i_routing> routing, std::uint32_t bucket_index);

    /**
     * @brief Copy constructor
     * @param other Source k_bucket
     */
    k_bucket(const k_bucket& other);

    /**
     * @brief Move constructor
     * @param other Source k_bucket
     */
    k_bucket(k_bucket&& other) noexcept;

    /**
     * @brief Copy assignment operator
     * @param other Source k_bucket
     * @return Reference to this
     */
    k_bucket& operator=(const k_bucket& other);

    /**
     * @brief Move assignment operator
     * @param other Source k_bucket
     * @return Reference to this
     */
    k_bucket& operator=(k_bucket&& other) noexcept;

    /**
     * @brief Destructor
     */
    ~k_bucket() = default;

    /**
     * @brief Auto-update bucket with node (legacy API)
     * @param kn Node to update
     * @return Update state result
     */
    update_state auto_update(k_node kn);

    /**
     * @brief Get nodes from front of bucket
     * @param count Number of nodes to get (default: 1)
     * @param ignore_nodes Nodes to ignore
     * @return Vector of nodes from front
     */
    k_nodes get_node_front(std::size_t count = 1, 
                          const k_nodes& ignore_nodes = k_nodes{});

    /**
     * @brief Get nodes from back of bucket
     * @param count Number of nodes to get (default: 1)
     * @param ignore_nodes Nodes to ignore
     * @return Vector of nodes from back
     */
    k_nodes get_node_back(std::size_t count = 1, 
                         const k_nodes& ignore_nodes = k_nodes{});

    /**
     * @brief Get all nodes in bucket
     * @return Vector of all nodes
     */
    k_nodes get_nodes();

    /**
     * @brief Add node to back of bucket
     * @param kn Node to add
     */
    void add_back(k_node kn);

    /**
     * @brief Move existing node to back of bucket
     * @param kn Node to move
     */
    void move_back(k_node& kn);

    /**
     * @brief Delete node from bucket (legacy - use sparingly)
     * @param kn Node to delete
     */
    void delete_node(k_node& kn);

    /**
     * @brief Swap node in bucket
     * @param node_src Source node to replace
     * @param node_dest Destination node to insert
     */
    void swap_node(k_node& node_src, k_node node_dest);

    /**
     * @brief Check if node exists in bucket
     * @param kn Node to check
     * @return true if exists, false otherwise
     */
    bool is_exist(k_node& kn) const;

    /**
     * @brief Check if bucket is full
     * @return true if full, false otherwise
     */
    bool is_full() const;

    /**
     * @brief Get current node count in bucket
     * @return Number of nodes
     */
    std::size_t get_node_count() const;

    /**
     * @brief Print bucket contents vertically
     */
    void print_vertical() const;

    /**
     * @brief Print bucket contents horizontally
     */
    void print_horizontal() const;

    /**
     * @brief Set routing table reference (for adapter usage)
     * @param routing Routing table interface
     * @param bucket_index Index of this bucket
     */
    void set_routing(std::shared_ptr<ss::dht::i_routing> routing, std::uint32_t bucket_index);

    /**
     * @brief Refresh bucket contents from routing table
     */
    void refresh_from_routing();

    /**
     * @brief Set maximum bucket size
     * @param k Maximum number of nodes
     */
    void set_k_value(std::size_t k);

    /**
     * @brief Get maximum bucket size
     * @return Maximum number of nodes
     */
    std::size_t get_k_value() const noexcept;

private:
    /// Local cache of nodes in this bucket
    mutable std::vector<k_node> nodes_;
    
    /// Routing table interface for delegated operations
    std::shared_ptr<ss::dht::i_routing> routing_;
    
    /// Index of this bucket in the routing table
    std::uint32_t bucket_index_ = 0;
    
    /// Maximum number of nodes in bucket
    std::size_t k_value_ = DEFAULT_K;
    
    /// Mutex for thread-safe access
    mutable std::shared_mutex mutex_;
    
    /// Cache validity flag
    mutable bool cache_valid_ = false;
    
    /// Last cache update time
    mutable std::chrono::steady_clock::time_point last_cache_update_;

    /**
     * @brief Update cache from routing table
     */
    void update_cache() const;

    /**
     * @brief Check if cache is valid
     * @return true if cache is recent enough
     */
    bool is_cache_valid() const;

    /**
     * @brief Find node in local cache
     * @param kn Node to find
     * @return Iterator to node or end()
     */
    std::vector<k_node>::iterator find_node(const k_node& kn);

    /**
     * @brief Find node in local cache (const version)
     * @param kn Node to find
     * @return Const iterator to node or cend()
     */
    std::vector<k_node>::const_iterator find_node(const k_node& kn) const;

    /**
     * @brief Filter out ignored nodes
     * @param nodes Input nodes
     * @param ignore_nodes Nodes to ignore
     * @return Filtered nodes
     */
    k_nodes filter_ignore_nodes(const k_nodes& nodes, const k_nodes& ignore_nodes) const;
};

} // namespace ss::kademlia