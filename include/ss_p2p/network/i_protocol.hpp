#pragma once

#include "../core/types.hpp"
#include "../core/result.hpp"
#include "../core/interfaces.hpp"

#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <type_traits>

namespace ss::network {

/**
 * @brief Protocol version information
 */
struct protocol_version {
    /// Major version number
    std::uint16_t major = 1;
    /// Minor version number  
    std::uint16_t minor = 0;
    /// Patch version number
    std::uint16_t patch = 0;

    /**
     * @brief Default constructor
     */
    constexpr protocol_version() = default;

    /**
     * @brief Constructor with version components
     * @param maj Major version
     * @param min Minor version  
     * @param pat Patch version
     */
    constexpr protocol_version(std::uint16_t maj, std::uint16_t min, std::uint16_t pat) noexcept
        : major(maj), minor(min), patch(pat) {}

    /**
     * @brief Equality comparison
     */
    constexpr bool operator==(const protocol_version& other) const noexcept {
        return major == other.major && minor == other.minor && patch == other.patch;
    }

    /**
     * @brief Inequality comparison
     */
    constexpr bool operator!=(const protocol_version& other) const noexcept {
        return !(*this == other);
    }

    /**
     * @brief Less-than comparison for ordering
     */
    constexpr bool operator<(const protocol_version& other) const noexcept {
        if (major != other.major) return major < other.major;
        if (minor != other.minor) return minor < other.minor;
        return patch < other.patch;
    }

    /**
     * @brief Check compatibility with another version
     * @param other Other version to check
     * @return true if versions are compatible
     */
    constexpr bool is_compatible(const protocol_version& other) const noexcept {
        // Compatible if major versions match and this version >= other
        return major == other.major && !(*this < other);
    }

    /**
     * @brief Convert to string representation
     * @return Version string in format "major.minor.patch"
     */
    std::string to_string() const;

    /**
     * @brief Parse version from string
     * @param version_str Version string in format "major.minor.patch"
     * @return Parsed version or error
     */
    static ss::core::result<protocol_version, std::string> from_string(const std::string& version_str);
};

/**
 * @brief Protocol error codes
 */
enum class protocol_error {
    /// No error occurred
    none = 0,
    /// Invalid message format
    invalid_format,
    /// Unsupported protocol version
    unsupported_version,
    /// Message too large for protocol
    message_too_large,
    /// Message too small for protocol
    message_too_small,
    /// Invalid message type
    invalid_message_type,
    /// Missing required fields
    missing_required_fields,
    /// Invalid field value
    invalid_field_value,
    /// Checksum verification failed
    checksum_failure,
    /// Compression/decompression error
    compression_error,
    /// Encryption/decryption error
    encryption_error,
    /// Protocol state violation
    protocol_state_error,
    /// Unknown error
    unknown
};

/**
 * @brief Convert protocol_error to string representation
 * @param error Error code to convert
 * @return String description of the error
 */
std::string to_string(protocol_error error) noexcept;

/**
 * @brief Protocol configuration parameters
 */
struct protocol_config {
    /// Protocol version to use
    protocol_version version{1, 0, 0};
    /// Enable message compression
    bool enable_compression = false;
    /// Compression level (0-9, higher = better compression)
    std::uint8_t compression_level = 6;
    /// Enable message encryption
    bool enable_encryption = false;
    /// Enable message authentication codes (MAC)
    bool enable_authentication = false;
    /// Maximum message size in bytes
    std::uint32_t max_message_size = 1024 * 1024;  // 1MB
    /// Enable message fragmentation for large messages
    bool enable_fragmentation = true;
    /// Fragment size for large messages
    std::uint32_t fragment_size = 65536;  // 64KB
    /// Enable message ordering guarantees
    bool enable_ordering = false;
    /// Enable delivery acknowledgments
    bool enable_acknowledgments = false;
    /// Acknowledgment timeout
    std::chrono::milliseconds ack_timeout{5000};
    /// Maximum retransmission attempts
    std::uint8_t max_retransmits = 3;
};

/**
 * @brief Protocol statistics for monitoring
 */
struct protocol_stats {
    /// Total messages encoded
    std::uint64_t messages_encoded = 0;
    /// Total messages decoded
    std::uint64_t messages_decoded = 0;
    /// Total encoding errors
    std::uint64_t encoding_errors = 0;
    /// Total decoding errors
    std::uint64_t decoding_errors = 0;
    /// Total bytes encoded
    std::uint64_t bytes_encoded = 0;
    /// Total bytes decoded
    std::uint64_t bytes_decoded = 0;
    /// Compression ratio (compressed_size / original_size)
    double compression_ratio = 1.0;
    /// Average encoding time in microseconds
    std::uint64_t avg_encoding_time_us = 0;
    /// Average decoding time in microseconds
    std::uint64_t avg_decoding_time_us = 0;
    /// Number of fragmented messages sent
    std::uint64_t fragmented_sent = 0;
    /// Number of fragmented messages received
    std::uint64_t fragmented_received = 0;
    /// Number of messages retransmitted
    std::uint64_t retransmissions = 0;
};

/**
 * @brief Message metadata attached during protocol processing
 */
struct message_metadata {
    /// Message type identifier
    std::uint32_t type_id = 0;
    /// Message sequence number
    std::uint64_t sequence_number = 0;
    /// Fragment information (if fragmented)
    struct fragment_info {
        std::uint32_t fragment_id = 0;
        std::uint32_t total_fragments = 1;
        std::uint32_t fragment_offset = 0;
    } fragment;
    /// Timestamp when message was created
    std::chrono::steady_clock::time_point created_at;
    /// Protocol-specific flags
    std::uint32_t flags = 0;
    /// Checksum/hash of message content
    std::uint32_t checksum = 0;
    /// Custom attributes
    std::unordered_map<std::string, std::string> attributes;

