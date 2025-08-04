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
#include <cstdint>
#include <array>

namespace ss::ice {

/**
 * @brief STUN message types according to RFC 5389
 */
enum class stun_message_type : std::uint16_t {
    /// Binding Request (0x0001)
    binding_request = 0x0001,
    /// Binding Response (0x0101)
    binding_response = 0x0101,
    /// Binding Error Response (0x0111)
    binding_error_response = 0x0111,
    /// Shared Secret Request (0x0002) - RFC 3489 only
    shared_secret_request = 0x0002,
    /// Shared Secret Response (0x0102) - RFC 3489 only
    shared_secret_response = 0x0102,
    /// Shared Secret Error Response (0x0112) - RFC 3489 only
    shared_secret_error_response = 0x0112
};

/**
 * @brief STUN attribute types according to RFC 5389
 */
enum class stun_attribute_type : std::uint16_t {
    /// MAPPED-ADDRESS (0x0001)
    mapped_address = 0x0001,
    /// RESPONSE-ADDRESS (0x0002) - deprecated
    response_address = 0x0002,
    /// CHANGE-REQUEST (0x0003) - RFC 3489 only
    change_request = 0x0003,
    /// SOURCE-ADDRESS (0x0004) - deprecated
    source_address = 0x0004,
    /// CHANGED-ADDRESS (0x0005) - RFC 3489 only
    changed_address = 0x0005,
    /// USERNAME (0x0006)
    username = 0x0006,
    /// PASSWORD (0x0007) - deprecated
    password = 0x0007,
    /// MESSAGE-INTEGRITY (0x0008)
    message_integrity = 0x0008,
    /// ERROR-CODE (0x0009)
    error_code = 0x0009,
    /// UNKNOWN-ATTRIBUTES (0x000A)
    unknown_attributes = 0x000A,
    /// REFLECTED-FROM (0x000B) - deprecated
    reflected_from = 0x000B,
    /// REALM (0x0014)
    realm = 0x0014,
    /// NONCE (0x0015)
    nonce = 0x0015,
    /// XOR-MAPPED-ADDRESS (0x0020)
    xor_mapped_address = 0x0020,
    /// SOFTWARE (0x8022)
    software = 0x8022,
    /// ALTERNATE-SERVER (0x8023)
    alternate_server = 0x8023,
    /// FINGERPRINT (0x8028)
    fingerprint = 0x8028
};

/**
 * @brief STUN error codes according to RFC 5389
 */
enum class stun_error_code : std::uint16_t {
    /// Try Alternate (300)
    try_alternate = 300,
    /// Bad Request (400)
    bad_request = 400,
    /// Unauthorized (401)
    unauthorized = 401,
    /// Unknown Attribute (420)
    unknown_attribute = 420,
    /// Stale Nonce (438)
    stale_nonce = 438,
    /// Server Error (500)
    server_error = 500
};

/**
 * @brief STUN client error codes
 */
enum class stun_client_error {
    /// No error occurred
    none = 0,
    /// Invalid STUN message format
    invalid_message,
    /// Unsupported STUN version
    unsupported_version,
    /// Authentication failed
    authentication_failed,
    /// Request timeout
    timeout,
    /// Network error during communication
    network_error,
    /// Server returned error response
    server_error,
    /// Invalid server endpoint
    invalid_server,
    /// Transaction ID mismatch
    transaction_mismatch,
    /// Fingerprint verification failed
    fingerprint_failed,
    /// Message integrity check failed
    integrity_check_failed,
    /// Unknown error
    unknown
};

/**
 * @brief Convert stun_client_error to string representation
 */
std::string to_string(stun_client_error error) noexcept;

/**
 * @brief Convert stun_message_type to string representation
 */
std::string to_string(stun_message_type type) noexcept;

/**
 * @brief Convert stun_error_code to string representation
 */
std::string to_string(stun_error_code code) noexcept;

/**
 * @brief STUN transaction identifier (96 bits)
 */
class stun_transaction_id {
public:
    static constexpr size_t SIZE = 12; // 96 bits / 8 = 12 bytes
    using storage_type = std::array<std::uint8_t, SIZE>;

