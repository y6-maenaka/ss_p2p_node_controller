#pragma once

#include "../core/types.hpp"
#include "../core/result.hpp"
#include "../core/interfaces.hpp"

#include <memory>
#include <vector>
#include <chrono>
#include <string>
#include <unordered_map>
#include <functional>

namespace ss::dht {

/**
 * @brief DHT key type - 160-bit identifier for stored values
 */
using dht_key = ss::core::node_id;

/**
 * @brief DHT value with metadata and TTL management
 * 
 * Represents a value stored in the DHT with associated metadata,
 * expiration time, and replication information.
 */
struct dht_value {
    /// The actual value data
    std::vector<std::uint8_t> data;
    /// Optional value metadata (e.g., content type, encoding)
    std::string metadata;
    /// Timestamp when value was stored
    std::chrono::steady_clock::time_point stored_at;
    /// Time-to-live duration (0 = no expiration)
    std::chrono::seconds ttl{0};
    /// Publisher's node ID (who originally stored this value)
    ss::core::node_id publisher;
    /// Number of times this value has been republished
    std::uint32_t republish_count = 0;
    /// Sequence number for versioning (higher = newer)
    std::uint64_t sequence_number = 0;
    /// Digital signature of the value (optional, for authenticated storage)
    std::vector<std::uint8_t> signature;

    /**
     * @brief Constructor
     * @param value_data The value to store
     * @param time_to_live TTL for the value
     * @param publisher_id Node ID of the publisher
     */
    dht_value(std::vector<std::uint8_t> value_data, 
              std::chrono::seconds time_to_live,
              ss::core::node_id publisher_id) noexcept
        : data(std::move(value_data))
        , stored_at(std::chrono::steady_clock::now())
        , ttl(time_to_live)
        , publisher(std::move(publisher_id)) {}

    /**
     * @brief Check if value has expired based on TTL
     * @return true if expired, false otherwise
     */
    bool is_expired() const noexcept {
        if (ttl.count() == 0) {
            return false; // No expiration
        }
        auto age = std::chrono::steady_clock::now() - stored_at;
        return std::chrono::duration_cast<std::chrono::seconds>(age) >= ttl;
    }

    /**
     * @brief Get remaining TTL
     * @return Remaining time-to-live, or max duration if no TTL
     */
    std::chrono::seconds remaining_ttl() const noexcept {
        if (ttl.count() == 0) {
            return std::chrono::seconds::max();
        }
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - stored_at);
        return ttl > age ? ttl - age : std::chrono::seconds{0};
    }

    /**
     * @brief Refresh the stored timestamp (for republishing)
     */
    void refresh() noexcept {
        stored_at = std::chrono::steady_clock::now();
        ++republish_count;
    }

    /**
     * @brief Get age of the stored value
     * @return Duration since value was stored
     */
    std::chrono::seconds age() const noexcept {
        auto age_duration = std::chrono::steady_clock::now() - stored_at;
        return std::chrono::duration_cast<std::chrono::seconds>(age_duration);
    }

    /**
     * @brief Validate value signature (if present)
     * @param public_key Public key for signature verification
     * @return true if signature is valid or no signature present
     */
    bool validate_signature(const std::vector<std::uint8_t>& public_key) const noexcept {
        // Implementation would verify signature against data + metadata
        // For now, return true if no signature or if we have a signature and key
        return signature.empty() || (!signature.empty() && !public_key.empty());
    }
};

/**
 * @brief Storage operation statistics for monitoring
 */
