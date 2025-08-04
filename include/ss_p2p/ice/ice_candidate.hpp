#pragma once

#include "../core/types.hpp"
#include "../core/result.hpp"
#include "i_nat_traversal.hpp"

#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <chrono>
#include <functional>

namespace ss::ice {

/**
 * @brief ICE candidate types according to RFC 8445
 */
enum class candidate_type {
    /// Host candidate - local interface address
    host,
    /// Server reflexive candidate - external address via STUN
    server_reflexive,
    /// Peer reflexive candidate - discovered during connectivity checks
    peer_reflexive,
    /// Relay candidate - address via TURN relay
    relay
};

/**
 * @brief ICE transport protocol types
 */
enum class candidate_transport {
    /// User Datagram Protocol
    udp,
    /// Transmission Control Protocol
    tcp
};

/**
 * @brief ICE candidate component types
 */
enum class candidate_component : std::uint8_t {
    /// RTP component (1)
    rtp = 1,
    /// RTCP component (2)
    rtcp = 2
};

/**
 * @brief ICE candidate foundation identifier
 * 
 * Foundation is used to group candidates that have the same type,
 * base IP address, protocol, and STUN server.
 */
class candidate_foundation {
public:
    /**
     * @brief Default constructor
     */
    candidate_foundation() = default;

    /**
     * @brief Construct from string
     * @param foundation Foundation string
     */
    explicit candidate_foundation(std::string foundation) 
        : value_(std::move(foundation)) {}

    /**
     * @brief Get foundation value
     */
    const std::string& value() const noexcept { return value_; }

    /**
     * @brief Equality comparison
     */
    bool operator==(const candidate_foundation& other) const noexcept {
        return value_ == other.value_;
    }

    /**
     * @brief Inequality comparison
     */
    bool operator!=(const candidate_foundation& other) const noexcept {
        return !(*this == other);
    }

    /**
     * @brief Less-than comparison for ordering
     */
    bool operator<(const candidate_foundation& other) const noexcept {
        return value_ < other.value_;
    }

    /**
     * @brief Generate foundation from candidate properties
     * @param type Candidate type
     * @param base_address Base IP address
     * @param stun_server STUN server used (empty for host candidates)
     * @return Generated foundation
     */
    static candidate_foundation generate(candidate_type type,
                                       const std::string& base_address,
                                       const std::string& stun_server = "");

private:
    std::string value_;
};

/**
 * @brief Convert candidate_type to string representation
 */
std::string to_string(candidate_type type) noexcept;

/**
 * @brief Convert candidate_transport to string representation
 */
std::string to_string(candidate_transport transport) noexcept;

/**
 * @brief Convert candidate_component to string representation
 */
std::string to_string(candidate_component component) noexcept;

/**
 * @brief ICE candidate priority calculation
 * 
 * Priority is calculated according to RFC 8445 formula:
 * priority = (2^24) * type_pref + (2^8) * local_pref + component_pref
 */
class candidate_priority {
public:
    /// Type preference values (higher = more preferred)
    static constexpr std::uint8_t TYPE_PREF_HOST = 126;
    static constexpr std::uint8_t TYPE_PREF_PEER_REFLEXIVE = 110;
    static constexpr std::uint8_t TYPE_PREF_SERVER_REFLEXIVE = 100;
    static constexpr std::uint8_t TYPE_PREF_RELAY = 0;

    /// Component preference values
    static constexpr std::uint8_t COMPONENT_PREF_RTP = 255;
    static constexpr std::uint8_t COMPONENT_PREF_RTCP = 254;

    /**
     * @brief Default constructor (priority 0)
     */
    constexpr candidate_priority() noexcept : value_(0) {}

    /**
     * @brief Construct from priority value
     * @param priority Priority value
     */
    explicit constexpr candidate_priority(std::uint32_t priority) noexcept 
        : value_(priority) {}

    /**
     * @brief Get priority value
     */
    constexpr std::uint32_t value() const noexcept { return value_; }

    /**
     * @brief Equality comparison
     */
    constexpr bool operator==(const candidate_priority& other) const noexcept {
        return value_ == other.value_;
    }

    /**
     * @brief Inequality comparison
     */
    constexpr bool operator!=(const candidate_priority& other) const noexcept {
        return value_ != other.value_;
    }

    /**
     * @brief Less-than comparison (higher priority = higher value)
     */
    constexpr bool operator<(const candidate_priority& other) const noexcept {
        return value_ < other.value_;
    }

    /**
     * @brief Greater-than comparison
     */
    constexpr bool operator>(const candidate_priority& other) const noexcept {
        return value_ > other.value_;
    }

