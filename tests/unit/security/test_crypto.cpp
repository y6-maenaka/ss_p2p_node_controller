#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../../../include/ss_p2p/security/i_crypto.hpp"
#include "../../../src/ss_p2p/security/impl/openssl_crypto.cpp"

using namespace ss::security;
using namespace ss::core;

/**
 * @brief Test fixture for cryptography tests
 */
class CryptoTest : public ::testing::Test {
protected:
    void SetUp() override {
        crypto_provider = std::make_shared<openssl_crypto>();
        auto start_result = crypto_provider->start().get();
        ASSERT_TRUE(start_result.is_ok());
    }

    void TearDown() override {
        if (crypto_provider) {
            auto stop_result = crypto_provider->stop().get();
            ASSERT_TRUE(stop_result.is_ok());
        }
    }

    crypto_ptr crypto_provider;
};

// === Symmetric Encryption Tests ===

TEST_F(CryptoTest, GenerateSymmetricKey_AES256GCM_Success) {
    auto result = crypto_provider->generate_symmetric_key(symmetric_algorithm::aes_256_gcm);
    
    ASSERT_TRUE(result.is_ok());
    auto key = std::move(result.value());
    
    EXPECT_EQ(key.type(), key_type::symmetric);
    EXPECT_EQ(key.algorithm(), "AES-256-GCM");
    EXPECT_EQ(key.size(), 32); // 256 bits
    EXPECT_TRUE(key.is_valid());
}

TEST_F(CryptoTest, GenerateSymmetricKey_ChaCha20Poly1305_Success) {
    auto result = crypto_provider->generate_symmetric_key(symmetric_algorithm::chacha20_poly1305);
    
    ASSERT_TRUE(result.is_ok());
    auto key = std::move(result.value());
    
    EXPECT_EQ(key.type(), key_type::symmetric);
    EXPECT_EQ(key.algorithm(), "ChaCha20-Poly1305");
    EXPECT_EQ(key.size(), 32); // 256 bits
    EXPECT_TRUE(key.is_valid());
}

TEST_F(CryptoTest, SymmetricEncryptDecrypt_AES256GCM_Success) {
    // Generate key
    auto key_result = crypto_provider->generate_symmetric_key(symmetric_algorithm::aes_256_gcm);
    ASSERT_TRUE(key_result.is_ok());
    auto key = std::move(key_result.value());

    // Test data
    std::vector<std::uint8_t> plaintext = {
        0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x57, 0x6f, 0x72, 0x6c, 0x64 // "Hello World"
    };
    std::vector<std::uint8_t> aad = {0x01, 0x02, 0x03, 0x04};

    // Encrypt
    auto encrypt_result = crypto_provider->symmetric_encrypt(plaintext, key, aad);
    ASSERT_TRUE(encrypt_result.is_ok());
    auto encrypted = std::move(encrypt_result.value());

    EXPECT_FALSE(encrypted.ciphertext.empty());
    EXPECT_FALSE(encrypted.tag.empty());
    EXPECT_FALSE(encrypted.iv.empty());
    EXPECT_EQ(encrypted.aad, aad);
    EXPECT_EQ(encrypted.algorithm, "AES-256-GCM");

    // Decrypt
    auto decrypt_result = crypto_provider->symmetric_decrypt(encrypted, key);
    ASSERT_TRUE(decrypt_result.is_ok());
    auto decrypted = std::move(decrypt_result.value());

    EXPECT_EQ(decrypted, plaintext);
}

