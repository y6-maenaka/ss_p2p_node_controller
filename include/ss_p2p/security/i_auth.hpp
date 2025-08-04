#pragma once

#include "../core/types.hpp"
#include "../core/result.hpp"
#include "../core/interfaces.hpp"
#include "i_crypto.hpp"

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <optional>
#include <unordered_map>
#include <functional>

namespace ss::security {

/**
 * @brief Authentication error codes
 */
enum class auth_error {
    /// No error occurred
    none = 0,
    /// Authentication failed - invalid credentials
    authentication_failed,
    /// Authorization failed - insufficient permissions
    authorization_failed,
    /// Invalid certificate or certificate chain
    invalid_certificate,
    /// Certificate has expired
    certificate_expired,
    /// Certificate has been revoked
    certificate_revoked,
    /// Certificate is not yet valid
    certificate_not_yet_valid,
    /// Certificate chain verification failed
    chain_verification_failed,
    /// Trust anchor not found
    trust_anchor_not_found,
    /// Invalid signature on certificate or message
    invalid_signature,
    /// Nonce replay attack detected
    replay_attack,
    /// Challenge-response authentication failed
    challenge_failed,
    /// Session has expired
    session_expired,
    /// Session not found
    session_not_found,
    /// Rate limit exceeded
    rate_limit_exceeded,
    /// Peer is blacklisted
    peer_blacklisted,
    /// Authentication timeout
    timeout,
    /// Insufficient entropy for secure operation
    insufficient_entropy,
    /// Unknown authentication error
    unknown
};

/**
 * @brief Convert auth_error to string representation
 * @param error Error code to convert
 * @return String description of the error
 */
std::string to_string(auth_error error) noexcept;

/**
 * @brief Authentication method types
 */
enum class auth_method {
    /// Challenge-response authentication
    challenge_response,
    /// Public key authentication
    public_key,
    /// Certificate-based authentication
    certificate,
    /// Pre-shared key authentication
    preshared_key,
    /// Web of Trust authentication
    web_of_trust
};

/**
 * @brief Certificate types
 */
enum class certificate_type {
    /// Self-signed certificate
    self_signed,
    /// Certificate authority signed
    ca_signed,
    /// Peer-to-peer certificate (web of trust)
    p2p_certificate
};

/**
 * @brief Certificate status
 */
enum class certificate_status {
    /// Certificate is valid and trusted
    valid,
    /// Certificate has expired
    expired,
    /// Certificate has been revoked
    revoked,
    /// Certificate is not yet valid
    not_yet_valid,
    /// Certificate is untrusted
    untrusted,
    /// Certificate validation is pending
    pending,
    /// Certificate status is unknown
    unknown
};

/**
 * @brief Authentication level
 */
enum class auth_level {
    /// No authentication
    none = 0,
    /// Basic authentication (identity verified)
    basic = 1,
    /// Strong authentication (cryptographic proof)
    strong = 2,
    /// Mutual authentication (both peers authenticated)
    mutual = 3
};

/**
 * @brief Peer identity information
 */
struct peer_identity {
    /// Unique peer identifier
    ss::core::peer_id peer_id;
    /// Public key for this peer
    crypto_key public_key;
    /// Optional human-readable name
    std::optional<std::string> display_name;
    /// Optional email address
    std::optional<std::string> email;
    /// Additional metadata
    std::unordered_map<std::string, std::string> metadata;
    /// Timestamp when identity was created
    std::chrono::system_clock::time_point created_at;
    /// Timestamp when identity was last updated
    std::chrono::system_clock::time_point updated_at;
};

/**
 * @brief Digital certificate representation
 */
struct certificate {
    /// Certificate type
    certificate_type type;
    /// Subject identity
    peer_identity subject;
    /// Issuer identity (same as subject for self-signed)
    peer_identity issuer;
    /// Certificate serial number
    std::string serial_number;
    /// Valid from timestamp
    std::chrono::system_clock::time_point valid_from;
    /// Valid until timestamp
    std::chrono::system_clock::time_point valid_until;
    /// Certificate signature
    signature_data signature;
    /// Certificate extensions (key usage, etc.)
    std::unordered_map<std::string, std::string> extensions;
    /// Raw certificate data (DER or PEM format)
    std::vector<std::uint8_t> raw_data;
};

/**
 * @brief Authentication challenge data
 */
struct auth_challenge {
    /// Unique challenge identifier
    std::string challenge_id;
    /// Challenge nonce (cryptographically random)
    std::vector<std::uint8_t> nonce;
    /// Challenge timestamp
    std::chrono::system_clock::time_point timestamp;
    /// Challenge expiration time
    std::chrono::system_clock::time_point expires_at;
    /// Required authentication method
    auth_method required_method;
    /// Additional challenge parameters
    std::unordered_map<std::string, std::string> parameters;
};

/**
 * @brief Authentication response data
 */
struct auth_response {
    /// Challenge identifier this response is for
    std::string challenge_id;
    /// Peer identity making the response
    peer_identity peer;
    /// Response signature or proof
    signature_data proof;
    /// Optional certificate chain
    std::vector<certificate> certificate_chain;
    /// Response timestamp
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Authentication session information
 */
struct auth_session {
    /// Unique session identifier
    std::string session_id;
    /// Authenticated peer identity
    peer_identity peer;
    /// Authentication level achieved
    auth_level level;
    /// Authentication method used
    auth_method method;
    /// Session start time
    std::chrono::system_clock::time_point start_time;
    /// Session expiration time
    std::chrono::system_clock::time_point expires_at;
    /// Last activity timestamp
    std::chrono::system_clock::time_point last_activity;
    /// Session-specific keys
    std::optional<crypto_key> session_key;
    /// Session metadata
    std::unordered_map<std::string, std::string> metadata;
};

/**
 * @brief Trust level for peer relationships
 */
enum class trust_level {
    /// Peer is completely untrusted
    none = 0,
    /// Peer has minimal trust
    minimal = 25,
    /// Peer has moderate trust
    moderate = 50,
    /// Peer has high trust
    high = 75,
    /// Peer is fully trusted
    full = 100
};

/**
 * @brief Trust relationship between peers
 */
struct trust_relationship {
    /// Source peer (who trusts)
    ss::core::peer_id source_peer;
    /// Target peer (who is trusted)
    ss::core::peer_id target_peer;
    /// Level of trust
    trust_level level;
    /// Trust establishment timestamp
    std::chrono::system_clock::time_point established_at;
    /// Trust last updated timestamp
    std::chrono::system_clock::time_point updated_at;
    /// Optional trust metadata (reason, etc.)
    std::unordered_map<std::string, std::string> metadata;
};

/**
 * @brief Authentication event callback types
 */
using auth_success_callback = std::function<void(const auth_session&)>;
using auth_failure_callback = std::function<void(const ss::core::peer_id&, auth_error)>;
using cert_validation_callback = std::function<bool(const certificate&)>;

/**
 * @brief Authentication and authorization interface
 * 
 * Provides comprehensive authentication mechanisms for P2P networks including
 * challenge-response authentication, certificate management, and web of trust.
 * 
 * All operations are designed to be resistant to timing attacks and provide
 * protection against common authentication vulnerabilities.
 * 
 * Thread safety: All operations must be thread-safe for concurrent access.
 */
class i_auth : public ss::core::i_component {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~i_auth() = default;

