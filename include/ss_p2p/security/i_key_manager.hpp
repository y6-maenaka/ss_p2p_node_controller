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
 * @brief Key management error codes
 */
enum class key_error {
    /// No error occurred
    none = 0,
    /// Key not found in storage
    key_not_found,
    /// Key already exists
    key_already_exists,
    /// Invalid key format or data
    invalid_key_format,
    /// Key has expired
    key_expired,
    /// Key is not yet valid
    key_not_yet_valid,
    /// Insufficient permissions to access key
    access_denied,
    /// Key storage is corrupted
    storage_corrupted,
    /// Key storage is locked
    storage_locked,
    /// Invalid passphrase or authentication
    invalid_passphrase,
    /// Key derivation failed
    derivation_failed,
    /// Key exchange failed
    exchange_failed,
    /// Hardware security module error
    hsm_error,
    /// Memory protection failed
    memory_protection_failed,
    /// Backup operation failed
    backup_failed,
    /// Restore operation failed
    restore_failed,
    /// Key rotation failed
    rotation_failed,
    /// Storage quota exceeded
    storage_full,
    /// Unknown key management error
    unknown
};

/**
 * @brief Convert key_error to string representation
 * @param error Error code to convert
 * @return String description of the error
 */
std::string to_string(key_error error) noexcept;

/**
 * @brief Key usage permissions
 */
enum class key_usage : std::uint32_t {
    /// No specific usage restriction
    none = 0,
    /// Key can be used for encryption
    encrypt = 1 << 0,
    /// Key can be used for decryption
    decrypt = 1 << 1,
    /// Key can be used for signing
    sign = 1 << 2,
    /// Key can be used for verification
    verify = 1 << 3,
    /// Key can be used for key agreement
    key_agreement = 1 << 4,
    /// Key can be used for key derivation
    key_derivation = 1 << 5,
    /// Key can be used for authentication
    authenticate = 1 << 6,
    /// Key can be exported
    exportable = 1 << 7,
    /// All operations allowed
    all = 0xFFFFFFFF
};

/**
 * @brief Bitwise operators for key_usage
 */