TEST_F(CryptoTest, SymmetricDecrypt_InvalidTag_Failure) {
    // Generate key and encrypt data
    auto key_result = crypto_provider->generate_symmetric_key(symmetric_algorithm::aes_256_gcm);
    ASSERT_TRUE(key_result.is_ok());
    auto key = std::move(key_result.value());

    std::vector<std::uint8_t> plaintext = {0x48, 0x65, 0x6c, 0x6c, 0x6f};
    auto encrypt_result = crypto_provider->symmetric_encrypt(plaintext, key);
    ASSERT_TRUE(encrypt_result.is_ok());
    auto encrypted = std::move(encrypt_result.value());

    // Corrupt the authentication tag
    encrypted.tag[0] ^= 0xFF;

    // Attempt to decrypt
    auto decrypt_result = crypto_provider->symmetric_decrypt(encrypted, key);
    EXPECT_TRUE(decrypt_result.is_err());
    EXPECT_EQ(decrypt_result.error(), crypto_error::verification_failed);
}

// === Asymmetric Encryption Tests ===

TEST_F(CryptoTest, GenerateKeyPair_RSA_Success) {
    auto result = crypto_provider->generate_key_pair(asymmetric_algorithm::rsa_oaep);
    
    ASSERT_TRUE(result.is_ok());
    auto key_pair = std::move(result.value());
    
    EXPECT_EQ(key_pair.public_key.type(), key_type::public_key);
    EXPECT_EQ(key_pair.private_key.type(), key_type::private_key);
    EXPECT_EQ(key_pair.public_key.algorithm(), "RSA-OAEP");
    EXPECT_EQ(key_pair.private_key.algorithm(), "RSA-OAEP");
    EXPECT_TRUE(key_pair.public_key.is_valid());
    EXPECT_TRUE(key_pair.private_key.is_valid());
}

TEST_F(CryptoTest, GenerateKeyPair_X25519_Success) {
    auto result = crypto_provider->generate_key_pair(asymmetric_algorithm::x25519_chacha20_poly1305);
    
    ASSERT_TRUE(result.is_ok());
    auto key_pair = std::move(result.value());
    
    EXPECT_EQ(key_pair.public_key.type(), key_type::public_key);
    EXPECT_EQ(key_pair.private_key.type(), key_type::private_key);
    EXPECT_EQ(key_pair.public_key.algorithm(), "X25519");
    EXPECT_EQ(key_pair.private_key.algorithm(), "X25519");
    EXPECT_EQ(key_pair.public_key.size(), 32);
    EXPECT_EQ(key_pair.private_key.size(), 32);
}

TEST_F(CryptoTest, AsymmetricEncryptDecrypt_RSA_Success) {
    // Generate key pair
    auto key_pair_result = crypto_provider->generate_key_pair(asymmetric_algorithm::rsa_oaep);
    ASSERT_TRUE(key_pair_result.is_ok());
    auto key_pair = std::move(key_pair_result.value());

    // Test data (RSA has size limitations)
    std::vector<std::uint8_t> plaintext = {
        0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x57, 0x6f, 0x72, 0x6c, 0x64
    };

    // Encrypt with public key
    auto encrypt_result = crypto_provider->asymmetric_encrypt(plaintext, key_pair.public_key);
    ASSERT_TRUE(encrypt_result.is_ok());
    auto ciphertext = std::move(encrypt_result.value());

    EXPECT_FALSE(ciphertext.empty());
    EXPECT_NE(ciphertext, plaintext);

    // Decrypt with private key
    auto decrypt_result = crypto_provider->asymmetric_decrypt(ciphertext, key_pair.private_key);
    ASSERT_TRUE(decrypt_result.is_ok());
    auto decrypted = std::move(decrypt_result.value());

    EXPECT_EQ(decrypted, plaintext);
}

