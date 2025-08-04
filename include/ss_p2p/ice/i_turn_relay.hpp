#pragma once

#include "../core/types.hpp"
#include "../core/result.hpp"
#include "../core/interfaces.hpp"
#include "../network/i_transport.hpp"
#include "i_stun_client.hpp"

#include <memory>
#include <vector>
#include <functional>
#include <chrono>
#include <string>
#include <optional>
#include <unordered_map>

namespace ss::ice {

/**
 * @brief TURN message types according to RFC 5766
 */
enum class turn_message_type : std::uint16_t {
    /// Allocate Request (0x0003)
    allocate_request = 0x0003,
    /// Allocate Response (0x0103)
    allocate_response = 0x0103,
    /// Allocate Error Response (0x0113)
    allocate_error_response = 0x0113,
    /// Refresh Request (0x0004)
    refresh_request = 0x0004,
    /// Refresh Response (0x0104)
    refresh_response = 0x0104,
    /// Refresh Error Response (0x0114)
    refresh_error_response = 0x0114,
    /// Send Indication (0x0016)
    send_indication = 0x0016,
    /// Data Indication (0x0017)
    data_indication = 0x0017,
    /// CreatePermission Request (0x0008)
    create_permission_request = 0x0008,
    /// CreatePermission Response (0x0108)
    create_permission_response = 0x0108,
    /// CreatePermission Error Response (0x0118)
    create_permission_error_response = 0x0118,
    /// ChannelBind Request (0x0009)
    channel_bind_request = 0x0009,
    /// ChannelBind Response (0x0109)
    channel_bind_response = 0x0109,
    /// ChannelBind Error Response (0x0119)
    channel_bind_error_response = 0x0119
};

/**
 * @brief TURN attribute types according to RFC 5766
 */
enum class turn_attribute_type : std::uint16_t {
    /// CHANNEL-NUMBER (0x000C)
    channel_number = 0x000C,
    /// LIFETIME (0x000D)
    lifetime = 0x000D,
    /// XOR-PEER-ADDRESS (0x0012)
    xor_peer_address = 0x0012,
    /// DATA (0x0013)
    data = 0x0013,
    /// XOR-RELAYED-ADDRESS (0x0016)
    xor_relayed_address = 0x0016,
    /// EVEN-PORT (0x0018)
    even_port = 0x0018,
    /// REQUESTED-TRANSPORT (0x0019)
    requested_transport = 0x0019,
    /// DONT-FRAGMENT (0x001A)
    dont_fragment = 0x001A,
    /// RESERVATION-TOKEN (0x0022)
    reservation_token = 0x0022,
    /// BANDWIDTH (0x0010) - Non-standard extension
    bandwidth = 0x0010
};

/**
 * @brief TURN error codes specific to relay operations
 */
enum class turn_error_code : std::uint16_t {
    /// Forbidden (403)
    forbidden = 403,
    /// Allocation Mismatch (437)
    allocation_mismatch = 437,
    /// Wrong Credentials (441)
    wrong_credentials = 441,
    /// Unsupported Transport Protocol (442)
    unsupported_transport = 442,
    /// Allocation Quota Reached (486)
    allocation_quota_reached = 486,
    /// Insufficient Capacity (508)
    insufficient_capacity = 508
};

/**
 * @brief TURN relay error codes
 */
enum class turn_relay_error {
    /// No error occurred
    none = 0,
    /// Allocation request failed
    allocation_failed,
    /// Permission creation failed
    permission_failed,
    /// Channel binding failed
    channel_bind_failed,
    /// Data relay failed
    relay_failed,
    /// Authentication failed
    authentication_failed,
    /// Unsupported transport protocol
    unsupported_transport,
    /// Quota exceeded
    quota_exceeded,
    /// Insufficient bandwidth
    insufficient_bandwidth,
    /// Allocation expired
    allocation_expired,
    /// Invalid peer address
    invalid_peer,
    /// Channel not found
    channel_not_found,
    /// Network error
    network_error,
    /// Timeout occurred
    timeout,
    /// Unknown error
    unknown
};

/**
 * @brief Convert turn_relay_error to string representation
 */
std::string to_string(turn_relay_error error) noexcept;

/**
 * @brief Convert turn_message_type to string representation
 */
std::string to_string(turn_message_type type) noexcept;

/**
 * @brief Convert turn_error_code to string representation
 */
std::string to_string(turn_error_code code) noexcept;

/**
 * @brief TURN transport protocol identifier
 */
enum class turn_transport : std::uint8_t {
    /// UDP transport (17)
    udp = 17,
    /// TCP transport (6)
    tcp = 6
};

/**
 * @brief TURN channel number (0x4000-0x7FFF)
 */
class turn_channel_number {
public:
    static constexpr std::uint16_t MIN_CHANNEL = 0x4000;
    static constexpr std::uint16_t MAX_CHANNEL = 0x7FFF;

