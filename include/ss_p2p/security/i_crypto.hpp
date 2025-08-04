#pragma once

#include "../core/types.hpp"
#include "../core/result.hpp"
#include "../core/interfaces.hpp"

#include <vector>
#include <array>
#include <string>
#include <memory>
#include <chrono>
#include <optional>

namespace ss::security {

/**
 * @brief Cryptographic error codes
 */
enum class crypto_error {
    /// No error occurred
    none = 0,
    /// Invalid key size or format
    invalid_key,
    /// Invalid input data size or format
    invalid_input,
    /// Encryption operation failed
    encryption_failed,
    /// Decryption operation failed
    decryption_failed,
    /// Authentication verification failed
    verification_failed,
    /// Key generation failed
    key_generation_failed,
    /// Signature generation failed
    signature_failed,
    /// Hash computation failed
    hash_failed,
    /// Random number generation failed
    random_failed,
    /// Insufficient entropy for operation
    insufficient_entropy,
    /// Algorithm not supported
    unsupported_algorithm,
    /// Memory allocation failed
    memory_error,
    /// Hardware acceleration not available
    hardware_unavailable,
    /// Operation would exceed maximum allowed time
    timeout,
    /// Unknown cryptographic error
    unknown
};

/**
 * @brief Convert crypto_error to string representation
 * @param error Error code to convert
 * @return String description of the error
 */
std::string to_string(crypto_error error) noexcept;

/**
 * @brief Supported symmetric encryption algorithms
 */
enum class symmetric_algorithm {
    /// AES-256 in GCM mode (authenticated encryption)
    aes_256_gcm,
    /// AES-192 in GCM mode
    aes_192_gcm,
    /// AES-128 in GCM mode
    aes_128_gcm,
    /// ChaCha20-Poly1305 (authenticated encryption)
    chacha20_poly1305
};

/**
 * @brief Supported asymmetric encryption algorithms
 */
enum class asymmetric_algorithm {
    /// RSA with OAEP padding
    rsa_oaep,
    /// Elliptic Curve Integrated Encryption Scheme
    ecies_p256,
    /// X25519 with ChaCha20-Poly1305
    x25519_chacha20_poly1305
};

/**
 * @brief Supported digital signature algorithms
 */
enum class signature_algorithm {
    /// Ed25519 signature scheme
    ed25519,
    /// ECDSA with P-256 curve and SHA-256
    ecdsa_p256_sha256,
    /// RSA-PSS with SHA-256
    rsa_pss_sha256
};

/**
 * @brief Supported hash algorithms
 */
enum class hash_algorithm {
    /// SHA-256
    sha256,
    /// SHA-384
    sha384,
    /// SHA-512
    sha512,
    /// SHA-3-256
    sha3_256,
    /// SHA-3-512
    sha3_512,
    /// BLAKE2b-256
    blake2b_256,
    /// BLAKE2b-512
    blake2b_512
};

/**
 * @brief Cryptographic key types
 */
enum class key_type {
    /// Symmetric encryption key
    symmetric,
    /// Asymmetric public key
    public_key,
    /// Asymmetric private key
    private_key,
    /// Key derivation key
    kdf_key
};

/**
 * @brief Secure byte container for cryptographic data
 * 
 * Automatically zeroes memory on destruction and provides
 * protection against memory disclosure.
 */
class secure_bytes {
public:
    using value_type = std::uint8_t;
    using size_type = std::size_t;
    using iterator = value_type*;
    using const_iterator = const value_type*;

    /**
     * @brief Default constructor
     */
    secure_bytes() noexcept = default;

    /**
     * @brief Construct with specific size
     * @param size Number of bytes to allocate
     */
    explicit secure_bytes(size_type size);

    /**
     * @brief Construct from data
     * @param data Data to copy
     */
    explicit secure_bytes(const std::vector<std::uint8_t>& data);

    /**
     * @brief Construct from data
     * @param data Data to move
     */
    explicit secure_bytes(std::vector<std::uint8_t>&& data);

    /**
     * @brief Copy constructor (creates secure copy)
     */
    secure_bytes(const secure_bytes& other);