TEST_F(CryptoTest, AsymmetricDecrypt_WrongKey_Failure) {
    // Generate two key pairs
    auto key_pair1_result = crypto_provider->generate_key_pair(asymmetric_algorithm::rsa_oaep);
    ASSERT_TRUE(key_pair1_result.is_ok());
    auto key_pair1 = std::move(key_pair1_result.value());

    auto key_pair2_result = crypto_provider->generate_key_pair(asymmetric_algorithm::rsa_oaep);
    ASSERT_TRUE(key_pair2_result.is_ok());
    auto key_pair2 = std::move(key_pair2_result.value());

    std::vector<std::uint8_t> plaintext = {0x48, 0x65, 0x6c, 0x6c, 0x6f};

    // Encrypt with first public key
    auto encrypt_result = crypto_provider->asymmetric_encrypt(plaintext, key_pair1.public_key);
    ASSERT_TRUE(encrypt_result.is_ok());
    auto ciphertext = std::move(encrypt_result.value());

    // Try to decrypt with second private key
    auto decrypt_result = crypto_provider->asymmetric_decrypt(ciphertext, key_pair2.private_key);
    EXPECT_TRUE(decrypt_result.is_err());
    EXPECT_EQ(decrypt_result.error(), crypto_error::decryption_failed);
}

// === Digital Signature Tests ===

TEST_F(CryptoTest, GenerateSignatureKeyPair_Ed25519_Success) {
    auto result = crypto_provider->generate_signature_key_pair(signature_algorithm::ed25519);
    
    ASSERT_TRUE(result.is_ok());
    auto key_pair = std::move(result.value());
    
    EXPECT_EQ(key_pair.public_key.type(), key_type::public_key);
    EXPECT_EQ(key_pair.private_key.type(), key_type::private_key);
    EXPECT_EQ(key_pair.public_key.algorithm(), "Ed25519");
    EXPECT_EQ(key_pair.private_key.algorithm(), "Ed25519");
    EXPECT_EQ(key_pair.public_key.size(), 32);
    EXPECT_EQ(key_pair.private_key.size(), 64);
}

TEST_F(CryptoTest, SignVerify_Ed25519_Success) {
    // Generate key pair
    auto key_pair_result = crypto_provider->generate_signature_key_pair(signature_algorithm::ed25519);
    ASSERT_TRUE(key_pair_result.is_ok());
    auto key_pair = std::move(key_pair_result.value());

    // Test data
    std::vector<std::uint8_t> message = {
        0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x61, 0x20, 0x74, 0x65, 0x73, 0x74
    };

    // Sign message
    auto sign_result = crypto_provider->sign(message, key_pair.private_key);
    ASSERT_TRUE(sign_result.is_ok());
    auto signature = std::move(sign_result.value());

    EXPECT_FALSE(signature.signature.empty());
    EXPECT_EQ(signature.algorithm, "Ed25519");
    EXPECT_EQ(signature.signature.size(), 64);

    // Verify signature
    auto verify_result = crypto_provider->verify(message, signature, key_pair.public_key);
    ASSERT_TRUE(verify_result.is_ok());
    EXPECT_TRUE(verify_result.value());
}

TEST_F(CryptoTest, Verify_InvalidSignature_Failure) {
    // Generate key pair
    auto key_pair_result = crypto_provider->generate_signature_key_pair(signature_algorithm::ed25519);
    ASSERT_TRUE(key_pair_result.is_ok());
    auto key_pair = std::move(key_pair_result.value());

    std::vector<std::uint8_t> message = {0x54, 0x65, 0x73, 0x74};
    
    // Sign message
    auto sign_result = crypto_provider->sign(message, key_pair.private_key);
    ASSERT_TRUE(sign_result.is_ok());
    auto signature = std::move(sign_result.value());

    // Corrupt signature
    signature.signature[0] ^= 0xFF;

    // Verify corrupted signature
    auto verify_result = crypto_provider->verify(message, signature, key_pair.public_key);
    ASSERT_TRUE(verify_result.is_ok());
    EXPECT_FALSE(verify_result.value());
}

