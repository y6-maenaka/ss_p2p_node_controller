#pragma once

#include <cstdint>
#include <chrono>

namespace ss::dht {

/**
 * @brief Kademlia DHT configuration parameters
 * 
 * Contains all configurable parameters for Kademlia protocol operation,
 * including k-bucket sizes, timeouts, and algorithm constants.
 * 
 * Default values are based on the original Kademlia paper and proven
 * implementations, but can be adjusted for specific network conditions.
 */
struct kademlia_config {
    // Basic Kademlia parameters

    /// K-bucket size (number of contacts per bucket)
    /// Higher values provide more redundancy but increase routing table size
    std::uint32_t k_bucket_size = 20;

    /// Alpha parameter (concurrency factor for iterative operations)
    /// Number of parallel lookups in find_node/find_value operations
    std::uint32_t alpha = 3;

    /// Beta parameter (maximum number of nodes to return in find_node)
    /// Typical value is same as k_bucket_size
    std::uint32_t beta = 20;

    /// Number of bits in node ID (typically 160 for SHA-1 based IDs)
    std::uint32_t node_id_bits = 160;

    // Timeout and interval settings

    /// RPC timeout for individual node communications
    std::chrono::milliseconds rpc_timeout{5000};

    /// Timeout for complete find operations (find_node, find_value)
    std::chrono::milliseconds find_timeout{30000};

    /// Interval between routing table maintenance operations
    std::chrono::minutes maintenance_interval{10};

    /// Interval for refreshing buckets (sending find_node for random IDs)
    std::chrono::minutes bucket_refresh_interval{60};

    /// Interval for republishing stored values
    std::chrono::minutes republish_interval{60};

    /// Maximum age before considering a node stale
    std::chrono::minutes node_timeout{15};

    /// Grace period before removing failed nodes
    std::chrono::minutes removal_grace_period{5};

    // Failure and retry handling

    /// Maximum number of consecutive failures before removing a node
    std::uint32_t max_failures = 3;

    /// Maximum number of retries for failed operations
    std::uint32_t max_retries = 2;

    /// Initial retry delay (exponential backoff)
    std::chrono::milliseconds initial_retry_delay{100};

    /// Maximum retry delay
    std::chrono::milliseconds max_retry_delay{5000};

    // Storage and replication

    /// Default TTL for stored values (0 = no default TTL)
    std::chrono::seconds default_value_ttl{0};

    /// Maximum TTL allowed for stored values
    std::chrono::hours max_value_ttl{24};

    /// Replication factor (number of nodes that should store each value)
    std::uint32_t replication_factor = 3;

    /// Maximum number of values to store locally
    std::uint64_t max_stored_values = 100000;

    /// Maximum storage size in bytes
    std::uint64_t max_storage_bytes = 1024 * 1024 * 100; // 100MB

    // Network optimization

    /// Enable bucket splitting for improved load balancing
    bool enable_bucket_splitting = true;

    /// Bucket split threshold (split when bucket has more nodes than this)
    std::uint32_t bucket_split_threshold = 25;

    /// Maximum bucket depth (prevent infinite splitting)
    std::uint32_t max_bucket_depth = 5;

    /// Enable least-recently-used (LRU) eviction from buckets
    bool enable_lru_eviction = true;

    /// Prefer nodes with lower latency in routing decisions
    bool prefer_low_latency = true;

    /// Maximum acceptable round-trip time for node communication
    std::chrono::milliseconds max_acceptable_rtt{10000};

    // Security and validation

    /// Enable cryptographic validation of node IDs
    bool enable_node_id_validation = false;

    /// Enable value signature verification
    bool enable_value_signatures = false;

    /// Maximum message size to accept
    std::uint32_t max_message_size = 1024 * 1024; // 1MB

    /// Rate limiting: maximum requests per second from single node
    std::uint32_t max_requests_per_second = 100;

    /// Enable protection against Sybil attacks
    bool enable_sybil_protection = false;

    // Performance tuning

    /// Number of I/O threads for network operations
    std::uint32_t io_threads = 1;

    /// Size of send/receive buffers
    std::uint32_t buffer_size = 65536; // 64KB

    /// Enable statistics collection (may impact performance)
    bool enable_statistics = true;

    /// Enable detailed logging (debug mode)
    bool enable_debug_logging = false;

    /// Batch size for bulk operations
    std::uint32_t batch_size = 10;

    // Bootstrap and discovery

    /// Minimum number of nodes needed before considering bootstrap complete
    std::uint32_t min_bootstrap_nodes = 10;

    /// Maximum time to wait for bootstrap completion
    std::chrono::minutes bootstrap_timeout{5};

    /// Enable passive node discovery (learn from incoming messages)
    bool enable_passive_discovery = true;

    /// Enable aggressive bootstrapping (more frequent initial lookups)
    bool enable_aggressive_bootstrap = false;

    // Maintenance and optimization

    /// Ping random nodes during maintenance to keep connections alive
    bool ping_random_nodes = true;

    /// Number of random pings to send during each maintenance cycle
    std::uint32_t random_pings_per_maintenance = 5;

    /// Enable periodic routing table optimization
    bool enable_routing_optimization = true;

    /// Remove nodes that consistently have high latency
    bool remove_high_latency_nodes = false;

    /// High latency threshold for node removal
    std::chrono::milliseconds high_latency_threshold{5000};