    // === Peer Authentication ===

    /**
     * @brief Initiate authentication with a peer
     * @param peer_endpoint Peer to authenticate with
     * @param method Preferred authentication method
     * @param timeout Authentication timeout
     * @return Authentication session or error
     */
    virtual ss::core::async_result<ss::core::result<auth_session, auth_error>>
    authenticate_peer(
        const ss::core::endpoint& peer_endpoint,
        auth_method method,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{30000}) = 0;

    /**
     * @brief Respond to authentication challenge
     * @param challenge Authentication challenge from peer
     * @return Authentication response or error
     */
    virtual ss::core::result<auth_response, auth_error>
    respond_to_challenge(const auth_challenge& challenge) = 0;

    /**
     * @brief Verify authentication response
     * @param response Authentication response to verify
     * @return Authentication session if valid, error otherwise
     */
    virtual ss::core::result<auth_session, auth_error>
    verify_auth_response(const auth_response& response) = 0;

    // === Session Management ===

    /**
     * @brief Get active authentication session for peer
     * @param peer_id Peer identifier
     * @return Active session or nullopt if no session
     */
    virtual std::optional<auth_session>
    get_session(const ss::core::peer_id& peer_id) const noexcept = 0;

    /**
     * @brief Check if peer is authenticated
     * @param peer_id Peer identifier
     * @param required_level Minimum required authentication level
     * @return true if authenticated at required level
     */
    virtual bool is_authenticated(
        const ss::core::peer_id& peer_id,
        auth_level required_level = auth_level::basic) const noexcept = 0;