TEST_F(CryptoTest, Verify_ModifiedMessage_Failure) {
    // Generate key pair
    auto key_pair_result = crypto_provider->generate_signature_key_pair(signature_algorithm::ed25519);
    ASSERT_TRUE(key_pair_result.is_ok());
    auto key_pair = std::move(key_pair_result.value());

    std::vector<std::uint8_t> message = {0x54, 0x65, 0x73, 0x74};
    
    // Sign message
    auto sign_result = crypto_provider->sign(message, key_pair.private_key);
    ASSERT_TRUE(sign_result.is_ok());
    auto signature = std::move(sign_result.value());

    // Modify message
    message[0] ^= 0xFF;

    // Verify with modified message
    auto verify_result = crypto_provider->verify(message, signature, key_pair.public_key);
    ASSERT_TRUE(verify_result.is_ok());
    EXPECT_FALSE(verify_result.value());
}

// === Hash Function Tests ===

TEST_F(CryptoTest, Hash_SHA256_Success) {
    std::vector<std::uint8_t> data = {
        0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x57, 0x6f, 0x72, 0x6c, 0x64 // "Hello World"
    };

    auto result = crypto_provider->hash(data, hash_algorithm::sha256);
    ASSERT_TRUE(result.is_ok());
    auto digest = std::move(result.value());

    EXPECT_EQ(digest.size(), 32); // SHA-256 produces 32-byte digest
    
    // Known SHA-256 hash of "Hello World"
    std::vector<std::uint8_t> expected = {
        0xa5, 0x91, 0xa6, 0xd4, 0x0b, 0xf4, 0x20, 0x40, 0x4a, 0x01, 0x17, 0x33,
        0xcf, 0xb7, 0xb1, 0x90, 0xd6, 0x2c, 0x65, 0xbf, 0x0b, 0xcd, 0xa3, 0x2b,
        0x57, 0xb2, 0x77, 0xd9, 0xad, 0x9f, 0x14, 0x6e
    };
    
    EXPECT_EQ(digest, expected);
}

TEST_F(CryptoTest, Hash_EmptyData_Success) {
    std::vector<std::uint8_t> empty_data;

    auto result = crypto_provider->hash(empty_data, hash_algorithm::sha256);
    ASSERT_TRUE(result.is_ok());
    auto digest = std::move(result.value());

    EXPECT_EQ(digest.size(), 32);
    
    // Known SHA-256 hash of empty string
    std::vector<std::uint8_t> expected = {
        0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4, 0xc8,
        0x99, 0x6f, 0xb9, 0x24, 0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
        0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
    };
    
    EXPECT_EQ(digest, expected);
}

TEST_F(CryptoTest, HMAC_SHA256_Success) {
    // Generate HMAC key
    auto key_result = crypto_provider->generate_symmetric_key(symmetric_algorithm::aes_256_gcm);
    ASSERT_TRUE(key_result.is_ok());
    auto key = std::move(key_result.value());

    std::vector<std::uint8_t> data = {0x48, 0x65, 0x6c, 0x6c, 0x6f};

    auto result = crypto_provider->hmac(data, key, hash_algorithm::sha256);
    ASSERT_TRUE(result.is_ok());
    auto hmac_digest = std::move(result.value());

    EXPECT_EQ(hmac_digest.size(), 32); // HMAC-SHA256 produces 32-byte digest
    EXPECT_FALSE(hmac_digest.empty());
}

// === Key Derivation Tests ===

TEST_F(CryptoTest, DeriveKey_PBKDF2_Success) {
    std::string password = "test_password";
    std::vector<std::uint8_t> salt = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    std::uint32_t iterations = 1000;
    std::size_t key_length = 32;

    auto result = crypto_provider->derive_key_pbkdf2(password, salt, iterations, key_length);
    ASSERT_TRUE(result.is_ok());
    auto derived_key = std::move(result.value());

    EXPECT_EQ(derived_key.type(), key_type::kdf_key);
    EXPECT_EQ(derived_key.algorithm(), "PBKDF2");
    EXPECT_EQ(derived_key.size(), key_length);
    EXPECT_TRUE(derived_key.is_valid());
}