    /**
     * @brief Move constructor
     */
    secure_bytes(secure_bytes&& other) noexcept;

    /**
     * @brief Copy assignment (creates secure copy)
     */
    secure_bytes& operator=(const secure_bytes& other);

    /**
     * @brief Move assignment
     */
    secure_bytes& operator=(secure_bytes&& other) noexcept;

    /**
     * @brief Destructor - automatically zeroes memory
     */
    ~secure_bytes();

    /**
     * @brief Get data pointer
     */
    value_type* data() noexcept { return data_.data(); }

    /**
     * @brief Get const data pointer
     */
    const value_type* data() const noexcept { return data_.data(); }

    /**
     * @brief Get size in bytes
     */
    size_type size() const noexcept { return data_.size(); }

    /**
     * @brief Check if empty
     */
    bool empty() const noexcept { return data_.empty(); }

    /**
     * @brief Resize container
     */
    void resize(size_type new_size);

    /**
     * @brief Clear and zero memory
     */
    void clear() noexcept;

    /**
     * @brief Iterator access
     */
    iterator begin() noexcept { return data_.data(); }
    const_iterator begin() const noexcept { return data_.data(); }
    iterator end() noexcept { return data_.data() + data_.size(); }
    const_iterator end() const noexcept { return data_.data() + data_.size(); }

    /**
     * @brief Element access
     */
    value_type& operator[](size_type index) { return data_[index]; }
    const value_type& operator[](size_type index) const { return data_[index]; }

    /**
     * @brief Convert to vector (copies data)
     */
    std::vector<std::uint8_t> to_vector() const;

private:
    std::vector<std::uint8_t> data_;
    
    void secure_zero() noexcept;
};

/**
 * @brief Cryptographic key representation
 */
class crypto_key {
public:
    /**
     * @brief Construct key from data
     * @param type Key type
     * @param algorithm Algorithm this key is for
     * @param data Key material
     */
    crypto_key(key_type type, std::string algorithm, secure_bytes data);

    /**
     * @brief Copy constructor (creates secure copy)
     */
    crypto_key(const crypto_key& other);

    /**
     * @brief Move constructor
     */
    crypto_key(crypto_key&& other) noexcept;

    /**
     * @brief Copy assignment (creates secure copy)
     */
    crypto_key& operator=(const crypto_key& other);

    /**
     * @brief Move assignment
     */
    crypto_key& operator=(crypto_key&& other) noexcept;

    /**
     * @brief Destructor
     */
    ~crypto_key() = default;

    /**
     * @brief Get key type
     */
    key_type type() const noexcept { return type_; }

    /**
     * @brief Get algorithm identifier
     */
    const std::string& algorithm() const noexcept { return algorithm_; }

    /**
     * @brief Get key material
     */
    const secure_bytes& data() const noexcept { return data_; }

    /**
     * @brief Get key size in bytes
     */
    std::size_t size() const noexcept { return data_.size(); }

    /**
     * @brief Check if key is valid
     */
    bool is_valid() const noexcept;

    /**
     * @brief Get key fingerprint (SHA-256 hash)
     */
    std::string fingerprint() const;

private:
    key_type type_;
    std::string algorithm_;
    secure_bytes data_;
};

/**
 * @brief Asymmetric key pair
 */
struct key_pair {
    crypto_key public_key;
    crypto_key private_key;

    /**
     * @brief Constructor
     */
    key_pair(crypto_key pub, crypto_key priv)
        : public_key(std::move(pub)), private_key(std::move(priv)) {}
};

/**
 * @brief Encrypted data with authentication tag
 */
struct encrypted_data {
    /// Ciphertext
    std::vector<std::uint8_t> ciphertext;
    /// Authentication tag (for authenticated encryption)
    std::vector<std::uint8_t> tag;
    /// Initialization vector / nonce
    std::vector<std::uint8_t> iv;
    /// Additional authenticated data (optional)
    std::vector<std::uint8_t> aad;
    /// Algorithm used for encryption
    std::string algorithm;
};

/**
 * @brief Digital signature data
 */
struct signature_data {
    /// Signature bytes
    std::vector<std::uint8_t> signature;
    /// Algorithm used for signing
    std::string algorithm;
    /// Public key fingerprint of signer
    std::string signer_fingerprint;
    /// Timestamp when signature was created
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Cryptographic operations interface
 * 
 * Provides secure cryptographic primitives including symmetric and asymmetric
 * encryption, digital signatures, and cryptographic hashing. All implementations
 * must be resistant to timing attacks and provide constant-time operations where
 * possible.
 * 
 * Thread safety: All operations must be thread-safe for concurrent access.
 * Memory security: All sensitive data must be properly zeroed after use.
 */
class i_crypto : public ss::core::i_component {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~i_crypto() = default;