    /**
     * @brief Default constructor - creates zero-initialized transaction ID
     */
    constexpr stun_transaction_id() noexcept : data_{} {}

    /**
     * @brief Construct from byte array
     */
    explicit constexpr stun_transaction_id(const storage_type& data) noexcept : data_(data) {}

    /**
     * @brief Get raw data access
     */
    constexpr const storage_type& data() const noexcept { return data_; }

    /**
     * @brief Get mutable raw data access
     */
    constexpr storage_type& data() noexcept { return data_; }

    /**
     * @brief Equality comparison
     */
    constexpr bool operator==(const stun_transaction_id& other) const noexcept {
        return data_ == other.data_;
    }

    /**
     * @brief Inequality comparison
     */
    constexpr bool operator!=(const stun_transaction_id& other) const noexcept {
        return !(*this == other);
    }

    /**
     * @brief Generate random transaction ID
     */
    static stun_transaction_id random();

private:
    storage_type data_;
};

/**
 * @brief STUN attribute value container
 */
struct stun_attribute {
    /// Attribute type
    stun_attribute_type type;
    /// Attribute value data
    std::vector<std::uint8_t> value;

    /**
     * @brief Constructor
     */
    stun_attribute(stun_attribute_type attr_type, std::vector<std::uint8_t> attr_value)
        : type(attr_type), value(std::move(attr_value)) {}
};

/**
 * @brief STUN message structure according to RFC 5389
 */
struct stun_message {
    /// Message type
    stun_message_type type;
    /// Transaction ID
    stun_transaction_id transaction_id;
    /// Message attributes
    std::vector<stun_attribute> attributes;
    /// Raw message data (for validation)
    std::vector<std::uint8_t> raw_data;

    /**
     * @brief Constructor
     */
    stun_message(stun_message_type msg_type, stun_transaction_id tx_id)
        : type(msg_type), transaction_id(tx_id) {}

    /**
     * @brief Find attribute by type
     * @param attr_type Attribute type to find
     * @return Pointer to attribute or nullptr if not found
     */
    const stun_attribute* find_attribute(stun_attribute_type attr_type) const noexcept;

    /**
     * @brief Add attribute to message
     * @param attr Attribute to add
     */
    void add_attribute(stun_attribute attr) {
        attributes.emplace_back(std::move(attr));
    }

    /**
     * @brief Check if message has error response
     * @return true if this is an error response
     */
    bool is_error_response() const noexcept {
        return type == stun_message_type::binding_error_response ||
               type == stun_message_type::shared_secret_error_response;
    }

    /**
     * @brief Get error code from error response
     * @return Error code or 0 if not an error response
     */
    std::uint16_t get_error_code() const noexcept;

    /**
     * @brief Get error reason phrase
     * @return Error reason string
     */
    std::string get_error_reason() const;
};

/**
 * @brief STUN binding result containing mapped address information
 */
struct stun_binding_result {
    /// Mapped (external) address as seen by STUN server
    ss::core::endpoint mapped_address;
    /// Source address that server saw request from
    std::optional<ss::core::endpoint> source_address;
    /// Changed address provided by server (for NAT detection)
    std::optional<ss::core::endpoint> changed_address;
    /// Response latency
    std::chrono::milliseconds latency{0};
    /// Server software information
    std::string server_software;
    /// Raw STUN response message
    stun_message response_message;