    /**
     * @brief Calculate priority from components
     * @param type Candidate type
     * @param local_preference Local preference (0-65535)
     * @param component Component type
     * @return Calculated priority
     */
    static constexpr candidate_priority calculate(candidate_type type,
                                                std::uint16_t local_preference,
                                                candidate_component component) noexcept {
        std::uint8_t type_pref = 0;
        switch (type) {
            case candidate_type::host:
                type_pref = TYPE_PREF_HOST;
                break;
            case candidate_type::peer_reflexive:
                type_pref = TYPE_PREF_PEER_REFLEXIVE;
                break;
            case candidate_type::server_reflexive:
                type_pref = TYPE_PREF_SERVER_REFLEXIVE;
                break;
            case candidate_type::relay:
                type_pref = TYPE_PREF_RELAY;
                break;
        }

        std::uint8_t component_pref = (component == candidate_component::rtp) 
            ? COMPONENT_PREF_RTP : COMPONENT_PREF_RTCP;

        std::uint32_t priority = (static_cast<std::uint32_t>(type_pref) << 24) +
                                (static_cast<std::uint32_t>(local_preference) << 8) +
                                static_cast<std::uint32_t>(component_pref);
        
        return candidate_priority(priority);
    }

private:
    std::uint32_t value_;
};

/**
 * @brief ICE candidate representing a transport address
 * 
 * Contains all information needed for ICE connectivity checks
 * and candidate prioritization according to RFC 8445.
 */
class ice_candidate {
public:
    /**
     * @brief Constructor
     * @param foundation Candidate foundation
     * @param component Component ID
     * @param transport Transport protocol
     * @param priority Candidate priority
     * @param address Candidate address
     * @param type Candidate type
     */
    ice_candidate(candidate_foundation foundation,
                 candidate_component component,
                 candidate_transport transport,
                 candidate_priority priority,
                 ss::core::endpoint address,
                 candidate_type type)
        : foundation_(std::move(foundation))
        , component_(component)
        , transport_(transport)
        , priority_(priority)
        , address_(std::move(address))
        , type_(type)
        , gathered_at_(std::chrono::steady_clock::now()) {}

    /**
     * @brief Copy constructor
     */
    ice_candidate(const ice_candidate&) = default;

    /**
     * @brief Move constructor
     */
    ice_candidate(ice_candidate&&) noexcept = default;

    /**
     * @brief Copy assignment operator
     */
    ice_candidate& operator=(const ice_candidate&) = default;

    /**
     * @brief Move assignment operator
     */
    ice_candidate& operator=(ice_candidate&&) noexcept = default;

    /**
     * @brief Destructor
     */
    ~ice_candidate() = default;

    /**
     * @brief Get candidate foundation
     */
    const candidate_foundation& foundation() const noexcept { return foundation_; }

    /**
     * @brief Get component ID
     */
    candidate_component component() const noexcept { return component_; }

    /**
     * @brief Get transport protocol
     */
    candidate_transport transport() const noexcept { return transport_; }

    /**
     * @brief Get candidate priority
     */
    candidate_priority priority() const noexcept { return priority_; }

    /**
     * @brief Get candidate address
     */
    const ss::core::endpoint& address() const noexcept { return address_; }

    /**
     * @brief Get candidate type
     */
    candidate_type type() const noexcept { return type_; }

    /**
     * @brief Get base address for non-host candidates
     */
    const std::optional<ss::core::endpoint>& base_address() const noexcept { 
        return base_address_; 
    }

    /**
     * @brief Set base address
     * @param base Base address
     */
    void set_base_address(const ss::core::endpoint& base) {
        base_address_ = base;
    }

    /**
     * @brief Get related address (e.g., STUN server for srflx)
     */
    const std::optional<ss::core::endpoint>& related_address() const noexcept { 
        return related_address_; 
    }

    /**
     * @brief Set related address
     * @param related Related address
     */
    void set_related_address(const ss::core::endpoint& related) {
        related_address_ = related;
    }

    /**
     * @brief Get extension attributes
     */
    const std::unordered_map<std::string, std::string>& extensions() const noexcept {
        return extensions_;
    }

    /**
     * @brief Add extension attribute
     * @param name Attribute name
     * @param value Attribute value
     */
    void add_extension(const std::string& name, const std::string& value) {
        extensions_[name] = value;
    }

    /**
     * @brief Get time when candidate was gathered
     */
    std::chrono::steady_clock::time_point gathered_at() const noexcept {
        return gathered_at_;
    }