    /**
     * @brief Default constructor - invalid channel
     */
    constexpr turn_channel_number() noexcept : value_(0) {}

    /**
     * @brief Construct from channel number
     * @param value Channel number value
     */
    explicit constexpr turn_channel_number(std::uint16_t value) noexcept 
        : value_(value) {}

    /**
     * @brief Get channel number value
     */
    constexpr std::uint16_t value() const noexcept { return value_; }

    /**
     * @brief Check if channel number is valid
     */
    constexpr bool is_valid() const noexcept {
        return value_ >= MIN_CHANNEL && value_ <= MAX_CHANNEL;
    }

    /**
     * @brief Equality comparison
     */
    constexpr bool operator==(const turn_channel_number& other) const noexcept {
        return value_ == other.value_;
    }

    /**
     * @brief Inequality comparison
     */
    constexpr bool operator!=(const turn_channel_number& other) const noexcept {
        return value_ != other.value_;
    }

    /**
     * @brief Generate next available channel number
     * @param used_channels Set of already used channel numbers
     * @return Next available channel or invalid if none available
     */
    static turn_channel_number next_available(const std::vector<turn_channel_number>& used_channels);

private:
    std::uint16_t value_;
};

/**
 * @brief TURN allocation information
 */
struct turn_allocation {
    /// Relay server endpoint
    ss::core::endpoint server_endpoint;
    /// Allocated relay address (XOR-RELAYED-ADDRESS)
    ss::core::endpoint relayed_address;
    /// Allocation lifetime in seconds
    std::uint32_t lifetime_seconds;
    /// Transport protocol used
    turn_transport transport;
    /// Allocation timestamp
    std::chrono::steady_clock::time_point allocated_at;
    /// Username used for allocation
    std::string username;
    /// Bandwidth limit in bytes per second (0 = unlimited)
    std::uint64_t bandwidth_limit = 0;
    /// Number of permissions created
    std::uint32_t permission_count = 0;
    /// Number of active channels
    std::uint32_t channel_count = 0;

    /**
     * @brief Check if allocation is still valid
     * @return true if allocation hasn't expired
     */
    bool is_valid() const noexcept {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - allocated_at);
        return elapsed.count() < static_cast<std::int64_t>(lifetime_seconds);
    }

    /**
     * @brief Get remaining lifetime in seconds
     * @return Seconds until expiration (0 if expired)
     */
    std::uint32_t remaining_lifetime() const noexcept {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - allocated_at);
        auto remaining = static_cast<std::int64_t>(lifetime_seconds) - elapsed.count();
        return remaining > 0 ? static_cast<std::uint32_t>(remaining) : 0;
    }
};

/**
 * @brief TURN permission for peer communication
 */
struct turn_permission {
    /// Peer IP address
    ss::core::endpoint peer_address;
    /// Permission creation time
    std::chrono::steady_clock::time_point created_at;
    /// Permission lifetime (default 5 minutes)
    std::chrono::seconds lifetime{300};
    /// Usage statistics
    std::uint64_t bytes_sent = 0;
    std::uint64_t bytes_received = 0;
    std::uint32_t packets_sent = 0;
    std::uint32_t packets_received = 0;