    /**
     * @brief Constructor
     */
    stun_binding_result(ss::core::endpoint mapped_addr, stun_message response)
        : mapped_address(std::move(mapped_addr))
        , response_message(std::move(response)) {}
};

/**
 * @brief STUN client configuration parameters
 */
struct stun_client_config {
    /// Request timeout
    std::chrono::milliseconds request_timeout{3000};
    /// Maximum retransmission attempts
    std::uint32_t max_retries = 3;
    /// Retransmission timeout (RTO)
    std::chrono::milliseconds rto{500};
    /// Enable message integrity (HMAC-SHA1)
    bool enable_integrity = false;
    /// Enable fingerprint (CRC-32)
    bool enable_fingerprint = true;
    /// Username for authentication
    std::string username;
    /// Password/key for authentication
    std::string password;
    /// Software identification string
    std::string software = "ss_p2p_ice_client/1.0";
    /// Enable RFC 3489 compatibility mode
    bool rfc3489_compat = false;
};

/**
 * @brief Callback for STUN binding completion
 */
using stun_binding_handler = std::function<void(ss::core::result<stun_binding_result, stun_client_error>)>;

/**
 * @brief Callback for keep-alive completion
 */
using stun_keepalive_handler = std::function<void(ss::core::result<void, stun_client_error>)>;

/**
 * @brief Interface for STUN client operations
 * 
 * Provides RFC 5389 compliant STUN client functionality for NAT discovery,
 * keep-alive operations, and external address resolution. Supports both
 * centralized STUN servers and distributed peer-to-peer STUN operations.
 * 
 * The client handles message encoding/decoding, transaction management,
 * retransmissions, and authentication according to STUN specifications.
 */
class i_stun_client : public ss::core::i_component {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~i_stun_client() = default;

    /**
     * @brief Perform STUN binding request to discover external address
     * @param server_endpoint STUN server endpoint
     * @param local_endpoint Local endpoint to bind from
     * @param config Client configuration
     * @return Awaitable result with binding information
     */
    virtual ss::core::async_result<ss::core::result<stun_binding_result, stun_client_error>>
    binding_request(const ss::core::endpoint& server_endpoint,
                   const ss::core::endpoint& local_endpoint,
                   const stun_client_config& config = {}) = 0;

    /**
     * @brief Perform asynchronous STUN binding request
     * @param server_endpoint STUN server endpoint
     * @param local_endpoint Local endpoint to bind from
     * @param handler Completion callback
     * @param config Client configuration
     */
    virtual void binding_request_async(const ss::core::endpoint& server_endpoint,
                                      const ss::core::endpoint& local_endpoint,
                                      stun_binding_handler handler,
                                      const stun_client_config& config = {}) = 0;

    /**
     * @brief Perform STUN binding with change request (RFC 3489 NAT detection)
     * @param server_endpoint STUN server endpoint
     * @param local_endpoint Local endpoint to bind from
     * @param change_ip Request IP address change
     * @param change_port Request port change
     * @param config Client configuration
     * @return Awaitable result with binding information
     */
    virtual ss::core::async_result<ss::core::result<stun_binding_result, stun_client_error>>
    binding_request_with_change(const ss::core::endpoint& server_endpoint,
                               const ss::core::endpoint& local_endpoint,
                               bool change_ip, bool change_port,
                               const stun_client_config& config = {}) = 0;

    /**
     * @brief Send keep-alive binding request to maintain NAT mapping
     * @param server_endpoint STUN server endpoint
     * @param local_endpoint Local endpoint to use
     * @param config Client configuration
     * @return Awaitable result indicating success or failure
     */
    virtual ss::core::async_result<ss::core::result<void, stun_client_error>>
    send_keepalive(const ss::core::endpoint& server_endpoint,
                  const ss::core::endpoint& local_endpoint,
                  const stun_client_config& config = {}) = 0;

    /**
     * @brief Send asynchronous keep-alive
     * @param server_endpoint STUN server endpoint
     * @param local_endpoint Local endpoint to use
     * @param handler Completion callback
     * @param config Client configuration
     */
    virtual void send_keepalive_async(const ss::core::endpoint& server_endpoint,
                                     const ss::core::endpoint& local_endpoint,
                                     stun_keepalive_handler handler,
                                     const stun_client_config& config = {}) = 0;

    /**
     * @brief Parse raw STUN message from network data
     * @param data Raw message bytes
     * @param sender Sender endpoint
     * @return Parsed STUN message or error
     */
    virtual ss::core::result<stun_message, stun_client_error>
    parse_message(const std::vector<std::uint8_t>& data,
                 const ss::core::endpoint& sender) const = 0;