    // === Symmetric Encryption ===

    /**
     * @brief Generate symmetric encryption key
     * @param algorithm Symmetric algorithm to generate key for
     * @return Generated key or error
     */
    virtual ss::core::result<crypto_key, crypto_error>
    generate_symmetric_key(symmetric_algorithm algorithm) = 0;

    /**
     * @brief Encrypt data with symmetric key
     * @param plaintext Data to encrypt
     * @param key Symmetric encryption key
     * @param aad Additional authenticated data (optional)
     * @return Encrypted data or error
     */
    virtual ss::core::result<encrypted_data, crypto_error>
    symmetric_encrypt(
        const std::vector<std::uint8_t>& plaintext,
        const crypto_key& key,
        const std::vector<std::uint8_t>& aad = {}) = 0;

    /**
     * @brief Decrypt data with symmetric key
     * @param encrypted Encrypted data
     * @param key Symmetric decryption key
     * @return Decrypted plaintext or error
     */
    virtual ss::core::result<std::vector<std::uint8_t>, crypto_error>
    symmetric_decrypt(
        const encrypted_data& encrypted,
        const crypto_key& key) = 0;

    // === Asymmetric Encryption ===

    /**
     * @brief Generate asymmetric key pair
     * @param algorithm Asymmetric algorithm to generate keys for
     * @return Generated key pair or error
     */
    virtual ss::core::result<key_pair, crypto_error>
    generate_key_pair(asymmetric_algorithm algorithm) = 0;

    /**
     * @brief Encrypt data with public key
     * @param plaintext Data to encrypt
     * @param public_key Public key for encryption
     * @return Encrypted data or error
     */
    virtual ss::core::result<std::vector<std::uint8_t>, crypto_error>
    asymmetric_encrypt(
        const std::vector<std::uint8_t>& plaintext,
        const crypto_key& public_key) = 0;

    /**
     * @brief Decrypt data with private key
     * @param ciphertext Encrypted data
     * @param private_key Private key for decryption
     * @return Decrypted plaintext or error
     */
    virtual ss::core::result<std::vector<std::uint8_t>, crypto_error>
    asymmetric_decrypt(
        const std::vector<std::uint8_t>& ciphertext,
        const crypto_key& private_key) = 0;

    // === Digital Signatures ===

    /**
     * @brief Generate signature key pair
     * @param algorithm Signature algorithm to generate keys for
     * @return Generated key pair or error
     */
    virtual ss::core::result<key_pair, crypto_error>
    generate_signature_key_pair(signature_algorithm algorithm) = 0;

    /**
     * @brief Sign data with private key
     * @param data Data to sign
     * @param private_key Private key for signing
     * @return Digital signature or error
     */
    virtual ss::core::result<signature_data, crypto_error>
    sign(
        const std::vector<std::uint8_t>& data,
        const crypto_key& private_key) = 0;

    /**
     * @brief Verify signature with public key
     * @param data Original data
     * @param signature Signature to verify
     * @param public_key Public key for verification
     * @return true if signature is valid, false if invalid, error on failure
     */
    virtual ss::core::result<bool, crypto_error>
    verify(
        const std::vector<std::uint8_t>& data,
        const signature_data& signature,
        const crypto_key& public_key) = 0;

    // === Cryptographic Hashing ===