TEST_F(CryptoTest, DeriveKey_HKDF_Success) {
    // Generate input key material
    auto ikm_result = crypto_provider->generate_symmetric_key(symmetric_algorithm::aes_256_gcm);
    ASSERT_TRUE(ikm_result.is_ok());
    auto ikm = std::move(ikm_result.value());

    std::vector<std::uint8_t> salt = {0x01, 0x02, 0x03, 0x04};
    std::vector<std::uint8_t> info = {0x05, 0x06, 0x07, 0x08};
    std::size_t key_length = 48;

    auto result = crypto_provider->derive_key_hkdf(ikm, salt, info, key_length);
    ASSERT_TRUE(result.is_ok());
    auto derived_key = std::move(result.value());

    EXPECT_EQ(derived_key.type(), key_type::kdf_key);
    EXPECT_EQ(derived_key.algorithm(), "HKDF");
    EXPECT_EQ(derived_key.size(), key_length);
    EXPECT_TRUE(derived_key.is_valid());
}

// === Random Number Generation Tests ===

TEST_F(CryptoTest, RandomBytes_Success) {
    std::size_t length = 32;

    auto result = crypto_provider->random_bytes(length);
    ASSERT_TRUE(result.is_ok());
    auto random_data = std::move(result.value());

    EXPECT_EQ(random_data.size(), length);
    
    // Generate another set of random bytes and ensure they're different
    auto result2 = crypto_provider->random_bytes(length);
    ASSERT_TRUE(result2.is_ok());
    auto random_data2 = std::move(result2.value());

    EXPECT_NE(random_data, random_data2);
}

TEST_F(CryptoTest, RandomBytes_ZeroLength_Success) {
    auto result = crypto_provider->random_bytes(0);
    ASSERT_TRUE(result.is_ok());
    auto random_data = std::move(result.value());

    EXPECT_TRUE(random_data.empty());
}

// === Utility Function Tests ===

TEST_F(CryptoTest, IsAlgorithmSupported_KnownAlgorithms_True) {
    EXPECT_TRUE(crypto_provider->is_algorithm_supported("AES-256-GCM"));
    EXPECT_TRUE(crypto_provider->is_algorithm_supported("ChaCha20-Poly1305"));
    EXPECT_TRUE(crypto_provider->is_algorithm_supported("RSA-OAEP"));
    EXPECT_TRUE(crypto_provider->is_algorithm_supported("Ed25519"));
    EXPECT_TRUE(crypto_provider->is_algorithm_supported("SHA-256"));
}

TEST_F(CryptoTest, IsAlgorithmSupported_UnknownAlgorithm_False) {
    EXPECT_FALSE(crypto_provider->is_algorithm_supported("UnknownAlgorithm"));
    EXPECT_FALSE(crypto_provider->is_algorithm_supported(""));
}

TEST_F(CryptoTest, GetKeySize_KnownAlgorithms_CorrectSizes) {
    EXPECT_EQ(crypto_provider->get_key_size("AES-256-GCM"), 32);
    EXPECT_EQ(crypto_provider->get_key_size("AES-128-GCM"), 16);
    EXPECT_EQ(crypto_provider->get_key_size("Ed25519"), 32);
    EXPECT_EQ(crypto_provider->get_key_size("UnknownAlgorithm"), 0);
}

TEST_F(CryptoTest, GetSecurityLevel_KnownAlgorithms_CorrectLevels) {
    EXPECT_EQ(crypto_provider->get_security_level("AES-256-GCM"), 256);
    EXPECT_EQ(crypto_provider->get_security_level("AES-128-GCM"), 128);
    EXPECT_EQ(crypto_provider->get_security_level("Ed25519"), 128);
    EXPECT_EQ(crypto_provider->get_security_level("UnknownAlgorithm"), 0);
}