struct storage_stats {
    /// Total number of stored key-value pairs
    std::uint64_t total_entries = 0;
    /// Total storage space used in bytes
    std::uint64_t total_storage_bytes = 0;
    /// Number of store operations performed
    std::uint64_t store_operations = 0;
    /// Number of retrieve operations performed
    std::uint64_t retrieve_operations = 0;
    /// Number of expired entries removed
    std::uint64_t expired_removals = 0;
    /// Number of entries removed due to storage limits
    std::uint64_t evicted_entries = 0;
    /// Average retrieval time in microseconds
    std::uint64_t avg_retrieve_time_us = 0;
    /// Average store time in microseconds
    std::uint64_t avg_store_time_us = 0;
    /// Number of republish operations performed
    std::uint64_t republish_operations = 0;
    /// Timestamp of last cleanup operation
    std::chrono::steady_clock::time_point last_cleanup{};
};

/**
 * @brief Storage configuration parameters
 */
struct storage_config {
    /// Maximum number of entries to store
    std::uint64_t max_entries = 100000;
    /// Maximum storage space in bytes
    std::uint64_t max_storage_bytes = 1024 * 1024 * 100; // 100MB
    /// Default TTL for stored values (0 = no default TTL)
    std::chrono::seconds default_ttl{0};
    /// Maximum TTL that can be set for values
    std::chrono::seconds max_ttl{std::chrono::hours{24}};
    /// Cleanup interval for expired entries
    std::chrono::minutes cleanup_interval{5};
    /// Republish interval for locally published values
    std::chrono::minutes republish_interval{60};
    /// Enable value signature verification
    bool enable_signature_verification = false;
    /// Maximum value size in bytes
    std::uint32_t max_value_size = 1024 * 1024; // 1MB
    /// LRU eviction when storage is full
    bool enable_lru_eviction = true;
};

/**
 * @brief Callback type for value change notifications
 */
using value_change_handler = std::function<void(const dht_key& key, const dht_value& value, bool is_removal)>;

/**
 * @brief Interface for DHT storage layer
 * 
 * Provides key-value storage with TTL management, replication support,
 * and expiration handling for distributed hash table operations.
 * 
 * Storage implementations should be thread-safe and handle concurrent
 * access from multiple DHT operations.
 */
class i_storage : public ss::core::i_component {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~i_storage() = default;

    /**
     * @brief Store a value with specified key
     * @param key Storage key
     * @param value Value to store
     * @return Awaitable result indicating success or error
     */
    virtual ss::core::async_result<ss::core::result<void, std::string>>
    store(const dht_key& key, dht_value value) = 0;

    /**
     * @brief Retrieve value by key
     * @param key Storage key to look up
     * @return Awaitable result with value or error if not found
     */
    virtual ss::core::async_result<ss::core::result<dht_value, std::string>>
    retrieve(const dht_key& key) = 0;

    /**
     * @brief Check if key exists in storage
     * @param key Key to check
     * @return true if key exists, false otherwise
     */
    virtual bool contains(const dht_key& key) const noexcept = 0;

    /**
     * @brief Remove value by key
     * @param key Key to remove
     * @return Awaitable result indicating if value was removed
     */
    virtual ss::core::async_result<ss::core::result<bool, std::string>>
    remove(const dht_key& key) = 0;

    /**
     * @brief Get all keys stored locally
     * @return Vector of all stored keys
     */
    virtual std::vector<dht_key> get_all_keys() const = 0;

    /**
     * @brief Get all key-value pairs
     * @return Map of all stored key-value pairs
     */
    virtual std::unordered_map<dht_key, dht_value> get_all_entries() const = 0;

    /**
     * @brief Find values that should be republished
     * @param max_age Maximum age before republishing
     * @return Awaitable result with keys that need republishing
     */
    virtual ss::core::async_result<std::vector<dht_key>>
    find_republish_candidates(std::chrono::seconds max_age) = 0;

    /**
     * @brief Clean up expired entries
     * @return Awaitable result with number of entries removed
     */
    virtual ss::core::async_result<std::uint64_t> cleanup_expired() = 0;

