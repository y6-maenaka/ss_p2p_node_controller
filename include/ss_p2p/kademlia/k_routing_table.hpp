#pragma once

#include "./k_bucket.hpp"
#include "./k_node.hpp"
#include "./node_id.hpp"
#include "../dht/i_routing.hpp"
#include "../core/types.hpp"
#include "../ss_logger.hpp"

#include <array>
#include <span>
#include <string>
#include <vector>
#include <iostream>
#include <chrono>
#include <optional>
#include <memory>
#include <mutex>

#include "boost/asio.hpp"

namespace ss::kademlia {

class k_bucket_iterator;

/**
 * @brief Legacy k_routing_table adapter - wraps new dht::i_routing interface
 * 
 * This adapter provides backward compatibility for the legacy Kademlia routing table
 * by wrapping the new ss::dht::i_routing implementation. All operations are delegated
 * to the underlying modern implementation while maintaining the legacy API surface.
 * 
 * RAII Principles:
 * - Automatic resource management through smart pointers
 * - Exception-safe operations  
 * - Proper synchronization and thread safety
 */
class k_routing_table {
    friend k_bucket;
    friend k_bucket_iterator;

public:
    using routing_table = std::array<k_bucket, 160>;
    using update_state = k_bucket::update_state;
    
    static constexpr unsigned short K_BUCKET_COUNT = 160;
    static constexpr int bucket_first_index = 0;
    static constexpr int bucket_end_index = 159;

    /**
     * @brief Constructor
     * @param self_id Local node identifier
     * @param logger Logger instance (legacy compatibility)
     */
    k_routing_table(node_id self_id, ss_logger* logger);

    /**
     * @brief Constructor with routing interface
     * @param self_id Local node identifier
     * @param routing Modern routing interface
     * @param logger Logger instance
     */
    k_routing_table(node_id self_id, 
                   std::shared_ptr<ss::dht::i_routing> routing,
                   ss_logger* logger);

    /**
     * @brief Copy constructor
     * @param other Source routing table
     */
    k_routing_table(const k_routing_table& other);

    /**
     * @brief Move constructor
     * @param other Source routing table
     */
    k_routing_table(k_routing_table&& other) noexcept;

    /**
     * @brief Copy assignment operator
     * @param other Source routing table
     * @return Reference to this
     */
    k_routing_table& operator=(const k_routing_table& other);

    /**
     * @brief Move assignment operator
     * @param other Source routing table
     * @return Reference to this
     */
    k_routing_table& operator=(k_routing_table&& other) noexcept;

    /**
     * @brief Destructor
     */
    ~k_routing_table() = default;

    /**
     * @brief Swap node in routing table
     * @param node_src Source node to replace
     * @param node_dest Destination node to insert
     */
    void swap_node(k_node& node_src, k_node node_dest);

    /**
     * @brief Auto-update routing table with node
     * @param kn Node to update
     * @return Update state result
     */
    update_state auto_update(k_node kn);

    /**
     * @brief Check if node exists in routing table
     * @param kn Node to check
     * @return true if exists, false otherwise
     */
    bool is_exist(k_node& kn);

    /**
     * @brief Collect nodes around root node
     * @param root_node Root node for search
     * @param max_count Maximum number of nodes to collect
     * @param ignore_nodes Nodes to ignore
     * @return Vector of collected nodes
     */
    std::vector<k_node> collect_node(k_node& root_node, 
                                    std::size_t max_count, 
                                    const std::vector<k_node>& ignore_nodes = {});

    /**
     * @brief Get nodes from front of bucket containing root node
     * @param root_node Root node
     * @param count Number of nodes to get
     * @param ignore_nodes Nodes to ignore
     * @return Vector of nodes from front
     */
    std::vector<k_node> get_node_front(k_node& root_node, 
                                      std::size_t count = 1, 
                                      const std::vector<k_node>& ignore_nodes = {});

    /**
     * @brief Get nodes from back of bucket containing root node
     * @param root_node Root node
     * @param count Number of nodes to get
     * @param ignore_nodes Nodes to ignore
     * @return Vector of nodes from back
     */
    std::vector<k_node> get_node_back(k_node& root_node, 
                                     std::size_t count = 1, 
                                     const std::vector<k_node>& ignore_nodes = {});

    /**
     * @brief Get bucket containing node
     * @param kn Node to find bucket for
     * @return Reference to bucket
     */
    k_bucket& get_bucket(k_node& kn);

    /**
     * @brief Get bucket by branch index
     * @param branch Branch/bucket index
     * @return Reference to bucket
     */
    k_bucket& get_bucket(unsigned short branch);

    /**
     * @brief Calculate branch index for node
     * @param kn Node to calculate branch for
     * @return Branch index
     */
    unsigned short calc_branch_index(k_node& kn);