    /**
     * @brief Encode STUN message to network format
     * @param message STUN message to encode
     * @param config Configuration for encoding (integrity, fingerprint)
     * @return Encoded message bytes or error
     */
    virtual ss::core::result<std::vector<std::uint8_t>, stun_client_error>
    encode_message(const stun_message& message,
                  const stun_client_config& config = {}) const = 0;

    /**
     * @brief Verify message integrity (HMAC-SHA1)
     * @param message STUN message to verify
     * @param key Shared secret key
     * @return true if integrity check passes
     */
    virtual bool verify_message_integrity(const stun_message& message,
                                         const std::string& key) const = 0;

    /**
     * @brief Verify message fingerprint (CRC-32)
     * @param message STUN message to verify
     * @return true if fingerprint is valid
     */
    virtual bool verify_fingerprint(const stun_message& message) const = 0;

    /**
     * @brief Create XOR-MAPPED-ADDRESS attribute from endpoint
     * @param endpoint Endpoint to encode
     * @param transaction_id Transaction ID for XOR operation
     * @return Encoded attribute data
     */
    virtual std::vector<std::uint8_t> 
    create_xor_mapped_address(const ss::core::endpoint& endpoint,
                             const stun_transaction_id& transaction_id) const = 0;

    /**
     * @brief Parse XOR-MAPPED-ADDRESS attribute to endpoint
     * @param attribute_data Attribute value data
     * @param transaction_id Transaction ID for XOR operation
     * @return Decoded endpoint or error
     */
    virtual ss::core::result<ss::core::endpoint, stun_client_error>
    parse_xor_mapped_address(const std::vector<std::uint8_t>& attribute_data,
                            const stun_transaction_id& transaction_id) const = 0;

    /**
     * @brief Statistics for STUN operations
     */
    struct stun_client_stats {
        std::uint64_t requests_sent = 0;
        std::uint64_t responses_received = 0;
        std::uint64_t error_responses = 0;
        std::uint64_t timeouts = 0;
        std::uint64_t retransmissions = 0;
        std::uint64_t integrity_failures = 0;
        std::uint64_t fingerprint_failures = 0;
        std::chrono::milliseconds avg_response_time{0};
        std::chrono::steady_clock::time_point last_request_time{};
        std::chrono::steady_clock::time_point last_response_time{};
    };
    
    /**
     * @brief Get statistics for STUN operations
     * @return Current statistics
     */
    virtual stun_client_stats get_stats() const noexcept = 0;

    /**
     * @brief Reset client statistics
     */
    virtual void reset_stats() noexcept = 0;

    /**
     * @brief Set transport for STUN operations
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
    i_stun_client() = default;

    /**
     * @brief Protected copy constructor (deleted)
     */
    i_stun_client(const i_stun_client&) = delete;

    /**
     * @brief Protected move constructor (deleted)
     */
    i_stun_client(i_stun_client&&) = delete;

    /**
     * @brief Protected copy assignment (deleted)
     */
    i_stun_client& operator=(const i_stun_client&) = delete;

    /**
     * @brief Protected move assignment (deleted)
     */
    i_stun_client& operator=(i_stun_client&&) = delete;
};

/**
 * @brief Smart pointer type for STUN client instances
 */
using stun_client_ptr = std::shared_ptr<i_stun_client>;

/**
 * @brief Weak pointer type for STUN client instances
 */
using stun_client_weak_ptr = std::weak_ptr<i_stun_client>;

} // namespace ss::ice

/**
 * @brief Hash specialization for stun_transaction_id
 */
template<>
struct std::hash<ss::ice::stun_transaction_id> {
    std::size_t operator()(const ss::ice::stun_transaction_id& id) const noexcept {
        const auto& data = id.data();
        std::size_t hash = 0;
        for (size_t i = 0; i < ss::ice::stun_transaction_id::SIZE; i += sizeof(std::size_t)) {
            std::size_t chunk = 0;
            std::memcpy(&chunk, &data[i], 
                       std::min(sizeof(std::size_t), ss::ice::stun_transaction_id::SIZE - i));
            hash ^= std::hash<std::size_t>{}(chunk) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
        return hash;
    }
};