constexpr key_usage operator|(key_usage lhs, key_usage rhs) noexcept {
    return static_cast<key_usage>(
        static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

constexpr key_usage operator&(key_usage lhs, key_usage rhs) noexcept {
    return static_cast<key_usage>(
        static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
}

constexpr bool has_usage(key_usage flags, key_usage usage) noexcept {
    return (flags & usage) == usage;
}

/**
 * @brief Key storage security level
 */
enum class security_level {
    /// Standard software-based storage
    software = 0,
    /// Enhanced software storage with memory protection
    software_protected = 1,
    /// Hardware security module storage
    hardware = 2,
    /// Trusted platform module storage
    tpm = 3
};

/**
 * @brief Key metadata information
 */
struct key_metadata {
    /// Unique key identifier
    std::string key_id;
    /// Human-readable key label
    std::string label;
    /// Key algorithm identifier
    std::string algorithm;
    /// Key size in bits
    std::uint32_t key_size;
    /// Key usage permissions
    key_usage usage;
    /// Security level for key storage
    security_level sec_level;
    /// Key creation timestamp
    std::chrono::system_clock::time_point created_at;
    /// Key expiration timestamp (optional)
    std::optional<std::chrono::system_clock::time_point> expires_at;
    /// Last access timestamp
    std::chrono::system_clock::time_point last_accessed;
    /// Number of times key has been used
    std::uint64_t usage_count;
    /// Additional metadata
    std::unordered_map<std::string, std::string> attributes;
};

/**
 * @brief Key backup data
 */
struct key_backup {
    /// Backup format version
    std::uint32_t version;
    /// Backup creation timestamp
    std::chrono::system_clock::time_point created_at;
    /// Encrypted key data
    std::vector<std::uint8_t> encrypted_data;
    /// Key derivation parameters
    std::unordered_map<std::string, std::string> kdf_params;
    /// Backup metadata
    std::unordered_map<std::string, std::string> metadata;
    /// Integrity hash
    std::vector<std::uint8_t> integrity_hash;
};

/**
 * @brief Key exchange parameters for Diffie-Hellman
 */
struct key_exchange_params {
    /// Algorithm identifier (e.g., "X25519", "ECDH-P256")
    std::string algorithm;
    /// Our public key
    crypto_key our_public_key;
    /// Our private key
    crypto_key our_private_key;
    /// Peer's public key
    crypto_key peer_public_key;
    /// Optional key derivation info
    std::vector<std::uint8_t> kdf_info;
    /// Optional salt for key derivation
    std::vector<std::uint8_t> salt;
};

/**
 * @brief Derived key information
 */
struct derived_key_info {
    /// Base key identifier used for derivation
    std::string base_key_id;
    /// Derivation algorithm used
    std::string derivation_algorithm;
    /// Derivation parameters
    std::unordered_map<std::string, std::string> parameters;
    /// Salt used in derivation
    std::vector<std::uint8_t> salt;
    /// Context information used
    std::vector<std::uint8_t> context;
};

/**
 * @brief Key rotation policy
 */
struct key_rotation_policy {
    /// Automatic rotation interval
    std::chrono::hours rotation_interval;
    /// Maximum key age before forced rotation
    std::chrono::hours max_key_age;
    /// Maximum number of operations before rotation
    std::uint64_t max_operations;
    /// Overlap period for smooth key transition
    std::chrono::hours overlap_period;
    /// Whether to enable automatic rotation
    bool auto_rotate;
};

/**
 * @brief Key storage configuration
 */
struct storage_config {
    /// Storage backend type
    std::string backend_type;
    /// Storage location (file path, HSM slot, etc.)
    std::string location;
    /// Security level required
    security_level sec_level;
    /// Enable storage encryption
    bool encrypt_storage;
    /// Maximum number of keys to store
    std::uint32_t max_keys;
    /// Storage-specific parameters
    std::unordered_map<std::string, std::string> parameters;
};

/**
 * @brief Key access callback types
 */
using key_access_callback = std::function<bool(const std::string& key_id, key_usage requested_usage)>;
using key_rotation_callback = std::function<void(const std::string& old_key_id, const std::string& new_key_id)>;
using key_expiry_callback = std::function<void(const std::string& key_id)>;

/**
 * @brief Key management interface
 * 
 * Provides comprehensive key lifecycle management including generation, storage,
 * rotation, backup, and secure deletion. Supports various security levels from
 * software-based storage to hardware security modules.
 * 
 * All key operations are designed to prevent key material exposure and provide
 * secure memory handling with automatic zeroing.
 * 
 * Thread safety: All operations must be thread-safe for concurrent access.
 */
class i_key_manager : public ss::core::i_component {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~i_key_manager() = default;

    // === Key Generation ===

    /**
     * @brief Generate symmetric key
     * @param algorithm Symmetric algorithm to generate key for
     * @param key_id Unique identifier for the key
     * @param metadata Key metadata
     * @return Success or error
     */
    virtual ss::core::result<void, key_error>
    generate_symmetric_key(
        symmetric_algorithm algorithm,
        const std::string& key_id,
        const key_metadata& metadata = {}) = 0;

    /**
     * @brief Generate asymmetric key pair
     * @param algorithm Asymmetric algorithm to generate keys for
     * @param key_id Base identifier for the key pair (will append _pub/_priv)
     * @param metadata Key metadata
     * @return Success or error
     */
    virtual ss::core::result<void, key_error>
    generate_key_pair(
        asymmetric_algorithm algorithm,
        const std::string& key_id,
        const key_metadata& metadata = {}) = 0;

    /**
     * @brief Generate signature key pair
     * @param algorithm Signature algorithm to generate keys for
     * @param key_id Base identifier for the key pair
     * @param metadata Key metadata
     * @return Success or error
     */
    virtual ss::core::result<void, key_error>
    generate_signature_key_pair(
        signature_algorithm algorithm,
        const std::string& key_id,
        const key_metadata& metadata = {}) = 0;

    // === Key Storage and Retrieval ===

    /**
     * @brief Store key in secure storage
     * @param key_id Unique identifier for the key
     * @param key Key to store
     * @param metadata Key metadata
     * @return Success or error
     */
    virtual ss::core::result<void, key_error>
    store_key(
        const std::string& key_id,
        const crypto_key& key,
        const key_metadata& metadata) = 0;

    /**
     * @brief Retrieve key from storage
     * @param key_id Key identifier
     * @param usage Intended usage for access control
     * @return Retrieved key or error
     */
    virtual ss::core::result<crypto_key, key_error>
    get_key(
        const std::string& key_id,
        key_usage usage = key_usage::all) = 0;

    /**
     * @brief Check if key exists in storage
     * @param key_id Key identifier
     * @return true if key exists, false otherwise
     */
    virtual bool has_key(const std::string& key_id) const noexcept = 0;

    /**
     * @brief Delete key from storage
     * @param key_id Key identifier
     * @return Success or error
     */
    virtual ss::core::result<void, key_error>
    delete_key(const std::string& key_id) = 0;

    /**
     * @brief Get key metadata
     * @param key_id Key identifier
     * @return Key metadata or error
     */
    virtual ss::core::result<key_metadata, key_error>
    get_key_metadata(const std::string& key_id) const = 0;

    /**
     * @brief Update key metadata
     * @param key_id Key identifier
     * @param metadata New metadata
     * @return Success or error
     */
    virtual ss::core::result<void, key_error>
    update_key_metadata(
        const std::string& key_id,
        const key_metadata& metadata) = 0;

    /**
     * @brief List all stored keys
     * @param filter Optional filter criteria
     * @return Vector of key identifiers
     */
    virtual std::vector<std::string>
    list_keys(const std::unordered_map<std::string, std::string>& filter = {}) const = 0;

    // === Key Derivation ===

    /**
     * @brief Derive key from existing key using HKDF
     * @param base_key_id Base key identifier
     * @param derived_key_id New derived key identifier
     * @param info Derivation context information
     * @param salt Optional salt
     * @param key_length Desired derived key length
     * @param metadata Metadata for derived key
     * @return Success or error
     */
    virtual ss::core::result<void, key_error>
    derive_key_hkdf(
        const std::string& base_key_id,
        const std::string& derived_key_id,
        const std::vector<std::uint8_t>& info,
        const std::vector<std::uint8_t>& salt,
        std::size_t key_length,
        const key_metadata& metadata = {}) = 0;

    /**
     * @brief Derive key from password using PBKDF2
     * @param password Password for derivation
     * @param key_id Identifier for derived key
     * @param salt Random salt
     * @param iterations Number of iterations
     * @param key_length Desired key length
     * @param metadata Metadata for derived key
     * @return Success or error
     */
    virtual ss::core::result<void, key_error>
    derive_key_pbkdf2(
        const std::string& password,
        const std::string& key_id,
        const std::vector<std::uint8_t>& salt,
        std::uint32_t iterations,
        std::size_t key_length,
        const key_metadata& metadata = {}) = 0;

    /**
     * @brief Get derivation information for derived key
     * @param key_id Derived key identifier
     * @return Derivation information or error
     */
    virtual ss::core::result<derived_key_info, key_error>
    get_derived_key_info(const std::string& key_id) const = 0;

    // === Key Exchange ===

    /**
     * @brief Perform Diffie-Hellman key exchange
     * @param params Key exchange parameters
     * @param shared_key_id Identifier for resulting shared key
     * @param metadata Metadata for shared key
     * @return Success or error
     */
    virtual ss::core::result<void, key_error>
    perform_key_exchange(
        const key_exchange_params& params,
        const std::string& shared_key_id,
        const key_metadata& metadata = {}) = 0;

    /**
     * @brief Generate ephemeral key pair for key exchange
     * @param algorithm Key exchange algorithm
     * @return Generated key pair or error
     */
    virtual ss::core::result<key_pair, key_error>
    generate_ephemeral_key_pair(asymmetric_algorithm algorithm) = 0;

    // === Key Rotation ===

    /**
     * @brief Set key rotation policy
     * @param key_id Key identifier
     * @param policy Rotation policy
     * @return Success or error
     */
    virtual ss::core::result<void, key_error>
    set_rotation_policy(
        const std::string& key_id,
        const key_rotation_policy& policy) = 0;

    /**
     * @brief Rotate key manually
     * @param key_id Key identifier to rotate
     * @param new_key_id Identifier for new key (optional)
     * @return New key identifier or error
     */
    virtual ss::core::result<std::string, key_error>
    rotate_key(
        const std::string& key_id,
        const std::optional<std::string>& new_key_id = std::nullopt) = 0;

    /**
     * @brief Check if key needs rotation
     * @param key_id Key identifier
     * @return true if rotation is needed
     */
    virtual bool needs_rotation(const std::string& key_id) const = 0;

    /**
     * @brief Get keys that need rotation
     * @return Vector of key identifiers needing rotation
     */
    virtual std::vector<std::string> get_keys_needing_rotation() const = 0;

    // === Key Backup and Recovery ===

    /**
     * @brief Create encrypted backup of key
     * @param key_id Key identifier
     * @param passphrase Passphrase for backup encryption
     * @return Encrypted backup data or error
     */
    virtual ss::core::result<key_backup, key_error>
    backup_key(
        const std::string& key_id,
        const std::string& passphrase) = 0;

    /**
     * @brief Restore key from encrypted backup
     * @param backup Backup data
     * @param passphrase Passphrase for backup decryption
     * @param key_id Key identifier for restored key
     * @return Success or error
     */
    virtual ss::core::result<void, key_error>
    restore_key(
        const key_backup& backup,
        const std::string& passphrase,
        const std::string& key_id) = 0;

    /**
     * @brief Create full keystore backup
     * @param passphrase Master passphrase for backup
     * @return Encrypted backup data or error
     */
    virtual ss::core::result<std::vector<std::uint8_t>, key_error>
    backup_keystore(const std::string& passphrase) = 0;

    /**
     * @brief Restore full keystore from backup
     * @param backup_data Encrypted backup data
     * @param passphrase Master passphrase
     * @param overwrite_existing Whether to overwrite existing keys
     * @return Success or error
     */
    virtual ss::core::result<void, key_error>
    restore_keystore(
        const std::vector<std::uint8_t>& backup_data,
        const std::string& passphrase,
        bool overwrite_existing = false) = 0;

    // === Storage Management ===

    /**
     * @brief Initialize key storage
     * @param config Storage configuration
     * @param master_passphrase Master passphrase for storage encryption
     * @return Success or error
     */
    virtual ss::core::result<void, key_error>
    initialize_storage(
        const storage_config& config,
        const std::string& master_passphrase = "") = 0;

    /**
     * @brief Lock key storage
     * @return Success or error
     */
    virtual ss::core::result<void, key_error> lock_storage() = 0;

    /**
     * @brief Unlock key storage
     * @param passphrase Master passphrase
     * @return Success or error
     */
    virtual ss::core::result<void, key_error>
    unlock_storage(const std::string& passphrase) = 0;

    /**
     * @brief Check if storage is locked
     * @return true if storage is locked
     */
    virtual bool is_storage_locked() const noexcept = 0;

    /**
     * @brief Change master passphrase
     * @param old_passphrase Current passphrase
     * @param new_passphrase New passphrase
     * @return Success or error
     */
    virtual ss::core::result<void, key_error>
    change_master_passphrase(
        const std::string& old_passphrase,
        const std::string& new_passphrase) = 0;

    /**
     * @brief Get storage statistics
     * @return Map of storage statistics
     */
    virtual std::unordered_map<std::string, std::uint64_t>
    get_storage_stats() const = 0;

    /**
     * @brief Compact storage (remove deleted key slots)
     * @return Success or error
     */
    virtual ss::core::result<void, key_error> compact_storage() = 0;

    // === Security Operations ===

    /**
     * @brief Secure delete of key material from memory
     * @param key_id Key identifier
     * @return Success or error
     */
    virtual ss::core::result<void, key_error>
    secure_delete(const std::string& key_id) = 0;

    /**
     * @brief Enable memory protection for key storage
     * @return Success or error
     */
    virtual ss::core::result<void, key_error>
    enable_memory_protection() = 0;

    /**
     * @brief Get available security levels
     * @return Vector of supported security levels
     */
    virtual std::vector<security_level>
    get_available_security_levels() const = 0;

    // === Event Callbacks ===

    /**
     * @brief Set key access callback
     * @param callback Callback for key access authorization
     */
    virtual void set_key_access_callback(key_access_callback callback) = 0;

    /**
     * @brief Set key rotation callback
     * @param callback Callback for key rotation events
     */
    virtual void set_key_rotation_callback(key_rotation_callback callback) = 0;

    /**
     * @brief Set key expiry callback
     * @param callback Callback for key expiration events
     */
    virtual void set_key_expiry_callback(key_expiry_callback callback) = 0;

protected:
    /**
     * @brief Protected default constructor
     */
    i_key_manager() = default;

    /**
     * @brief Protected copy constructor (deleted)
     */
    i_key_manager(const i_key_manager&) = delete;

    /**
     * @brief Protected move constructor (deleted)
     */
    i_key_manager(i_key_manager&&) = delete;

    /**
     * @brief Protected copy assignment (deleted)
     */
    i_key_manager& operator=(const i_key_manager&) = delete;

    /**
     * @brief Protected move assignment (deleted)
     */
    i_key_manager& operator=(i_key_manager&&) = delete;
};

/**
 * @brief Smart pointer type for key manager instances
 */
using key_manager_ptr = std::shared_ptr<i_key_manager>;

/**
 * @brief Weak pointer type for key manager instances
 */
using key_manager_weak_ptr = std::weak_ptr<i_key_manager>;

} // namespace ss::security