    /**
     * @brief Get total node count in routing table
     * @return Total number of nodes
     */
    std::size_t get_node_count();

    /**
     * @brief Get bucket iterator pointing to first bucket
     * @return Bucket iterator
     */
    k_bucket_iterator get_begin_bucket_iterator();

    /**
     * @brief Print routing table contents
     * @param start_branch Starting branch index (default: 1)
     */
    void print(int start_branch = 1);

    /**
     * @brief Set modern routing interface
     * @param routing Modern routing interface
     */
    void set_routing(std::shared_ptr<ss::dht::i_routing> routing);

    /**
     * @brief Get underlying routing interface
     * @return Shared pointer to routing interface
     */
    std::shared_ptr<ss::dht::i_routing> routing() const noexcept;

    /**
     * @brief Get local node ID
     * @return Local node identifier
     */
    const node_id& self_id() const noexcept;

    /**
     * @brief Refresh all buckets from routing table
     */
    void refresh_all_buckets();

    /**
     * @brief Set k-value for all buckets
     * @param k Maximum nodes per bucket
     */
    void set_k_value(std::size_t k);

public:
    /// Logger instance for legacy compatibility
    ss_logger* _logger;

private:
    /// Array of k-buckets
    routing_table table_;
    
    /// Local node identifier
    node_id self_id_;
    
    /// Modern routing interface
    std::shared_ptr<ss::dht::i_routing> routing_;
    
    /// Mutex for thread-safe access
    mutable std::shared_mutex mutex_;

    /**
     * @brief Calculate branch index (internal)
     * @param kn Node to calculate branch for
     * @return Branch index
     */
    unsigned short calc_branch(k_node& kn);
    
    /**
     * @brief Initialize buckets with routing interface
     */
    void initialize_buckets();
};

/**
 * @brief Legacy bucket iterator for routing table traversal
 * 
 * Provides iteration over k-buckets in the routing table with
 * legacy API compatibility.
 */
class k_bucket_iterator {
    friend k_routing_table;

public:
    /**
     * @brief Pre-increment operator
     * @return Reference to this iterator
     */
    k_bucket_iterator& operator++();

    /**
     * @brief Post-increment operator
     * @return Copy of iterator before increment
     */
    k_bucket_iterator operator++(int);

    /**
     * @brief Pre-decrement operator
     * @return Reference to this iterator
     */
    k_bucket_iterator& operator--();

    /**
     * @brief Post-decrement operator
     * @return Copy of iterator before decrement
     */
    k_bucket_iterator operator--(int);

    /**
     * @brief Equality comparison
     * @param itr Other iterator
     * @return true if equal, false otherwise
     */
    bool operator==(const k_bucket_iterator& itr);

    /**
     * @brief Dereference operator
     * @return Reference to current bucket
     */
    k_bucket& operator*();

    /**
     * @brief Get raw bucket reference
     * @return Reference to current bucket
     */
    k_bucket& get_raw();

    /**
     * @brief Get current branch index
     * @return Branch index
     */
    unsigned short get_branch();

    /**
     * @brief Check if iterator is invalid
     * @return true if invalid, false otherwise
     */
    bool is_invalid() const;

    /**
     * @brief Reset iterator to beginning
     * @return Reference to this iterator
     */
    k_bucket_iterator& to_begin();

    /**
     * @brief Get all nodes from current bucket
     * @return Vector of nodes
     */
    std::vector<k_node> get_nodes();

    /**
     * @brief Print iterator state (debug)
     */
    void _print_() const;

    /**
     * @brief Create invalid iterator
     * @return Invalid iterator instance
     */
    static k_bucket_iterator invalid();

private:
    /**
     * @brief Default constructor (creates invalid iterator)
     */
    k_bucket_iterator();

    /**
     * @brief Constructor with routing table and position
     * @param routing_table Pointer to routing table
     * @param bucket_itr Iterator to bucket in table
     * @param branch Current branch index
     */
    k_bucket_iterator(k_routing_table* routing_table,
                     k_routing_table::routing_table::iterator bucket_itr,
                     unsigned short branch);
    
    /// Pointer to routing table
    k_routing_table* routing_table_;
    
    /// Iterator to current bucket
    k_routing_table::routing_table::iterator bucket_itr_;
    
    /// Current branch index
    int branch_;
};

/**
 * @brief Convert endpoints to k_nodes (legacy compatibility)
 * @param eps Vector of UDP endpoints
 * @return Vector of k_nodes
 */
std::vector<k_node> eps_to_k_nodes(std::vector<boost::asio::ip::udp::endpoint> eps);

/**
 * @brief Generate random k_node (legacy compatibility)
 * @return Random k_node
 */
k_node generate_random_k_node();

} // namespace ss::kademlia