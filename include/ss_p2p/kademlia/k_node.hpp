#pragma once

#include "./node_id.hpp"
#include "../core/types.hpp"
#include "../dht/i_routing.hpp"

#include <array>
#include <string>
#include <memory>
#include <chrono>

#include "boost/asio.hpp"

namespace ss::kademlia {

/**
 * @brief Legacy k_node adapter - wraps new dht::peer_info
 * 
 * This adapter provides backward compatibility for the legacy Kademlia k_node class
 * by wrapping the new ss::dht::peer_info implementation. All operations are delegated
 * to the underlying modern implementation while maintaining the legacy API surface.
 * 
 * RAII Principles:
 * - Automatic resource management through smart pointer
 * - Exception-safe operations
 * - Proper copy/move semantics with shared_ptr
 */
class k_node : public std::enable_shared_from_this<k_node> {
public:
    using ref = std::shared_ptr<k_node>;

    /**
     * @brief Default constructor - creates blank node
     */
    k_node();

    /**
     * @brief Copy constructor
     * @param kn Source k_node
     */
    k_node(const k_node& kn);

    /**
     * @brief Move constructor
     * @param kn Source k_node
     */
    k_node(k_node&& kn) noexcept;

    /**
     * @brief Constructor from endpoint
     * @param ep UDP endpoint
     */
    explicit k_node(boost::asio::ip::udp::endpoint ep);

    /**
     * @brief Constructor from node_id and endpoint
     * @param id Node identifier
     * @param ep UDP endpoint
     */
    k_node(const node_id& id, boost::asio::ip::udp::endpoint ep);

    /**
     * @brief Constructor from peer_info (adapter constructor)
     * @param peer_info Modern peer information
     */
    explicit k_node(const ss::dht::peer_info& peer_info);

    /**
     * @brief Constructor from peer_info (move adapter constructor)
     * @param peer_info Modern peer information
     */
    explicit k_node(ss::dht::peer_info&& peer_info);

    /**
     * @brief Copy assignment operator
     * @param kn Source k_node
     * @return Reference to this
     */
    k_node& operator=(const k_node& kn);

    /**
     * @brief Move assignment operator
     * @param kn Source k_node
     * @return Reference to this
     */
    k_node& operator=(k_node&& kn) noexcept;

    /**
     * @brief Destructor
     */
    ~k_node() = default;

    /**
     * @brief Equality comparison
     * @param kn Other k_node
     * @return true if equal, false otherwise
     */
    bool operator==(const k_node& kn) const;

    /**
     * @brief Inequality comparison
     * @param kn Other k_node
     * @return true if not equal, false otherwise
     */
    bool operator!=(const k_node& kn) const;

    /**
     * @brief Get endpoint
     * @return UDP endpoint
     */
    boost::asio::ip::udp::endpoint get_endpoint() const;

    /**
     * @brief Get node ID reference
     * @return Reference to node_id
     */
    node_id& get_id();

    /**
     * @brief Get const node ID reference
     * @return Const reference to node_id
     */
    const node_id& get_id() const;

    /**
     * @brief Convert to string representation
     * @return String representation
     */
    std::string to_str();

    /**
     * @brief Get shared_ptr to this
     * @return Shared pointer to this k_node
     */
    ref to_ref();

    /**
     * @brief Print node information to stdout
     */
    void print() const;

    /**
     * @brief Create blank k_node
     * @return Blank k_node instance
     */
    static k_node blank();

    /**
     * @brief Get underlying peer_info
     * @return Reference to peer_info
     */
    const ss::dht::peer_info& peer_info() const noexcept;

    /**
     * @brief Get mutable underlying peer_info
     * @return Mutable reference to peer_info
     */
    ss::dht::peer_info& peer_info() noexcept;

    /**
     * @brief Check if node is alive
     * @param max_age Maximum age before considering stale
     * @return true if alive, false otherwise
     */
    bool is_alive(std::chrono::milliseconds max_age = std::chrono::minutes{5}) const;

    /**
     * @brief Update liveness information
     * @param rtt Round-trip time
     */
    void update_liveness(std::chrono::microseconds rtt = {});

    /**
     * @brief Record communication failure
     */
    void record_failure();

    /**
     * @brief Get failure count
     * @return Number of consecutive failures
     */
    std::uint32_t failure_count() const noexcept;

    /**
     * @brief Get round-trip time
     * @return Last measured RTT
     */
    std::chrono::microseconds rtt() const noexcept;

private:
    /// Underlying peer_info implementation
    std::unique_ptr<ss::dht::peer_info> peer_info_;
    
    /// Legacy node_id wrapper (for reference returns)
    mutable std::unique_ptr<node_id> legacy_id_;
    mutable bool legacy_id_valid_ = false;

    /**
     * @brief Update legacy node_id from peer_info
     */
    void update_legacy_id() const;
};

/**
 * @brief Convert string to k_node (legacy compatibility)
 * @param from Source string
 * @return Parsed k_node
 */
k_node str_to_k_node(const std::string& from);

} // namespace ss::kademlia