    /**
     * @brief Default constructor
     */
    message_metadata() : created_at(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Encoded message with protocol headers and metadata
 */
struct encoded_message {
    /// Complete encoded message data (headers + payload)
    std::vector<std::uint8_t> data;
    /// Message metadata
    message_metadata metadata;
    /// Original payload size before encoding
    std::size_t original_size = 0;

    /**
     * @brief Constructor
     * @param encoded_data Encoded message data
     * @param meta Message metadata
     */
    encoded_message(std::vector<std::uint8_t> encoded_data, message_metadata meta)
        : data(std::move(encoded_data))
        , metadata(std::move(meta))
        , original_size(data.size()) {}
};

/**
 * @brief Decoded message with extracted payload and metadata
 */
struct decoded_message {
    /// Decoded payload data
    std::vector<std::uint8_t> payload;
    /// Message metadata
    message_metadata metadata;
    /// Encoded message size
    std::size_t encoded_size = 0;

    /**
     * @brief Constructor
     * @param payload_data Decoded payload
     * @param meta Message metadata
     */
    decoded_message(std::vector<std::uint8_t> payload_data, message_metadata meta)
        : payload(std::move(payload_data))
        , metadata(std::move(meta))
        , encoded_size(payload.size()) {}
};

/**
 * @brief Interface for protocol layer message processing
 * 
 * Provides abstraction for encoding/decoding messages with protocol-specific
 * features like versioning, compression, encryption, fragmentation, and reliability.
 * 
 * Protocol implementations handle the wire format and ensure compatibility
 * across different versions and features.
 */
class i_protocol : public ss::core::i_component {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~i_protocol() = default;

    /**
     * @brief Encode message for transmission
     * @param payload Raw message payload
     * @param type_id Message type identifier
     * @param metadata Optional metadata to include
     * @return Encoded message or error
     */
    virtual ss::core::result<encoded_message, protocol_error> 
    encode(const std::vector<std::uint8_t>& payload, 
           std::uint32_t type_id,
           std::optional<message_metadata> metadata = std::nullopt) = 0;

    /**
     * @brief Decode received message
     * @param data Encoded message data
     * @return Decoded message or error
     */
    virtual ss::core::result<decoded_message, protocol_error>
    decode(const std::vector<std::uint8_t>& data) = 0;

    /**
     * @brief Encode message asynchronously
     * @param payload Raw message payload
     * @param type_id Message type identifier
     * @param metadata Optional metadata to include
     * @return Awaitable encoded message or error
     */
    virtual ss::core::async_result<ss::core::result<encoded_message, protocol_error>>
    encode_async(const std::vector<std::uint8_t>& payload,
                 std::uint32_t type_id,
                 std::optional<message_metadata> metadata = std::nullopt) = 0;

    /**
     * @brief Decode message asynchronously
     * @param data Encoded message data
     * @return Awaitable decoded message or error
     */
    virtual ss::core::async_result<ss::core::result<decoded_message, protocol_error>>
    decode_async(const std::vector<std::uint8_t>& data) = 0;

    /**
     * @brief Validate message format without full decoding
     * @param data Message data to validate
     * @return true if format is valid, false otherwise
     */
    virtual bool validate_format(const std::vector<std::uint8_t>& data) const noexcept = 0;

    /**
     * @brief Get protocol version being used
     * @return Current protocol version
     */
    virtual protocol_version get_version() const noexcept = 0;

    /**
     * @brief Check if protocol supports a specific version
     * @param version Version to check
     * @return true if supported, false otherwise
     */
    virtual bool supports_version(const protocol_version& version) const noexcept = 0;

    /**
     * @brief Get maximum message size supported
     * @return Maximum payload size in bytes
     */
    virtual std::uint32_t max_message_size() const noexcept = 0;

    /**
     * @brief Get minimum message size required
     * @return Minimum payload size in bytes
     */
    virtual std::uint32_t min_message_size() const noexcept = 0;

    /**
     * @brief Get protocol overhead size
     * @return Number of bytes added by protocol headers
     */
    virtual std::uint32_t overhead_size() const noexcept = 0;

    /**
     * @brief Get current protocol configuration
     * @return Current configuration
     */
    virtual protocol_config get_config() const noexcept = 0;

    /**
     * @brief Update protocol configuration
     * @param config New configuration
     * @return Result indicating success or error
     */
    virtual ss::core::result<void, protocol_error> set_config(const protocol_config& config) = 0;

    /**
     * @brief Get current protocol statistics
     * @return Current statistics snapshot
     */
    virtual protocol_stats get_stats() const noexcept = 0;

    /**
     * @brief Reset protocol statistics
     */
    virtual void reset_stats() noexcept = 0;

    /**
     * @brief Check if protocol supports message fragmentation
     * @return true if fragmentation is supported
     */
    virtual bool supports_fragmentation() const noexcept = 0;

    /**
     * @brief Check if protocol supports message compression
     * @return true if compression is supported
     */
    virtual bool supports_compression() const noexcept = 0;

    /**
     * @brief Check if protocol supports message encryption
     * @return true if encryption is supported
     */
    virtual bool supports_encryption() const noexcept = 0;

    /**
     * @brief Check if protocol supports message ordering
     * @return true if ordering is supported
     */
    virtual bool supports_ordering() const noexcept = 0;

    /**
     * @brief Check if protocol supports delivery acknowledgments
     * @return true if acknowledgments are supported
     */
    virtual bool supports_acknowledgments() const noexcept = 0;

    /**
     * @brief Handle fragmented message assembly
     * @param fragment Fragment to process
     * @return Complete message if assembly finished, nullopt if more fragments needed
     */
    virtual std::optional<decoded_message> handle_fragment(const decoded_message& fragment) = 0;

    /**
     * @brief Generate acknowledgment message for received message
     * @param message Message to acknowledge
     * @return Acknowledgment message data
     */
    virtual ss::core::result<std::vector<std::uint8_t>, protocol_error>
    generate_acknowledgment(const decoded_message& message) = 0;

    /**
     * @brief Process acknowledgment message
     * @param ack_data Acknowledgment message data
     * @return Result indicating success or error
     */
    virtual ss::core::result<void, protocol_error>
    process_acknowledgment(const std::vector<std::uint8_t>& ack_data) = 0;

protected:
    /**
     * @brief Protected default constructor
     */
    i_protocol() = default;

    /**
     * @brief Protected copy constructor (deleted)
     */
    i_protocol(const i_protocol&) = delete;

    /**
     * @brief Protected move constructor (deleted)
     */
    i_protocol(i_protocol&&) = delete;

    /**
     * @brief Protected copy assignment (deleted)
     */
    i_protocol& operator=(const i_protocol&) = delete;

    /**
     * @brief Protected move assignment (deleted)
     */
    i_protocol& operator=(i_protocol&&) = delete;
};

/**
 * @brief Smart pointer type for protocol instances
 */
using protocol_ptr = std::shared_ptr<i_protocol>;

/**
 * @brief Weak pointer type for protocol instances
 */
using protocol_weak_ptr = std::weak_ptr<i_protocol>;

/**
 * @brief Message type registry for protocol implementations
 * 
 * Allows registration of custom message types with their serialization logic.
 */
template<typename MessageType>
class message_type_registry {
public:
    /// Message serializer function type
    using serializer_func = std::function<std::vector<std::uint8_t>(const MessageType&)>;
    /// Message deserializer function type
    using deserializer_func = std::function<ss::core::result<MessageType, std::string>(const std::vector<std::uint8_t>&)>;

    /**
     * @brief Register a message type
     * @param type_id Unique type identifier
     * @param serializer Serialization function
     * @param deserializer Deserialization function
     */
    void register_type(std::uint32_t type_id, 
                      serializer_func serializer,
                      deserializer_func deserializer) {
        serializers_[type_id] = std::move(serializer);
        deserializers_[type_id] = std::move(deserializer);
    }

    /**
     * @brief Unregister a message type
     * @param type_id Type identifier to remove
     */
    void unregister_type(std::uint32_t type_id) {
        serializers_.erase(type_id);
        deserializers_.erase(type_id);
    }

    /**
     * @brief Check if type is registered
     * @param type_id Type identifier to check
     * @return true if registered, false otherwise
     */
    bool is_registered(std::uint32_t type_id) const {
        return serializers_.find(type_id) != serializers_.end();
    }

    /**
     * @brief Serialize message of registered type
     * @param type_id Message type identifier
     * @param message Message to serialize
     * @return Serialized data or error
     */
    ss::core::result<std::vector<std::uint8_t>, std::string>
    serialize(std::uint32_t type_id, const MessageType& message) const {
        auto it = serializers_.find(type_id);
        if (it == serializers_.end()) {
            return ss::core::result<std::vector<std::uint8_t>, std::string>::err("Type not registered");
        }
        
        try {
            return ss::core::result<std::vector<std::uint8_t>, std::string>::ok(it->second(message));
        } catch (const std::exception& e) {
            return ss::core::result<std::vector<std::uint8_t>, std::string>::err(e.what());
        }
    }

    /**
     * @brief Deserialize message of registered type
     * @param type_id Message type identifier
     * @param data Serialized data
     * @return Deserialized message or error
     */
    ss::core::result<MessageType, std::string>
    deserialize(std::uint32_t type_id, const std::vector<std::uint8_t>& data) const {
        auto it = deserializers_.find(type_id);
        if (it == deserializers_.end()) {
            return ss::core::result<MessageType, std::string>::err("Type not registered");
        }
        
        return it->second(data);
    }

private:
    std::unordered_map<std::uint32_t, serializer_func> serializers_;
    std::unordered_map<std::uint32_t, deserializer_func> deserializers_;
};

} // namespace ss::network