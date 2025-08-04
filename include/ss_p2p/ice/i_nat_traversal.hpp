#pragma once

#include "../core/types.hpp"
#include "../core/result.hpp"
#include "../core/interfaces.hpp"
#include "../network/i_transport.hpp"

#include <memory>
#include <vector>
#include <functional>
#include <chrono>
#include <string>
#include <optional>

namespace ss::ice {

/**
 * @brief NAT type classification based on RFC 4787 and RFC 5128
 */
enum class nat_type {
    /// Direct connection without NAT
    open_internet,
    /// Full cone NAT - allows any external host to communicate
    full_cone,
    /// Restricted cone NAT - allows only hosts that received packets
    restricted_cone,
    /// Port restricted cone NAT - allows only specific host:port combinations
    port_restricted_cone,
    /// Symmetric NAT - different mapping for each destination
    symmetric,
    /// NAT type could not be determined
    unknown,
    /// Network is unreachable or blocking
    blocked
};

/**
 * @brief NAT traversal error codes
 */
enum class nat_error {
    /// No error occurred
    none = 0,
    /// NAT detection failed
    detection_failed,
    /// Hole punching attempt failed
    hole_punch_failed,
    /// Timeout during NAT traversal
    timeout,
    /// Symmetric NAT detected (cannot traverse)
    symmetric_nat,
    /// Network is blocked or unreachable
    network_blocked,
    /// Invalid arguments provided
    invalid_argument,
    /// Resource exhaustion
    resource_exhausted,
    /// Operation already in progress
    operation_in_progress,
    /// Unknown error
    unknown
};

/**
 * @brief Convert nat_error to string representation
 * @param error Error code to convert
 * @return String description of the error
 */
std::string to_string(nat_error error) noexcept;

/**
 * @brief Convert nat_type to string representation
 * @param type NAT type to convert
 * @return String description of the NAT type
 */
std::string to_string(nat_type type) noexcept;

/**
 * @brief NAT detection result containing type and external address
 */
struct nat_detection_result {
    /// Detected NAT type
    nat_type type = nat_type::unknown;
    /// External (public) IP address visible to remote peers
    std::optional<ss::core::endpoint> external_endpoint;
    /// Local endpoint used for detection
    ss::core::endpoint local_endpoint;
    /// Detection latency in milliseconds
    std::chrono::milliseconds detection_latency{0};
    /// Additional metadata from detection process
    std::string metadata;
    
    /**
     * @brief Check if NAT can be traversed
     * @return true if traversal is possible, false otherwise
     */
    bool is_traversable() const noexcept {
        return type != nat_type::symmetric && 
               type != nat_type::blocked && 
               type != nat_type::unknown;
    }
};

/**
 * @brief Hole punching configuration parameters
 */
struct hole_punch_config {
    /// Maximum number of punch attempts
    std::uint32_t max_attempts = 10;
    /// Interval between punch attempts
    std::chrono::milliseconds attempt_interval{100};
    /// Total timeout for hole punching process
    std::chrono::milliseconds total_timeout{5000};
    /// Use incremental port prediction
    bool use_port_prediction = true;
    /// Number of predicted ports to try
    std::uint32_t port_prediction_range = 5;
    /// Enable parallel punching from multiple local ports
    bool enable_parallel_punch = true;
    /// Number of parallel punch threads
    std::uint32_t parallel_threads = 3;
};

/**
 * @brief Hole punching attempt result
 */
struct hole_punch_result {
    /// Whether hole punching succeeded
    bool success = false;
    /// Local endpoint that established connection
    ss::core::endpoint local_endpoint;
    /// Remote endpoint that was reached
    ss::core::endpoint remote_endpoint;
    /// Number of attempts made
    std::uint32_t attempts_made = 0;
    /// Total time taken for successful punch
    std::chrono::milliseconds elapsed_time{0};
    /// Method used for successful punch (direct, predicted, parallel)
    std::string punch_method;
};

/**
 * @brief Callback for NAT detection completion
 */
using nat_detection_handler = std::function<void(ss::core::result<nat_detection_result, nat_error>)>;

/**
 * @brief Callback for hole punching completion
 */
using hole_punch_handler = std::function<void(ss::core::result<hole_punch_result, nat_error>)>;

/**
 * @brief Callback for NAT traversal progress updates
 */
using traversal_progress_handler = std::function<void(const std::string& progress_info)>;

/**
 * @brief Interface for NAT traversal operations
 * 
 * Provides comprehensive NAT detection, classification, and traversal capabilities
 * for peer-to-peer connectivity. Supports various hole punching techniques and
 * integrates with distributed STUN/TURN infrastructure.
 * 
 * All operations are designed to work in a fully distributed environment without
 * requiring dedicated STUN/TURN servers.
 */
class i_nat_traversal : public ss::core::i_component {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~i_nat_traversal() = default;

    /**
     * @brief Detect NAT type and external address
     * @param local_endpoint Local endpoint to use for detection
     * @param detection_servers List of detection server endpoints
     * @return Awaitable result with NAT detection information
     */
    virtual ss::core::async_result<ss::core::result<nat_detection_result, nat_error>>
    detect_nat(const ss::core::endpoint& local_endpoint,
              const std::vector<ss::core::endpoint>& detection_servers) = 0;

