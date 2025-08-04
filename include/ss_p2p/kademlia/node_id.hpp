#pragma once

#include "../core/types.hpp"
#include "../core/result.hpp"

#include <vector>
#include <span>
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>

#include "boost/asio.hpp"

namespace ss::kademlia {

/**
 * @brief Legacy node_id adapter - wraps new core::node_id
 * 
 * This adapter provides backward compatibility for the legacy Kademlia node_id class
 * by wrapping the new ss::core::node_id implementation. All operations are delegated
 * to the underlying modern implementation while maintaining the legacy API surface.
 * 
 * RAII Principles:
 * - Automatic resource management through smart pointer
 * - Exception-safe operations
 * - Proper copy/move semantics
 */
class node_id {
public:
    using value_type = std::uint8_t;
    using id = std::array<std::uint8_t, 20>;
    static constexpr unsigned short K_NODE_ID_LENGTH = 20;

    /**
     * @brief Default constructor - creates zero-initialized node ID
     */
    node_id() noexcept;

    /**
     * @brief Constructor from raw data
     * @param from Pointer to 20-byte data
     */
    explicit node_id(const void* from);

    /**
     * @brief Copy constructor
     * @param nid Source node_id
     */
    node_id(const node_id& nid) noexcept;

    /**
     * @brief Move constructor
     * @param nid Source node_id
     */
    node_id(node_id&& nid) noexcept;

    /**
     * @brief Constructor from new core::node_id
     * @param core_id Core node ID to wrap
     */
    explicit node_id(const ss::core::node_id& core_id) noexcept;

    /**
     * @brief Constructor from new core::node_id (move)
     * @param core_id Core node ID to wrap
     */
    explicit node_id(ss::core::node_id&& core_id) noexcept;

    /**
     * @brief Copy assignment operator
     * @param nid Source node_id
     * @return Reference to this
     */
    node_id& operator=(const node_id& nid) noexcept;

    /**
     * @brief Move assignment operator
     * @param nid Source node_id
     * @return Reference to this
     */
    node_id& operator=(node_id&& nid) noexcept;

    /**
     * @brief Destructor
     */
    ~node_id() = default;

    /**
     * @brief Convert to string representation (hex)
     * @return Hexadecimal string representation
     */
    std::string to_str() const;

    /**
     * @brief Equality comparison
     * @param nid Other node_id
     * @return true if equal, false otherwise
     */
    bool operator==(const node_id& nid) const noexcept;

    /**
     * @brief Inequality comparison
     * @param nid Other node_id
     * @return true if not equal, false otherwise
     */
    bool operator!=(const node_id& nid) const noexcept;

    /**
     * @brief Less-than comparison for ordering
     * @param nid Other node_id
     * @return true if this < nid, false otherwise
     */
    bool operator<(const node_id& nid) const noexcept;

    /**
     * @brief Get raw ID data (legacy compatibility)
     * @return Array of bytes
     */
    id operator()() const noexcept;

    /**
     * @brief Array subscript operator
     * @param idx Index (0-19)
     * @return Byte at index
     */
    unsigned char operator[](unsigned short idx) const;

    /**
     * @brief Create zero node_id
     * @return Zero-initialized node_id
     */
    static node_id none() noexcept;

    /**
     * @brief Generate random node_id
     * @return Randomly generated node_id
     */
    static node_id random();

    /**
     * @brief Print node_id to stdout (legacy method)
     */
    void print() const;

    /**
     * @brief Get underlying core::node_id
     * @return Reference to core node_id
     */
    const ss::core::node_id& core() const noexcept;

    /**
     * @brief Get mutable underlying core::node_id
     * @return Mutable reference to core node_id
     */
    ss::core::node_id& core() noexcept;

    /**
     * @brief Direct access to raw data
     * @return Raw ID array
     */
    const id& data() const noexcept;

private:
    /// Underlying core node_id implementation
    std::unique_ptr<ss::core::node_id> core_id_;

    /// Cached raw data for legacy API compatibility
    mutable id cached_data_;
    mutable bool cache_valid_ = false;

    /**
     * @brief Update cached data from core_id
     */
    void update_cache() const;
};

/**
 * @brief Calculate node_id from endpoint (legacy compatibility)
 * @param ep UDP endpoint
 * @return Calculated node_id
 */
node_id calc_node_id(boost::asio::ip::udp::endpoint& ep);

/**
 * @brief Calculate XOR distance between two node_ids (legacy compatibility)
 * @param nid_1 First node_id
 * @param nid_2 Second node_id
 * @return XOR distance as leading zero count
 */
unsigned short calc_node_xor_distance(const node_id& nid_1, const node_id& nid_2);

/**
 * @brief Convert string to node_id (legacy compatibility)
 * @param from Source string
 * @return Parsed node_id
 */
node_id str_to_node_id(const std::string& from);

} // namespace ss::kademlia