    /**
     * @brief Convert to SDP candidate string (a=candidate line)
     * @return SDP candidate string
     */
    std::string to_sdp() const;

    /**
     * @brief Parse candidate from SDP string
     * @param sdp_line SDP candidate line
     * @return Parsed candidate or error
     */
    static ss::core::result<ice_candidate, std::string> from_sdp(const std::string& sdp_line);

    /**
     * @brief Equality comparison
     */
    bool operator==(const ice_candidate& other) const noexcept {
        return foundation_ == other.foundation_ &&
               component_ == other.component_ &&
               transport_ == other.transport_ &&
               address_ == other.address_;
    }

    /**
     * @brief Inequality comparison
     */
    bool operator!=(const ice_candidate& other) const noexcept {
        return !(*this == other);
    }

    /**
     * @brief Less-than comparison for sorting (by priority descending)
     */
    bool operator<(const ice_candidate& other) const noexcept {
        return priority_ > other.priority_; // Higher priority first
    }

    /**
     * @brief Check if candidate is valid for connectivity checks
     */
    bool is_valid() const noexcept {
        return address_.is_valid() && priority_.value() > 0;
    }

    /**
     * @brief Check if candidate is a default candidate
     * @return true if this could be selected as default
     */
    bool is_default_candidate() const noexcept {
        return type_ == candidate_type::host || type_ == candidate_type::server_reflexive;
    }

    /**
     * @brief Get candidate age
     * @return Time since candidate was gathered
     */
    std::chrono::milliseconds age() const noexcept {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - gathered_at_);
    }

private:
    candidate_foundation foundation_;
    candidate_component component_;
    candidate_transport transport_;
    candidate_priority priority_;
    ss::core::endpoint address_;
    candidate_type type_;
    std::optional<ss::core::endpoint> base_address_;
    std::optional<ss::core::endpoint> related_address_;
    std::unordered_map<std::string, std::string> extensions_;
    std::chrono::steady_clock::time_point gathered_at_;
};

/**
 * @brief ICE candidate pair for connectivity checks
 * 
 * Represents a pair of local and remote candidates that can be used
 * for establishing connectivity according to RFC 8445.
 */
class candidate_pair {
public:
    /**
     * @brief Candidate pair state
     */
    enum class state {
        /// Pair is waiting to be checked
        waiting,
        /// Connectivity check is in progress
        in_progress,
        /// Connectivity check succeeded
        succeeded,
        /// Connectivity check failed
        failed,
        /// Pair is frozen (waiting for foundation unfreezing)
        frozen
    };

    /**
     * @brief Constructor
     * @param local Local candidate
     * @param remote Remote candidate
     */
    candidate_pair(ice_candidate local, ice_candidate remote)
        : local_(std::move(local))
        , remote_(std::move(remote))
        , state_(state::waiting)
        , priority_(calculate_pair_priority(local_.priority(), remote_.priority()))
        , created_at_(std::chrono::steady_clock::now()) {}

    /**
     * @brief Get local candidate
     */
    const ice_candidate& local() const noexcept { return local_; }

    /**
     * @brief Get remote candidate
     */
    const ice_candidate& remote() const noexcept { return remote_; }

    /**
     * @brief Get pair state
     */
    state get_state() const noexcept { return state_; }

    /**
     * @brief Set pair state
     * @param new_state New state
     */
    void set_state(state new_state) {
        state_ = new_state;
        last_state_change_ = std::chrono::steady_clock::now();
    }

    /**
     * @brief Get pair priority
     */
    std::uint64_t priority() const noexcept { return priority_; }

    /**
     * @brief Get nominated flag
     */
    bool is_nominated() const noexcept { return nominated_; }

    /**
     * @brief Set nominated flag
     * @param nominated Nomination status
     */
    void set_nominated(bool nominated) { nominated_ = nominated; }

    /**
     * @brief Get default flag
     */
    bool is_default() const noexcept { return is_default_; }

    /**
     * @brief Set default flag
     * @param is_default Default status
     */
    void set_default(bool is_default) { is_default_ = is_default; }

    /**
     * @brief Get round-trip time
     */
    std::optional<std::chrono::milliseconds> rtt() const noexcept { return rtt_; }

    /**
     * @brief Set round-trip time
     * @param round_trip_time RTT measurement
     */
    void set_rtt(std::chrono::milliseconds round_trip_time) {
        rtt_ = round_trip_time;
    }

    /**
     * @brief Get creation time
     */
    std::chrono::steady_clock::time_point created_at() const noexcept {
        return created_at_;
    }

    /**
     * @brief Get last state change time
     */
    std::chrono::steady_clock::time_point last_state_change() const noexcept {
        return last_state_change_;
    }