    /**
     * @brief Validate configuration parameters
     * @return true if configuration is valid, false otherwise
     */
    bool validate() const noexcept {
        // Basic parameter validation
        if (k_bucket_size == 0 || k_bucket_size > 255) return false;
        if (alpha == 0 || alpha > k_bucket_size) return false;
        if (beta == 0 || beta > 255) return false;
        if (node_id_bits == 0 || node_id_bits > 512) return false;
        
        // Timeout validation
        if (rpc_timeout.count() <= 0) return false;
        if (find_timeout < rpc_timeout) return false;
        if (maintenance_interval.count() <= 0) return false;
        
        // Failure handling validation
        if (max_failures == 0) return false;
        if (max_retries > 10) return false; // Reasonable upper bound
        
        // Storage validation
        if (replication_factor == 0 || replication_factor > k_bucket_size) return false;
        if (max_storage_bytes == 0) return false;
        
        // Network validation
        if (bucket_split_threshold <= k_bucket_size) return false;
        if (max_bucket_depth == 0 || max_bucket_depth > 20) return false;
        if (max_message_size == 0) return false;
        
        // Performance validation
        if (io_threads == 0 || io_threads > 64) return false;
        if (buffer_size < 1024) return false; // Minimum reasonable buffer
        
        return true;
    }

    /**
     * @brief Get configuration optimized for local testing
     * @return Configuration with shorter timeouts and smaller limits
     */
    static kademlia_config testing_config() noexcept {
        kademlia_config config;
        
        // Shorter timeouts for faster testing
        config.rpc_timeout = std::chrono::milliseconds{1000};
        config.find_timeout = std::chrono::milliseconds{5000};
        config.maintenance_interval = std::chrono::minutes{1};
        config.bucket_refresh_interval = std::chrono::minutes{2};
        config.node_timeout = std::chrono::minutes{2};
        
        // Smaller limits for resource conservation
        config.k_bucket_size = 8;
        config.alpha = 2;
        config.beta = 8;
        config.max_stored_values = 1000;
        config.max_storage_bytes = 1024 * 1024; // 1MB
        
        // More aggressive for testing
        config.enable_aggressive_bootstrap = true;
        config.min_bootstrap_nodes = 3;
        config.bootstrap_timeout = std::chrono::minutes{1};
        
        // Enable debugging
        config.enable_debug_logging = true;
        config.enable_statistics = true;
        
        return config;
    }

    /**
     * @brief Get configuration optimized for production use
     * @return Configuration with conservative timeouts and larger limits
     */
    static kademlia_config production_config() noexcept {
        kademlia_config config; // Use defaults
        
        // Conservative timeouts for production stability
        config.rpc_timeout = std::chrono::milliseconds{10000};
        config.find_timeout = std::chrono::milliseconds{60000};
        config.node_timeout = std::chrono::minutes{30};
        
        // Larger limits for production scale
        config.max_stored_values = 1000000;
        config.max_storage_bytes = 1024 * 1024 * 1024; // 1GB
        
        // Security enabled
        config.enable_sybil_protection = true;
        config.max_requests_per_second = 50; // More conservative
        
        // Performance optimizations
        config.io_threads = 4;
        config.enable_routing_optimization = true;
        config.remove_high_latency_nodes = true;
        
        // Disable debug features
        config.enable_debug_logging = false;
        
        return config;
    }

    /**
     * @brief Get configuration optimized for mobile/embedded devices
     * @return Configuration with minimal resource usage
     */
    static kademlia_config mobile_config() noexcept {
        kademlia_config config;
        
        // Minimal resource usage
        config.k_bucket_size = 10;
        config.alpha = 2;
        config.beta = 10;
        config.max_stored_values = 1000;
        config.max_storage_bytes = 1024 * 1024; // 1MB
        
        // Longer intervals to save battery
        config.maintenance_interval = std::chrono::minutes{30};
        config.bucket_refresh_interval = std::chrono::minutes{120};
        
        // Conservative network usage
        config.io_threads = 1;
        config.buffer_size = 16384; // 16KB
        config.max_requests_per_second = 10;
        
        // Disable optional features
        config.enable_statistics = false;
        config.enable_debug_logging = false;
        config.enable_routing_optimization = false;
        config.ping_random_nodes = false;
        
        return config;
    }
};

/**
 * @brief DHT operation result codes
 */
enum class dht_result_code {
    /// Operation completed successfully
    success = 0,
    /// Operation timed out
    timeout,
    /// Target not found
    not_found,
    /// Network error occurred
    network_error,
    /// Invalid parameters provided
    invalid_parameters,
    /// Storage capacity exceeded
    storage_full,
    /// Permission denied
    permission_denied,
    /// Duplicate entry
    duplicate,
    /// Operation cancelled
    cancelled,
    /// Internal error
    internal_error,
    /// Node unreachable
    unreachable,
    /// Rate limit exceeded
    rate_limited,
    /// Invalid signature
    invalid_signature,
    /// Unsupported operation
    unsupported
};

/**
 * @brief Convert DHT result code to string description
 * @param code Result code to convert
 * @return String description of the result code
 */
inline const char* to_string(dht_result_code code) noexcept {
    switch (code) {
        case dht_result_code::success: return "success";
        case dht_result_code::timeout: return "timeout";
        case dht_result_code::not_found: return "not_found";
        case dht_result_code::network_error: return "network_error";
        case dht_result_code::invalid_parameters: return "invalid_parameters";
        case dht_result_code::storage_full: return "storage_full";
        case dht_result_code::permission_denied: return "permission_denied";
        case dht_result_code::duplicate: return "duplicate";
        case dht_result_code::cancelled: return "cancelled";
        case dht_result_code::internal_error: return "internal_error";
        case dht_result_code::unreachable: return "unreachable";
        case dht_result_code::rate_limited: return "rate_limited";
        case dht_result_code::invalid_signature: return "invalid_signature";
        case dht_result_code::unsupported: return "unsupported";
        default: return "unknown";
    }
}

} // namespace ss::dht