    /**
     * @brief Perform asynchronous NAT detection
     * @param local_endpoint Local endpoint to use for detection
     * @param detection_servers List of detection server endpoints
     * @param handler Completion callback
     */
    virtual void detect_nat_async(const ss::core::endpoint& local_endpoint,
                                 const std::vector<ss::core::endpoint>& detection_servers,
                                 nat_detection_handler handler) = 0;

    /**
     * @brief Attempt hole punching to establish direct connection
     * @param local_endpoint Local endpoint to punch from
     * @param target_endpoint Target endpoint to reach
     * @param config Hole punching configuration
     * @return Awaitable result with hole punching outcome
     */
    virtual ss::core::async_result<ss::core::result<hole_punch_result, nat_error>>
    punch_hole(const ss::core::endpoint& local_endpoint,
              const ss::core::endpoint& target_endpoint,
              const hole_punch_config& config = {}) = 0;

    /**
     * @brief Perform asynchronous hole punching
     * @param local_endpoint Local endpoint to punch from
     * @param target_endpoint Target endpoint to reach
     * @param handler Completion callback
     * @param config Hole punching configuration
     */
    virtual void punch_hole_async(const ss::core::endpoint& local_endpoint,
                                 const ss::core::endpoint& target_endpoint,
                                 hole_punch_handler handler,
                                 const hole_punch_config& config = {}) = 0;

    /**
     * @brief Establish bidirectional connection through coordinated hole punching
     * @param local_endpoint Local endpoint to use
     * @param remote_peer_id Remote peer identifier
     * @param signaling_servers List of signaling relay endpoints
     * @return Awaitable result with established connection info
     */
    virtual ss::core::async_result<ss::core::result<hole_punch_result, nat_error>>
    establish_connection(const ss::core::endpoint& local_endpoint,
                        const ss::core::peer_id& remote_peer_id,
                        const std::vector<ss::core::endpoint>& signaling_servers) = 0;

    /**
     * @brief Get cached NAT detection result for local endpoint
     * @param local_endpoint Local endpoint to query
     * @return Cached detection result or empty if not available
     */
    virtual std::optional<nat_detection_result> 
    get_cached_nat_info(const ss::core::endpoint& local_endpoint) const noexcept = 0;

    /**
     * @brief Clear cached NAT detection results
     * @param local_endpoint Specific endpoint to clear, or all if not specified
     */
    virtual void clear_nat_cache(const std::optional<ss::core::endpoint>& local_endpoint = std::nullopt) = 0;

    /**
     * @brief Predict external port mapping for given local port
     * @param local_endpoint Local endpoint
     * @param nat_result Previous NAT detection result
     * @return Predicted external port range
     */
    virtual std::vector<std::uint16_t> 
    predict_external_ports(const ss::core::endpoint& local_endpoint,
                          const nat_detection_result& nat_result) const = 0;

    /**
     * @brief Check if direct connection is possible between two endpoints
     * @param local_nat Local NAT detection result
     * @param remote_nat Remote NAT detection result
     * @return true if direct connection is feasible
     */
    virtual bool can_connect_directly(const nat_detection_result& local_nat,
                                     const nat_detection_result& remote_nat) const noexcept = 0;

    /**
     * @brief Set progress handler for traversal operations
     * @param handler Progress update callback
     */
    virtual void set_progress_handler(traversal_progress_handler handler) = 0;

    /**
     * @brief Remove progress handler
     */
    virtual void clear_progress_handler() = 0;

    /**
     * @brief Statistics for NAT traversal operations
     */
    struct nat_traversal_stats {
        std::uint64_t detection_attempts = 0;
        std::uint64_t detection_successes = 0;
        std::uint64_t hole_punch_attempts = 0;
        std::uint64_t hole_punch_successes = 0;
        std::uint64_t connection_establishments = 0;
        std::chrono::milliseconds avg_detection_time{0};
        std::chrono::milliseconds avg_hole_punch_time{0};
        std::uint32_t cache_hits = 0;
        std::uint32_t cache_misses = 0;
    };
    
    /**
     * @brief Get statistics for NAT traversal operations
     * @return Current statistics
     */
    virtual nat_traversal_stats get_stats() const noexcept = 0;

    /**
     * @brief Reset traversal statistics
     */
    virtual void reset_stats() noexcept = 0;

protected:
    /**
     * @brief Protected default constructor
     */
    i_nat_traversal() = default;

    /**
     * @brief Protected copy constructor (deleted)
     */
    i_nat_traversal(const i_nat_traversal&) = delete;

    /**
     * @brief Protected move constructor (deleted)
     */
    i_nat_traversal(i_nat_traversal&&) = delete;

    /**
     * @brief Protected copy assignment (deleted)
     */
    i_nat_traversal& operator=(const i_nat_traversal&) = delete;

    /**
     * @brief Protected move assignment (deleted)
     */
    i_nat_traversal& operator=(i_nat_traversal&&) = delete;
};

/**
 * @brief Smart pointer type for NAT traversal instances
 */
using nat_traversal_ptr = std::shared_ptr<i_nat_traversal>;

/**
 * @brief Weak pointer type for NAT traversal instances
 */
using nat_traversal_weak_ptr = std::weak_ptr<i_nat_traversal>;

} // namespace ss::ice