    /**
     * @brief Convert state to string
     */
    static std::string to_string(state s) noexcept;

    /**
     * @brief Equality comparison
     */
    bool operator==(const candidate_pair& other) const noexcept {
        return local_ == other.local_ && remote_ == other.remote_;
    }

    /**
     * @brief Inequality comparison
     */
    bool operator!=(const candidate_pair& other) const noexcept {
        return !(*this == other);
    }

    /**
     * @brief Less-than comparison for priority ordering
     */
    bool operator<(const candidate_pair& other) const noexcept {
        return priority_ > other.priority_; // Higher priority first
    }

    /**
     * @brief Check if pair foundations match
     */
    bool has_matching_foundations() const noexcept {
        return local_.foundation() == remote_.foundation();
    }

    /**
     * @brief Check if pair is valid for connectivity checks
     */
    bool is_valid() const noexcept {
        return local_.is_valid() && remote_.is_valid() &&
               local_.component() == remote_.component() &&
               local_.transport() == remote_.transport();
    }

    /**
     * @brief Calculate pair priority according to RFC 8445
     * @param local_priority Local candidate priority
     * @param remote_priority Remote candidate priority
     * @return Calculated pair priority
     */
    static std::uint64_t calculate_pair_priority(candidate_priority local_priority,
                                                candidate_priority remote_priority) noexcept {
        std::uint32_t higher = std::max(local_priority.value(), remote_priority.value());
        std::uint32_t lower = std::min(local_priority.value(), remote_priority.value());
        return (static_cast<std::uint64_t>(1) << 32) * std::min(higher, lower) + 
               2 * std::max(higher, lower) + (higher > lower ? 1 : 0);
    }

private:
    ice_candidate local_;
    ice_candidate remote_;
    state state_;
    std::uint64_t priority_;
    bool nominated_ = false;
    bool is_default_ = false;
    std::optional<std::chrono::milliseconds> rtt_;
    std::chrono::steady_clock::time_point created_at_;
    std::chrono::steady_clock::time_point last_state_change_;
};

/**
 * @brief ICE candidate gathering configuration
 */
struct candidate_gathering_config {
    /// Enable host candidate gathering
    bool gather_host_candidates = true;
    /// Enable server reflexive candidate gathering
    bool gather_server_reflexive = true;
    /// Enable relay candidate gathering
    bool gather_relay_candidates = false;
    /// List of STUN servers for server reflexive candidates
    std::vector<ss::core::endpoint> stun_servers;
    /// List of TURN servers for relay candidates
    std::vector<ss::core::endpoint> turn_servers;
    /// TURN authentication credentials
    std::unordered_map<std::string, std::string> turn_credentials;
    /// Interface filter (empty = all interfaces)
    std::vector<std::string> interface_filter;
    /// Port range for candidate allocation
    std::pair<std::uint16_t, std::uint16_t> port_range{0, 0}; // 0,0 = any port
    /// Maximum gathering time
    std::chrono::milliseconds gathering_timeout{5000};
    /// Enable IPv6 candidates
    bool enable_ipv6 = true;
    /// Enable IPv4 candidates  
    bool enable_ipv4 = true;
};

/**
 * @brief Type alias for candidate collection
 */
using candidate_collection = std::vector<ice_candidate>;

/**
 * @brief Type alias for candidate pair collection
 */
using candidate_pair_collection = std::vector<candidate_pair>;

} // namespace ss::ice

/**
 * @brief Hash specialization for candidate_foundation
 */
template<>
struct std::hash<ss::ice::candidate_foundation> {
    std::size_t operator()(const ss::ice::candidate_foundation& foundation) const noexcept {
        return std::hash<std::string>{}(foundation.value());
    }
};

/**
 * @brief Hash specialization for candidate_priority
 */
template<>
struct std::hash<ss::ice::candidate_priority> {
    std::size_t operator()(const ss::ice::candidate_priority& priority) const noexcept {
        return std::hash<std::uint32_t>{}(priority.value());
    }
};

/**
 * @brief Hash specialization for ice_candidate
 */
template<>
struct std::hash<ss::ice::ice_candidate> {
    std::size_t operator()(const ss::ice::ice_candidate& candidate) const noexcept {
        auto foundation_hash = std::hash<ss::ice::candidate_foundation>{}(candidate.foundation());
        auto component_hash = std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(candidate.component()));
        auto address_hash = std::hash<ss::core::endpoint>{}(candidate.address());
        
        return foundation_hash ^ (component_hash << 1) ^ (address_hash << 2);
    }
};