TEST_F(CryptoTest, ConstantTimeCompare_EqualBuffers_True) {
    std::vector<std::uint8_t> buffer1 = {0x01, 0x02, 0x03, 0x04};
    std::vector<std::uint8_t> buffer2 = {0x01, 0x02, 0x03, 0x04};

    EXPECT_TRUE(crypto_provider->constant_time_compare(buffer1, buffer2));
}

TEST_F(CryptoTest, ConstantTimeCompare_DifferentBuffers_False) {
    std::vector<std::uint8_t> buffer1 = {0x01, 0x02, 0x03, 0x04};
    std::vector<std::uint8_t> buffer2 = {0x01, 0x02, 0x03, 0x05};

    EXPECT_FALSE(crypto_provider->constant_time_compare(buffer1, buffer2));
}

TEST_F(CryptoTest, ConstantTimeCompare_DifferentSizes_False) {
    std::vector<std::uint8_t> buffer1 = {0x01, 0x02, 0x03, 0x04};
    std::vector<std::uint8_t> buffer2 = {0x01, 0x02, 0x03};

    EXPECT_FALSE(crypto_provider->constant_time_compare(buffer1, buffer2));
}

// === Secure Bytes Tests ===

TEST_F(CryptoTest, SecureBytes_Construction_Success) {
    secure_bytes bytes(32);
    EXPECT_EQ(bytes.size(), 32);
    EXPECT_FALSE(bytes.empty());
    EXPECT_NE(bytes.data(), nullptr);
}

TEST_F(CryptoTest, SecureBytes_CopyConstruction_Success) {
    secure_bytes original(16);
    std::fill(original.begin(), original.end(), 0xAB);

    secure_bytes copy(original);
    EXPECT_EQ(copy.size(), original.size());
    EXPECT_TRUE(std::equal(copy.begin(), copy.end(), original.begin()));
}

TEST_F(CryptoTest, SecureBytes_MoveConstruction_Success) {
    secure_bytes original(16);
    std::fill(original.begin(), original.end(), 0xCD);
    auto original_size = original.size();

    secure_bytes moved(std::move(original));
    EXPECT_EQ(moved.size(), original_size);
    EXPECT_TRUE(std::all_of(moved.begin(), moved.end(), [](auto byte) { return byte == 0xCD; }));
}

TEST_F(CryptoTest, SecureBytes_Clear_Success) {
    secure_bytes bytes(32);
    std::fill(bytes.begin(), bytes.end(), 0xFF);

    bytes.clear();
    EXPECT_TRUE(bytes.empty());
    EXPECT_EQ(bytes.size(), 0);
}

// === Crypto Key Tests ===

TEST_F(CryptoTest, CryptoKey_Construction_Success) {
    secure_bytes key_data(32);
    std::fill(key_data.begin(), key_data.end(), 0x42);

    crypto_key key(key_type::symmetric, "AES-256-GCM", std::move(key_data));
    
    EXPECT_EQ(key.type(), key_type::symmetric);
    EXPECT_EQ(key.algorithm(), "AES-256-GCM");
    EXPECT_EQ(key.size(), 32);
    EXPECT_TRUE(key.is_valid());
}

TEST_F(CryptoTest, CryptoKey_Fingerprint_Consistent) {
    secure_bytes key_data(32);
    std::fill(key_data.begin(), key_data.end(), 0x42);

    crypto_key key1(key_type::symmetric, "AES-256-GCM", secure_bytes(key_data));
    crypto_key key2(key_type::symmetric, "AES-256-GCM", std::move(key_data));
    
    std::string fingerprint1 = key1.fingerprint();
    std::string fingerprint2 = key2.fingerprint();
    
    EXPECT_FALSE(fingerprint1.empty());
    EXPECT_FALSE(fingerprint2.empty());
    EXPECT_EQ(fingerprint1, fingerprint2);
}

// === Error Handling Tests ===

