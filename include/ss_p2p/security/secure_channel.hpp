#pragma once

#include "../core/types.hpp"
#include "../core/result.hpp"
#include "../core/interfaces.hpp"
#include "../network/i_transport.hpp"
#include "i_crypto.hpp"
#include "i_auth.hpp"
#include "i_key_manager.hpp"

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <optional>
#include <unordered_map>
#include <functional>
#include <atomic>

namespace ss::security {

/**
 * @brief Secure channel error codes
 */
enum class channel_error {
    /// No error occurred
    none = 0,
    /// Channel handshake failed
    handshake_failed,
    /// Authentication failed during handshake
    authentication_failed,
    /// Key exchange failed
    key_exchange_failed,
    /// Invalid message format or sequence
    invalid_message,
    /// Message replay detected
    replay_attack,
    /// Message out of sequence
    sequence_error,
    /// Encryption/decryption failed
    crypto_error,
    /// Channel is not established
    not_established,
    /// Channel has expired
    channel_expired,
    /// Rate limit exceeded
    rate_limit_exceeded,
    /// Peer is untrusted
    untrusted_peer,
    /// Protocol version mismatch
    version_mismatch,
    /// Network transport error
    transport_error,
    /// Unknown secure channel error
    unknown
};

/**
 * @brief Convert channel_error to string representation
 * @param error Error code to convert
 * @return String description of the error
 */
std::string to_string(channel_error error) noexcept;

/**
 * @brief Secure channel protocol version
 */
enum class protocol_version : std::uint16_t {
    /// Version 1.0
    v1_0 = 0x0100,
    /// Version 1.1 (current)
    v1_1 = 0x0101
};

/**
 * @brief Channel security level
 */
enum class channel_security_level {
    /// Basic security (encryption only)
    basic = 0,
    /// Standard security (encryption + authentication)
    standard = 1,
    /// High security (perfect forward secrecy)
    high = 2,
    /// Maximum security (post-quantum ready)
    maximum = 3
};

/**
 * @brief Handshake state
 */
enum class handshake_state {
    /// Not started
    not_started,
    /// Initiated by local peer
    initiated,
    /// Response received/sent
    response_exchanged,
    /// Key exchange in progress
    key_exchange,
    /// Authentication in progress
    authenticating,
    /// Handshake completed successfully
    completed,
    /// Handshake failed
    failed
};

/**
 * @brief Forward secrecy mode
 */
enum class forward_secrecy_mode {
    /// No forward secrecy
    none,
    /// Ephemeral keys for session
    ephemeral,
    /// Perfect forward secrecy with key rotation
    perfect
};

/**
 * @brief Secure channel configuration
 */
struct channel_config {
    /// Protocol version to use
    protocol_version version = protocol_version::v1_1;
    /// Security level requirement
    channel_security_level security_level = channel_security_level::standard;
    /// Forward secrecy mode
    forward_secrecy_mode fs_mode = forward_secrecy_mode::ephemeral;
    /// Handshake timeout
    std::chrono::milliseconds handshake_timeout{30000};
    /// Channel lifetime
    std::chrono::hours channel_lifetime{24};
    /// Key rotation interval
    std::chrono::minutes key_rotation_interval{60};
    /// Maximum message size
    std::uint32_t max_message_size = 1024 * 1024; // 1MB
    /// Enable replay protection
    bool replay_protection = true;
    /// Maximum allowed clock skew
    std::chrono::seconds max_clock_skew{300}; // 5 minutes
    /// Compression enabled
    bool compression = false;
    /// Additional protocol parameters
    std::unordered_map<std::string, std::string> parameters;
};

/**
 * @brief Channel statistics
 */
struct channel_stats {
    /// Channel establishment timestamp
    std::chrono::system_clock::time_point established_at;
    /// Last activity timestamp
    std::chrono::system_clock::time_point last_activity;
    /// Total bytes sent
    std::uint64_t bytes_sent = 0;
    /// Total bytes received
    std::uint64_t bytes_received = 0;
    /// Total messages sent
    std::uint64_t messages_sent = 0;
    /// Total messages received
    std::uint64_t messages_received = 0;
    /// Number of key rotations performed
    std::uint32_t key_rotations = 0;
    /// Number of replay attacks detected
    std::uint32_t replay_attacks_detected = 0;
    /// Average encryption latency in microseconds
    std::uint64_t avg_encryption_latency_us = 0;
    /// Average decryption latency in microseconds
    std::uint64_t avg_decryption_latency_us = 0;
};

/**
 * @brief Handshake message types
 */
enum class handshake_message_type : std::uint8_t {
    /// Initial handshake request
    client_hello = 1,
    /// Server response to client hello
    server_hello = 2,
    /// Key exchange message
    key_exchange = 3,
    /// Authentication challenge
    auth_challenge = 4,
    /// Authentication response
    auth_response = 5,
    /// Handshake finished
    finished = 6,
    /// Handshake error
    error = 255
};

/**
 * @brief Encrypted message envelope
 */
struct encrypted_message {
    /// Protocol version
    protocol_version version;
    /// Message sequence number
    std::uint64_t sequence;
    /// Timestamp when message was created
    std::chrono::system_clock::time_point timestamp;
    /// Encrypted payload
    std::vector<std::uint8_t> ciphertext;
    /// Authentication tag
    std::vector<std::uint8_t> tag;
    /// Initialization vector
    std::vector<std::uint8_t> iv;
    /// Additional authenticated data
    std::vector<std::uint8_t> aad;
    /// Message type hint
    std::uint8_t message_type = 0;
};

/**
 * @brief Key rotation context
 */
struct key_rotation_context {
    /// Current key generation
    std::uint32_t current_generation;
    /// Next key generation
    std::uint32_t next_generation;
    /// Key rotation timestamp
    std::chrono::system_clock::time_point rotation_time;
    /// Old key retention period
    std::chrono::minutes retention_period{10};
};

/**
 * @brief Secure channel event callbacks
 */
using channel_established_callback = std::function<void(const ss::core::endpoint&)>;
using channel_closed_callback = std::function<void(const ss::core::endpoint&, channel_error)>;
using message_received_callback = std::function<void(const ss::core::endpoint&, const std::vector<std::uint8_t>&)>;
using key_rotation_callback = std::function<void(const ss::core::endpoint&, std::uint32_t new_generation)>;

/**
 * @brief Secure communication channel interface
 * 
 * Provides end-to-end encrypted communication with perfect forward secrecy,
 * replay protection, and mutual authentication. Built on top of the transport
 * layer to provide secure messaging between authenticated peers.
 * 
 * Features:
 * - Double Ratchet protocol for perfect forward secrecy
 * - Replay attack prevention with sequence numbers
 * - Automatic key rotation
 * - Mutual authentication
 * - Message integrity protection
 * - Timing attack resistance
 * 
 * Thread safety: All operations must be thread-safe for concurrent access.
 */
class secure_channel : public ss::core::i_component {
public:
    /**
     * @brief Constructor
     * @param transport Network transport layer
     * @param crypto Cryptography provider
     * @param auth Authentication provider
     * @param key_manager Key management provider
     * @param config Channel configuration
     */
    secure_channel(
        ss::network::transport_ptr transport,
        crypto_ptr crypto,
        auth_ptr auth,
        key_manager_ptr key_manager,
        channel_config config = {});