    /**
     * @brief Check if permission is still valid
     */
    bool is_valid() const noexcept {
        auto now = std::chrono::steady_clock::now();
        return (now - created_at) < lifetime;
    }
};

/**
 * @brief TURN channel binding
 */
struct turn_channel_binding {
    /// Channel number
    turn_channel_number channel;
    /// Peer endpoint
    ss::core::endpoint peer_address;
    /// Binding creation time
    std::chrono::steady_clock::time_point created_at;
    /// Binding lifetime (default 10 minutes)
    std::chrono::seconds lifetime{600};
    /// Usage statistics
    std::uint64_t bytes_sent = 0;
    std::uint64_t bytes_received = 0;
    std::uint32_t packets_sent = 0;
    std::uint32_t packets_received = 0;

    /**
     * @brief Check if binding is still valid
     */
    bool is_valid() const noexcept {
        auto now = std::chrono::steady_clock::now();
        return (now - created_at) < lifetime;
    }
};

/**
 * @brief TURN relay configuration parameters
 */
struct turn_relay_config {
    /// Default allocation lifetime in seconds
    std::uint32_t default_lifetime = 600;
    /// Maximum allocation lifetime in seconds
    std::uint32_t max_lifetime = 3600;
    /// Maximum bandwidth per allocation (bytes/sec, 0 = unlimited)
    std::uint64_t max_bandwidth = 0;
    /// Maximum number of allocations per client
    std::uint32_t max_allocations_per_client = 5;
    /// Maximum number of permissions per allocation
    std::uint32_t max_permissions_per_allocation = 100;
    /// Maximum number of channel bindings per allocation
    std::uint32_t max_channels_per_allocation = 50;
    /// Permission refresh interval
    std::chrono::seconds permission_lifetime{300};
    /// Channel binding lifetime
    std::chrono::seconds channel_lifetime{600};
    /// Enable bandwidth limiting
    bool enable_bandwidth_limiting = true;
    /// Enable allocation quotas
    bool enable_quotas = true;
    /// Minimum port for relay addresses
    std::uint16_t min_port = 49152;
    /// Maximum port for relay addresses
    std::uint16_t max_port = 65535;
};

/**
 * @brief Callback for allocation completion
 */
using turn_allocation_handler = std::function<void(ss::core::result<turn_allocation, turn_relay_error>)>;

/**
 * @brief Callback for permission creation completion
 */
using turn_permission_handler = std::function<void(ss::core::result<turn_permission, turn_relay_error>)>;

/**
 * @brief Callback for channel binding completion
 */
using turn_channel_handler = std::function<void(ss::core::result<turn_channel_binding, turn_relay_error>)>;

/**
 * @brief Callback for data relay completion
 */
using turn_relay_handler = std::function<void(ss::core::result<void, turn_relay_error>)>;

/**
 * @brief Callback for incoming relayed data
 */
using turn_data_handler = std::function<void(const std::vector<std::uint8_t>& data, 
                                           const ss::core::endpoint& from)>;

/**
 * @brief Interface for TURN relay operations
 * 
 * Provides RFC 5766 compliant TURN relay functionality for NAT traversal
 * in scenarios where direct connection and hole punching are not possible.
 * Supports both centralized TURN servers and distributed peer-to-peer relay.
 * 
 * In distributed mode, any peer can act as a TURN relay for others,
 * enabling fully decentralized NAT traversal without dedicated infrastructure.
 */
class i_turn_relay : public ss::core::i_component {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~i_turn_relay() = default;

    /**
     * @brief Allocate relay address on TURN server
     * @param server_endpoint TURN server endpoint
     * @param transport Transport protocol (UDP/TCP)
     * @param lifetime_seconds Requested allocation lifetime
     * @param username Authentication username
     * @param password Authentication password
     * @return Awaitable result with allocation information
     */
    virtual ss::core::async_result<ss::core::result<turn_allocation, turn_relay_error>>
    allocate(const ss::core::endpoint& server_endpoint,
            turn_transport transport,
            std::uint32_t lifetime_seconds,
            const std::string& username,
            const std::string& password) = 0;