TEST_F(CryptoTest, SymmetricEncrypt_InvalidKey_Failure) {
    // Create invalid key (wrong type)
    secure_bytes key_data(32);
    crypto_key invalid_key(key_type::public_key, "AES-256-GCM", std::move(key_data));

    std::vector<std::uint8_t> plaintext = {0x48, 0x65, 0x6c, 0x6c, 0x6f};

    auto result = crypto_provider->symmetric_encrypt(plaintext, invalid_key);
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error(), crypto_error::invalid_key);
}

TEST_F(CryptoTest, AsymmetricEncrypt_WrongKeyType_Failure) {
    // Generate symmetric key instead of asymmetric
    auto key_result = crypto_provider->generate_symmetric_key(symmetric_algorithm::aes_256_gcm);
    ASSERT_TRUE(key_result.is_ok());
    auto key = std::move(key_result.value());

    std::vector<std::uint8_t> plaintext = {0x48, 0x65, 0x6c, 0x6c, 0x6f};

    auto result = crypto_provider->asymmetric_encrypt(plaintext, key);
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error(), crypto_error::invalid_key);
}

// === Performance Tests ===

TEST_F(CryptoTest, DISABLED_Performance_SymmetricEncryption_Benchmark) {
    // Generate key
    auto key_result = crypto_provider->generate_symmetric_key(symmetric_algorithm::aes_256_gcm);
    ASSERT_TRUE(key_result.is_ok());
    auto key = std::move(key_result.value());

    // Large test data (1MB)
    std::vector<std::uint8_t> large_data(1024 * 1024, 0x42);

    auto start = std::chrono::high_resolution_clock::now();
    
    // Perform multiple encryptions
    const int iterations = 100;
    for (int i = 0; i < iterations; ++i) {
        auto result = crypto_provider->symmetric_encrypt(large_data, key);
        ASSERT_TRUE(result.is_ok());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Encrypted " << iterations << " x 1MB in " << duration.count() << "ms" << std::endl;
    std::cout << "Throughput: " << (iterations * 1024 * 1024 * 1000) / duration.count() / (1024 * 1024) << " MB/s" << std::endl;
}

// === Integration Tests ===

TEST_F(CryptoTest, Integration_FullWorkflow_Success) {
    // 1. Generate key pair for signatures
    auto sig_key_result = crypto_provider->generate_signature_key_pair(signature_algorithm::ed25519);
    ASSERT_TRUE(sig_key_result.is_ok());
    auto sig_keys = std::move(sig_key_result.value());

    // 2. Generate symmetric key for encryption
    auto sym_key_result = crypto_provider->generate_symmetric_key(symmetric_algorithm::aes_256_gcm);
    ASSERT_TRUE(sym_key_result.is_ok());
    auto sym_key = std::move(sym_key_result.value());

    // 3. Create test message
    std::vector<std::uint8_t> message = {
        0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x61, 0x20, 0x73, 0x65, 0x63, 0x72, 0x65, 0x74
    };

    // 4. Sign message
    auto sign_result = crypto_provider->sign(message, sig_keys.private_key);
    ASSERT_TRUE(sign_result.is_ok());
    auto signature = std::move(sign_result.value());

    // 5. Encrypt message
    auto encrypt_result = crypto_provider->symmetric_encrypt(message, sym_key);
    ASSERT_TRUE(encrypt_result.is_ok());
    auto encrypted = std::move(encrypt_result.value());

    // 6. Decrypt message
    auto decrypt_result = crypto_provider->symmetric_decrypt(encrypted, sym_key);
    ASSERT_TRUE(decrypt_result.is_ok());
    auto decrypted = std::move(decrypt_result.value());

    // 7. Verify signature
    auto verify_result = crypto_provider->verify(decrypted, signature, sig_keys.public_key);
    ASSERT_TRUE(verify_result.is_ok());
    EXPECT_TRUE(verify_result.value());

    // 8. Verify message integrity
    EXPECT_EQ(decrypted, message);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}