    /**
     * @brief Virtual destructor
     */
    virtual ~secure_channel();

    // === Channel Establishment ===

    /**
     * @brief Establish secure channel with peer (as client)
     * @param peer_endpoint Remote peer endpoint
     * @param timeout Handshake timeout
     * @return Success or error
     */
    ss::core::async_result<ss::core::result<void, channel_error>>
    connect(
        const ss::core::endpoint& peer_endpoint,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{30000});

    /**
     * @brief Accept incoming secure channel connection (as server)
     * @param client_endpoint Client endpoint
     * @param timeout Accept timeout
     * @return Success or error
     */
    ss::core::async_result<ss::core::result<void, channel_error>>
    accept(
        const ss::core::endpoint& client_endpoint,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{30000});

    /**
     * @brief Close secure channel
     * @param peer_endpoint Peer to close channel with
     * @return Success or error
     */
    ss::core::async_result<ss::core::result<void, channel_error>>
    close(const ss::core::endpoint& peer_endpoint);

    /**
     * @brief Check if channel is established with peer
     * @param peer_endpoint Peer endpoint
     * @return true if channel is active
     */
    bool is_established(const ss::core::endpoint& peer_endpoint) const noexcept;

    /**
     * @brief Get handshake state for peer
     * @param peer_endpoint Peer endpoint
     * @return Current handshake state
     */
    handshake_state get_handshake_state(const ss::core::endpoint& peer_endpoint) const noexcept;

    // === Secure Messaging ===

    /**
     * @brief Send encrypted message to peer
     * @param peer_endpoint Target peer
     * @param message Message data to send
     * @param priority Message priority (higher = more urgent)
     * @return Success or error
     */
    ss::core::async_result<ss::core::result<void, channel_error>>
    send_message(
        const ss::core::endpoint& peer_endpoint,
        const std::vector<std::uint8_t>& message,
        std::uint32_t priority = 0);

    /**
     * @brief Send encrypted message with completion callback
     * @param peer_endpoint Target peer
     * @param message Message data to send
     * @param callback Completion callback
     */
    void send_message_async(
        const ss::core::endpoint& peer_endpoint,
        const std::vector<std::uint8_t>& message,
        std::function<void(ss::core::result<void, channel_error>)> callback);

    /**
     * @brief Receive next available message
     * @param timeout Receive timeout
     * @return Message with sender endpoint or error
     */
    ss::core::async_result<ss::core::result<
        std::pair<ss::core::endpoint, std::vector<std::uint8_t>>, channel_error>>
    receive_message(std::chrono::milliseconds timeout = std::chrono::milliseconds{5000});

    // === Key Management ===

    /**
     * @brief Manually trigger key rotation for peer
     * @param peer_endpoint Peer to rotate keys with
     * @return New key generation or error
     */
    ss::core::async_result<ss::core::result<std::uint32_t, channel_error>>
    rotate_keys(const ss::core::endpoint& peer_endpoint);