    /**
     * @brief Perform asynchronous allocation
     * @param server_endpoint TURN server endpoint
     * @param transport Transport protocol
     * @param lifetime_seconds Requested lifetime
     * @param username Authentication username
     * @param password Authentication password
     * @param handler Completion callback
     */
    virtual void allocate_async(const ss::core::endpoint& server_endpoint,
                               turn_transport transport,
                               std::uint32_t lifetime_seconds,
                               const std::string& username,
                               const std::string& password,
                               turn_allocation_handler handler) = 0;

    /**
     * @brief Refresh existing allocation
     * @param allocation Allocation to refresh
     * @param new_lifetime_seconds New lifetime in seconds
     * @return Awaitable result with updated allocation
     */
    virtual ss::core::async_result<ss::core::result<turn_allocation, turn_relay_error>>
    refresh_allocation(const turn_allocation& allocation,
                      std::uint32_t new_lifetime_seconds) = 0;

    /**
     * @brief Create permission for peer communication
     * @param allocation Relay allocation
     * @param peer_address Peer IP address
     * @return Awaitable result with permission information
     */
    virtual ss::core::async_result<ss::core::result<turn_permission, turn_relay_error>>
    create_permission(const turn_allocation& allocation,
                     const ss::core::endpoint& peer_address) = 0;

    /**
     * @brief Perform asynchronous permission creation
     * @param allocation Relay allocation
     * @param peer_address Peer IP address
     * @param handler Completion callback
     */
    virtual void create_permission_async(const turn_allocation& allocation,
                                        const ss::core::endpoint& peer_address,
                                        turn_permission_handler handler) = 0;

    /**
     * @brief Create channel binding for efficient data transfer
     * @param allocation Relay allocation
     * @param peer_address Peer endpoint
     * @param channel Channel number to bind
     * @return Awaitable result with channel binding information
     */
    virtual ss::core::async_result<ss::core::result<turn_channel_binding, turn_relay_error>>
    bind_channel(const turn_allocation& allocation,
                const ss::core::endpoint& peer_address,
                turn_channel_number channel) = 0;

    /**
     * @brief Perform asynchronous channel binding
     * @param allocation Relay allocation
     * @param peer_address Peer endpoint
     * @param channel Channel number
     * @param handler Completion callback
     */
    virtual void bind_channel_async(const turn_allocation& allocation,
                                   const ss::core::endpoint& peer_address,
                                   turn_channel_number channel,
                                   turn_channel_handler handler) = 0;

    /**
     * @brief Send data through TURN relay (Send Indication)
     * @param allocation Relay allocation
     * @param data Data to send
     * @param peer_address Target peer address
     * @return Awaitable result indicating success or failure
     */
    virtual ss::core::async_result<ss::core::result<void, turn_relay_error>>
    send_data(const turn_allocation& allocation,
             const std::vector<std::uint8_t>& data,
             const ss::core::endpoint& peer_address) = 0;

    /**
     * @brief Send data through channel (ChannelData)
     * @param allocation Relay allocation
     * @param channel_binding Channel binding
     * @param data Data to send
     * @return Awaitable result indicating success or failure
     */
    virtual ss::core::async_result<ss::core::result<void, turn_relay_error>>
    send_channel_data(const turn_allocation& allocation,
                     const turn_channel_binding& channel_binding,
                     const std::vector<std::uint8_t>& data) = 0;

    /**
     * @brief Perform asynchronous data send
     * @param allocation Relay allocation
     * @param data Data to send
     * @param peer_address Target peer
     * @param handler Completion callback
     */
    virtual void send_data_async(const turn_allocation& allocation,
                                const std::vector<std::uint8_t>& data,
                                const ss::core::endpoint& peer_address,
                                turn_relay_handler handler) = 0;