    /**
     * @brief Renew authentication session
     * @param session_id Session to renew
     * @param extension_time Additional time to extend session
     * @return Updated session or error
     */
    virtual ss::core::result<auth_session, auth_error>
    renew_session(
        const std::string& session_id,
        std::chrono::milliseconds extension_time) = 0;

    /**
     * @brief Terminate authentication session
     * @param session_id Session to terminate
     * @return Success or error
     */
    virtual ss::core::result<void, auth_error>
    terminate_session(const std::string& session_id) = 0;

    /**
     * @brief Get all active sessions
     * @return Vector of active sessions
     */
    virtual std::vector<auth_session> get_active_sessions() const = 0;

    // === Certificate Management ===

    /**
     * @brief Create self-signed certificate
     * @param identity Peer identity for certificate
     * @param validity_period Certificate validity period
     * @param key_pair Key pair for certificate
     * @return Generated certificate or error
     */
    virtual ss::core::result<certificate, auth_error>
    create_self_signed_certificate(
        const peer_identity& identity,
        std::chrono::hours validity_period,
        const key_pair& key_pair) = 0;

    /**
     * @brief Load certificate from data
     * @param cert_data Certificate data (DER or PEM format)
     * @return Parsed certificate or error
     */
    virtual ss::core::result<certificate, auth_error>
    load_certificate(const std::vector<std::uint8_t>& cert_data) = 0;

    /**
     * @brief Validate certificate
     * @param cert Certificate to validate
     * @param trust_anchors Trusted root certificates
     * @return Certificate status
     */
    virtual certificate_status
    validate_certificate(
        const certificate& cert,
        const std::vector<certificate>& trust_anchors = {}) const = 0;

    /**
     * @brief Add trusted certificate
     * @param cert Certificate to trust
     * @return Success or error
     */
    virtual ss::core::result<void, auth_error>
    add_trusted_certificate(const certificate& cert) = 0;

    /**
     * @brief Remove trusted certificate
     * @param serial_number Certificate serial number to remove
     * @return Success or error
     */
    virtual ss::core::result<void, auth_error>
    remove_trusted_certificate(const std::string& serial_number) = 0;

    /**
     * @brief Get trusted certificates
     * @return Vector of trusted certificates
     */
    virtual std::vector<certificate> get_trusted_certificates() const = 0;

    // === Web of Trust ===

    /**
     * @brief Add trust relationship
     * @param relationship Trust relationship to add
     * @return Success or error
     */
    virtual ss::core::result<void, auth_error>
    add_trust_relationship(const trust_relationship& relationship) = 0;

    /**
     * @brief Remove trust relationship
     * @param source_peer Source peer ID
     * @param target_peer Target peer ID
     * @return Success or error
     */
    virtual ss::core::result<void, auth_error>
    remove_trust_relationship(
        const ss::core::peer_id& source_peer,
        const ss::core::peer_id& target_peer) = 0;

    /**
     * @brief Get trust level for peer
     * @param peer_id Peer to get trust level for
     * @return Trust level (none if no relationship)
     */
    virtual trust_level get_trust_level(const ss::core::peer_id& peer_id) const = 0;

    /**
     * @brief Calculate transitive trust
     * @param peer_id Target peer ID
     * @param max_hops Maximum hops in trust chain
     * @return Calculated trust level through web of trust
     */
    virtual trust_level calculate_transitive_trust(
        const ss::core::peer_id& peer_id,
        std::uint32_t max_hops = 3) const = 0;