    /**
     * @brief Get current key generation for peer
     * @param peer_endpoint Peer endpoint
     * @return Current key generation
     */
    std::uint32_t get_key_generation(const ss::core::endpoint& peer_endpoint) const noexcept;

    /**
     * @brief Check if peer needs key rotation
     * @param peer_endpoint Peer endpoint
     * @return true if rotation is needed
     */
    bool needs_key_rotation(const ss::core::endpoint& peer_endpoint) const noexcept;

    // === Channel Management ===

    /**
     * @brief Get active peer endpoints
     * @return Vector of endpoints with established channels
     */
    std::vector<ss::core::endpoint> get_active_peers() const;

    /**
     * @brief Get channel statistics for peer
     * @param peer_endpoint Peer endpoint
     * @return Channel statistics or nullopt if no channel
     */
    std::optional<channel_stats> get_channel_stats(const ss::core::endpoint& peer_endpoint) const;

    /**
     * @brief Set channel configuration
     * @param config New configuration
     * @return Success or error
     */
    ss::core::result<void, channel_error> set_config(const channel_config& config);

    /**
     * @brief Get current channel configuration
     * @return Current configuration
     */
    const channel_config& get_config() const noexcept;

    // === Event Callbacks ===

    /**
     * @brief Set channel establishment callback
     * @param callback Callback for when channels are established
     */
    void set_channel_established_callback(channel_established_callback callback);

    /**
     * @brief Set channel closed callback
     * @param callback Callback for when channels are closed
     */
    void set_channel_closed_callback(channel_closed_callback callback);

    /**
     * @brief Set message received callback
     * @param callback Callback for incoming messages
     */
    void set_message_received_callback(message_received_callback callback);

    /**
     * @brief Set key rotation callback
     * @param callback Callback for key rotation events
     */
    void set_key_rotation_callback(key_rotation_callback callback);

    // === Component Interface ===

    /**
     * @brief Start the secure channel component
     * @return Success or error
     */
    ss::core::async_result<ss::core::result<void, std::error_code>> start() override;

    /**
     * @brief Stop the secure channel component
     * @return Success or error
     */
    ss::core::async_result<ss::core::result<void, std::error_code>> stop() override;

    /**
     * @brief Check if component is running
     * @return true if running
     */
    bool is_running() const noexcept override;

    /**
     * @brief Get component name
     * @return Component name
     */
    std::string name() const noexcept override;

private:
    // Forward declaration of implementation
    class impl;
    std::unique_ptr<impl> pimpl_;

    // === Private Helper Methods ===

    /**
     * @brief Handle incoming handshake message
     */
    ss::core::async_void handle_handshake_message(
        const ss::core::endpoint& sender,
        handshake_message_type type,
        const std::vector<std::uint8_t>& data);

    /**
     * @brief Handle incoming encrypted message
     */
    ss::core::async_void handle_encrypted_message(
        const ss::core::endpoint& sender,
        const encrypted_message& msg);

    /**
     * @brief Perform client handshake
     */
    ss::core::async_result<ss::core::result<void, channel_error>>
    perform_client_handshake(
        const ss::core::endpoint& server_endpoint,
        std::chrono::milliseconds timeout);

    /**
     * @brief Perform server handshake
     */
    ss::core::async_result<ss::core::result<void, channel_error>>
    perform_server_handshake(
        const ss::core::endpoint& client_endpoint,
        std::chrono::milliseconds timeout);

    /**
     * @brief Encrypt message for peer
     */
    ss::core::result<encrypted_message, channel_error>
    encrypt_message(
        const ss::core::endpoint& peer_endpoint,
        const std::vector<std::uint8_t>& plaintext);

    /**
     * @brief Decrypt message from peer
     */
    ss::core::result<std::vector<std::uint8_t>, channel_error>
    decrypt_message(
        const ss::core::endpoint& peer_endpoint,
        const encrypted_message& msg);

    /**
     * @brief Validate message sequence and timestamp
     */
    bool validate_message_sequence(
        const ss::core::endpoint& peer_endpoint,
        const encrypted_message& msg) const;

    /**
     * @brief Background task for key rotation
     */
    ss::core::async_void key_rotation_task();

    /**
     * @brief Background task for channel cleanup
     */
    ss::core::async_void cleanup_task();
};

/**
 * @brief Smart pointer type for secure channel instances
 */
using secure_channel_ptr = std::shared_ptr<secure_channel>;

/**
 * @brief Weak pointer type for secure channel instances
 */
using secure_channel_weak_ptr = std::weak_ptr<secure_channel>;

/**
 * @brief Factory function for creating secure channels
 * @param transport Network transport layer
 * @param crypto Cryptography provider
 * @param auth Authentication provider
 * @param key_manager Key management provider
 * @param config Channel configuration
 * @return Secure channel instance
 */
secure_channel_ptr create_secure_channel(
    ss::network::transport_ptr transport,
    crypto_ptr crypto,
    auth_ptr auth,
    key_manager_ptr key_manager,
    channel_config config = {});

} // namespace ss::security