    /**
     * @brief Set handler for incoming relayed data
     * @param handler Data reception callback
     */
    virtual void set_data_handler(turn_data_handler handler) = 0;

    /**
     * @brief Remove data handler
     */
    virtual void clear_data_handler() = 0;

    /**
     * @brief Get active allocation by server endpoint
     * @param server_endpoint TURN server endpoint
     * @return Allocation if exists, empty optional otherwise
     */
    virtual std::optional<turn_allocation> 
    get_allocation(const ss::core::endpoint& server_endpoint) const noexcept = 0;

    /**
     * @brief Get all active allocations
     * @return Vector of active allocations
     */
    virtual std::vector<turn_allocation> get_allocations() const = 0;

    /**
     * @brief Get permissions for allocation
     * @param allocation Target allocation
     * @return Vector of active permissions
     */
    virtual std::vector<turn_permission> 
    get_permissions(const turn_allocation& allocation) const = 0;

    /**
     * @brief Get channel bindings for allocation
     * @param allocation Target allocation
     * @return Vector of active channel bindings
     */
    virtual std::vector<turn_channel_binding> 
    get_channel_bindings(const turn_allocation& allocation) const = 0;

    /**
     * @brief Release allocation and all associated resources
     * @param allocation Allocation to release
     * @return Awaitable void result
     */
    virtual ss::core::async_void release_allocation(const turn_allocation& allocation) = 0;

    /**
     * @brief Statistics for TURN relay operations
     */
    struct turn_relay_stats {
        std::uint64_t allocations_created = 0;
        std::uint64_t allocations_failed = 0;
        std::uint64_t permissions_created = 0;
        std::uint64_t channels_bound = 0;
        std::uint64_t bytes_relayed = 0;
        std::uint64_t packets_relayed = 0;
        std::uint32_t active_allocations = 0;
        std::uint32_t active_permissions = 0;
        std::uint32_t active_channels = 0;
        std::chrono::milliseconds avg_allocation_time{0};
        std::uint64_t bandwidth_bytes_per_sec = 0;
    };
    
    /**
     * @brief Get statistics for TURN relay operations
     * @return Current statistics
     */
    virtual turn_relay_stats get_stats() const noexcept = 0;

    /**
     * @brief Reset relay statistics
     */
    virtual void reset_stats() noexcept = 0;

    /**
     * @brief Set relay configuration
     * @param config New configuration
     */
    virtual void set_config(const turn_relay_config& config) = 0;

    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    virtual turn_relay_config get_config() const noexcept = 0;

    /**
     * @brief Set transport for TURN operations
     * @param transport Transport instance to use
     */
    virtual void set_transport(ss::network::transport_ptr transport) = 0;

    /**
     * @brief Get current transport
     * @return Current transport or nullptr if none set
     */
    virtual ss::network::transport_ptr get_transport() const noexcept = 0;

protected:
    /**
     * @brief Protected default constructor
     */
    i_turn_relay() = default;

    /**
     * @brief Protected copy constructor (deleted)
     */
    i_turn_relay(const i_turn_relay&) = delete;

    /**
     * @brief Protected move constructor (deleted)
     */
    i_turn_relay(i_turn_relay&&) = delete;

    /**
     * @brief Protected copy assignment (deleted)
     */
    i_turn_relay& operator=(const i_turn_relay&) = delete;

    /**
     * @brief Protected move assignment (deleted)
     */
    i_turn_relay& operator=(i_turn_relay&&) = delete;
};

/**
 * @brief Smart pointer type for TURN relay instances
 */
using turn_relay_ptr = std::shared_ptr<i_turn_relay>;

/**
 * @brief Weak pointer type for TURN relay instances
 */
using turn_relay_weak_ptr = std::weak_ptr<i_turn_relay>;

} // namespace ss::ice

/**
 * @brief Hash specialization for turn_channel_number
 */
template<>
struct std::hash<ss::ice::turn_channel_number> {
    std::size_t operator()(const ss::ice::turn_channel_number& channel) const noexcept {
        return std::hash<std::uint16_t>{}(channel.value());
    }
};