    /**
     * @brief Get all trust relationships
     * @return Vector of trust relationships
     */
    virtual std::vector<trust_relationship> get_trust_relationships() const = 0;

    // === Message Authentication ===

    /**
     * @brief Authenticate message from peer
     * @param message Message data
     * @param sender_id Claimed sender peer ID
     * @param signature Message signature
     * @return true if message is authentic
     */
    virtual ss::core::result<bool, auth_error>
    authenticate_message(
        const std::vector<std::uint8_t>& message,
        const ss::core::peer_id& sender_id,
        const signature_data& signature) = 0;

    /**
     * @brief Sign message for sending
     * @param message Message data to sign
     * @return Message signature or error
     */
    virtual ss::core::result<signature_data, auth_error>
    sign_message(const std::vector<std::uint8_t>& message) = 0;

    // === Rate Limiting and Security ===

    /**
     * @brief Check if peer is rate limited
     * @param peer_id Peer to check
     * @return true if peer is currently rate limited
     */
    virtual bool is_rate_limited(const ss::core::peer_id& peer_id) const noexcept = 0;

    /**
     * @brief Add peer to blacklist
     * @param peer_id Peer to blacklist
     * @param reason Reason for blacklisting
     * @param duration Duration of blacklist (permanent if not specified)
     */
    virtual void blacklist_peer(
        const ss::core::peer_id& peer_id,
        const std::string& reason,
        std::optional<std::chrono::milliseconds> duration = std::nullopt) = 0;

    /**
     * @brief Remove peer from blacklist
     * @param peer_id Peer to remove from blacklist
     */
    virtual void whitelist_peer(const ss::core::peer_id& peer_id) = 0;

    /**
     * @brief Check if peer is blacklisted
     * @param peer_id Peer to check
     * @return true if peer is blacklisted
     */
    virtual bool is_blacklisted(const ss::core::peer_id& peer_id) const noexcept = 0;

    // === Event Callbacks ===

    /**
     * @brief Set authentication success callback
     * @param callback Callback function for successful authentication
     */
    virtual void set_auth_success_callback(auth_success_callback callback) = 0;

    /**
     * @brief Set authentication failure callback
     * @param callback Callback function for authentication failures
     */
    virtual void set_auth_failure_callback(auth_failure_callback callback) = 0;

    /**
     * @brief Set certificate validation callback
     * @param callback Callback function for custom certificate validation
     */
    virtual void set_cert_validation_callback(cert_validation_callback callback) = 0;

    // === Configuration ===

    /**
     * @brief Set authentication timeout
     * @param timeout Default authentication timeout
     */
    virtual void set_auth_timeout(std::chrono::milliseconds timeout) = 0;

    /**
     * @brief Set session timeout
     * @param timeout Default session timeout
     */
    virtual void set_session_timeout(std::chrono::milliseconds timeout) = 0;

    /**
     * @brief Set maximum authentication attempts per peer
     * @param max_attempts Maximum attempts before rate limiting
     */
    virtual void set_max_auth_attempts(std::uint32_t max_attempts) = 0;

    /**
     * @brief Enable or disable automatic session renewal
     * @param enabled true to enable automatic renewal
     */
    virtual void set_auto_session_renewal(bool enabled) = 0;

protected:
    /**
     * @brief Protected default constructor
     */
    i_auth() = default;

    /**
     * @brief Protected copy constructor (deleted)
     */
    i_auth(const i_auth&) = delete;

    /**
     * @brief Protected move constructor (deleted)
     */
    i_auth(i_auth&&) = delete;

    /**
     * @brief Protected copy assignment (deleted)
     */
    i_auth& operator=(const i_auth&) = delete;

    /**
     * @brief Protected move assignment (deleted)
     */
    i_auth& operator=(i_auth&&) = delete;
};

/**
 * @brief Smart pointer type for auth instances
 */
using auth_ptr = std::shared_ptr<i_auth>;

/**
 * @brief Weak pointer type for auth instances
 */
using auth_weak_ptr = std::weak_ptr<i_auth>;

} // namespace ss::security