    /**
     * @brief Get keys that this node is responsible for storing
     * @param local_node_id This node's ID
     * @param replication_factor Number of nodes that should store each value
     * @return Vector of keys this node should store
     */
    virtual std::vector<dht_key> get_responsible_keys(
        const ss::core::node_id& local_node_id,
        std::uint32_t replication_factor) const = 0;

    /**
     * @brief Update TTL for an existing entry
     * @param key Key to update
     * @param new_ttl New TTL value
     * @return Awaitable result indicating success or error
     */
    virtual ss::core::async_result<ss::core::result<void, std::string>>
    update_ttl(const dht_key& key, std::chrono::seconds new_ttl) = 0;

    /**
     * @brief Refresh entry timestamp (used for republishing)
     * @param key Key to refresh
     * @return Awaitable result indicating success or error
     */
    virtual ss::core::async_result<ss::core::result<void, std::string>>
    refresh_entry(const dht_key& key) = 0;

    /**
     * @brief Get storage statistics
     * @return Current storage statistics
     */
    virtual storage_stats get_stats() const noexcept = 0;

    /**
     * @brief Reset storage statistics
     */
    virtual void reset_stats() noexcept = 0;

    /**
     * @brief Get current storage configuration
     * @return Current configuration
     */
    virtual storage_config get_config() const noexcept = 0;

    /**
     * @brief Update storage configuration
     * @param config New configuration
     * @return Result indicating success or error
     */
    virtual ss::core::result<void, std::string> set_config(const storage_config& config) = 0;

    /**
     * @brief Get current storage usage
     * @return Current number of stored entries
     */
    virtual std::uint64_t entry_count() const noexcept = 0;

    /**
     * @brief Get current storage size in bytes
     * @return Total storage usage in bytes
     */
    virtual std::uint64_t storage_size() const noexcept = 0;

    /**
     * @brief Check if storage has available capacity
     * @param additional_entries Number of entries to add
     * @param additional_bytes Number of bytes to add
     * @return true if capacity is available, false otherwise
     */
    virtual bool has_capacity(std::uint64_t additional_entries = 1, 
                             std::uint64_t additional_bytes = 0) const noexcept = 0;

    /**
     * @brief Add value change notification handler
     * @param handler Callback for value changes
     * @return Handler ID for later removal
     */
    virtual std::uint64_t add_change_handler(value_change_handler handler) = 0;

    /**
     * @brief Remove value change notification handler
     * @param handler_id Handler ID returned by add_change_handler
     * @return true if handler was removed, false if not found
     */
    virtual bool remove_change_handler(std::uint64_t handler_id) = 0;

    /**
     * @brief Export storage state for persistence
     * @return Serialized storage data
     */
    virtual std::vector<std::uint8_t> export_state() const = 0;

    /**
     * @brief Import storage state from serialized data
     * @param data Serialized storage data
     * @return Awaitable result indicating success or error
     */
    virtual ss::core::async_result<ss::core::result<void, std::string>>
    import_state(const std::vector<std::uint8_t>& data) = 0;

    /**
     * @brief Perform storage maintenance (cleanup, optimization)
     * @return Awaitable void result
     */
    virtual ss::core::async_void perform_maintenance() = 0;

protected:
    /**
     * @brief Protected default constructor
     */
    i_storage() = default;

    /**
     * @brief Protected copy constructor (deleted)
     */
    i_storage(const i_storage&) = delete;

    /**
     * @brief Protected move constructor (deleted)
     */
    i_storage(i_storage&&) = delete;

    /**
     * @brief Protected copy assignment (deleted)
     */
    i_storage& operator=(const i_storage&) = delete;

    /**
     * @brief Protected move assignment (deleted)
     */
    i_storage& operator=(i_storage&&) = delete;
};

/**
 * @brief Smart pointer type for storage instances
 */
using storage_ptr = std::shared_ptr<i_storage>;

/**
 * @brief Weak pointer type for storage instances
 */
using storage_weak_ptr = std::weak_ptr<i_storage>;

} // namespace ss::dht