    /**
     * @brief Compute hash of data
     * @param data Data to hash
     * @param algorithm Hash algorithm to use
     * @return Hash digest or error
     */
    virtual ss::core::result<std::vector<std::uint8_t>, crypto_error>
    hash(
        const std::vector<std::uint8_t>& data,
        hash_algorithm algorithm) = 0;

    /**
     * @brief Compute HMAC of data
     * @param data Data to authenticate
     * @param key HMAC key
     * @param algorithm Hash algorithm for HMAC
     * @return HMAC digest or error
     */
    virtual ss::core::result<std::vector<std::uint8_t>, crypto_error>
    hmac(
        const std::vector<std::uint8_t>& data,
        const crypto_key& key,
        hash_algorithm algorithm) = 0;

    // === Key Derivation ===

    /**
     * @brief Derive key using PBKDF2
     * @param password Password for derivation
     * @param salt Random salt
     * @param iterations Number of iterations
     * @param key_length Desired key length in bytes
     * @param hash_alg Hash algorithm for PBKDF2
     * @return Derived key or error
     */
    virtual ss::core::result<crypto_key, crypto_error>
    derive_key_pbkdf2(
        const std::string& password,
        const std::vector<std::uint8_t>& salt,
        std::uint32_t iterations,
        std::size_t key_length,
        hash_algorithm hash_alg = hash_algorithm::sha256) = 0;

    /**
     * @brief Derive key using HKDF
     * @param input_key_material Input key material
     * @param salt Optional salt
     * @param info Optional context information
     * @param key_length Desired key length in bytes
     * @param hash_alg Hash algorithm for HKDF
     * @return Derived key or error
     */
    virtual ss::core::result<crypto_key, crypto_error>
    derive_key_hkdf(
        const crypto_key& input_key_material,
        const std::vector<std::uint8_t>& salt,
        const std::vector<std::uint8_t>& info,
        std::size_t key_length,
        hash_algorithm hash_alg = hash_algorithm::sha256) = 0;

    // === Random Number Generation ===

    /**
     * @brief Generate cryptographically secure random bytes
     * @param length Number of random bytes to generate
     * @return Random bytes or error
     */
    virtual ss::core::result<std::vector<std::uint8_t>, crypto_error>
    random_bytes(std::size_t length) = 0;

    // === Utility Functions ===

    /**
     * @brief Check if algorithm is supported
     * @param algorithm Algorithm identifier string
     * @return true if supported, false otherwise
     */
    virtual bool is_algorithm_supported(const std::string& algorithm) const noexcept = 0;

    /**
     * @brief Get recommended key size for algorithm
     * @param algorithm Algorithm identifier string
     * @return Key size in bytes, 0 if unknown/variable
     */
    virtual std::size_t get_key_size(const std::string& algorithm) const noexcept = 0;

    /**
     * @brief Get algorithm strength level (in bits)
     * @param algorithm Algorithm identifier string
     * @return Security strength in bits
     */
    virtual std::uint32_t get_security_level(const std::string& algorithm) const noexcept = 0;

    /**
     * @brief Constant-time memory comparison
     * @param a First buffer
     * @param b Second buffer
     * @return true if buffers are equal, false otherwise
     */
    virtual bool constant_time_compare(
        const std::vector<std::uint8_t>& a,
        const std::vector<std::uint8_t>& b) const noexcept = 0;

protected:
    /**
     * @brief Protected default constructor
     */
    i_crypto() = default;

    /**
     * @brief Protected copy constructor (deleted)
     */
    i_crypto(const i_crypto&) = delete;

    /**
     * @brief Protected move constructor (deleted)
     */
    i_crypto(i_crypto&&) = delete;

    /**
     * @brief Protected copy assignment (deleted)
     */
    i_crypto& operator=(const i_crypto&) = delete;

    /**
     * @brief Protected move assignment (deleted)
     */
    i_crypto& operator=(i_crypto&&) = delete;
};

/**
 * @brief Smart pointer type for crypto instances
 */
using crypto_ptr = std::shared_ptr<i_crypto>;

/**
 * @brief Weak pointer type for crypto instances
 */
using crypto_weak_ptr = std::weak_ptr<i_crypto>;

} // namespace ss::security