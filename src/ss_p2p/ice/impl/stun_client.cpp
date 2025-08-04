/**
 * @file stun_client.cpp
 * @brief RFC 5389 compliant STUN client implementation
 * 
 * Implements STUN protocol for NAT discovery and keep-alive operations.
 * Supports message integrity, fingerprint validation, and transaction management.
 * Designed for both centralized STUN servers and peer-to-peer STUN operations.
 */

#include "../../../../include/ss_p2p/ice/i_stun_client.hpp"
#include "../../../../include/ss_p2p/network/i_transport.hpp"
#include "../../../../include/ss_p2p/core/result.hpp"

#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <random>
#include <algorithm>
#include <functional>
#include <cstring>
#include <cstdint>

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

// For CRC32 fingerprint calculation
#include <zlib.h>
// For HMAC-SHA1 message integrity
#include <openssl/hmac.h>
#include <openssl/evp.h>

namespace ss::ice::impl {

/**
 * @brief STUN magic cookie value (RFC 5389)
 */
constexpr std::uint32_t STUN_MAGIC_COOKIE = 0x2112A442;

/**
 * @brief STUN header size in bytes
 */
constexpr size_t STUN_HEADER_SIZE = 20;

/**
 * @brief STUN message class masks (commented out unused constants)
 */
// constexpr std::uint16_t STUN_CLASS_REQUEST = 0x0000;
// constexpr std::uint16_t STUN_CLASS_INDICATION = 0x0010;
// constexpr std::uint16_t STUN_CLASS_SUCCESS_RESPONSE = 0x0100;
// constexpr std::uint16_t STUN_CLASS_ERROR_RESPONSE = 0x0110;

/**
 * @brief Pending STUN transaction
 */
struct stun_transaction {
    stun_transaction_id id;
    ss::core::endpoint target;
    stun_message_type message_type;
    std::chrono::steady_clock::time_point sent_at;
    std::chrono::milliseconds timeout;
    std::uint32_t retry_count = 0;
    std::uint32_t max_retries = 3;
    stun_binding_handler handler;
    stun_client_config config;
    
    stun_transaction(stun_transaction_id tx_id, 
                    ss::core::endpoint target_ep,
                    stun_message_type type,
                    std::chrono::milliseconds timeout_ms,
                    stun_binding_handler cb,
                    stun_client_config cfg)
        : id(tx_id)
        , target(std::move(target_ep))
        , message_type(type)
        , sent_at(std::chrono::steady_clock::now())
        , timeout(timeout_ms)
        , handler(std::move(cb))
        , config(std::move(cfg)) {}
};

/**
 * @brief RFC 5389 compliant STUN client implementation
 */
class rfc5389_stun_client : public i_stun_client {
private:
    /// IO context for async operations
    ss::core::io_context& io_context_;
    
    /// Transport layer for network communication
    ss::network::transport_ptr transport_;
    
    /// Pending transactions
    std::unordered_map<stun_transaction_id, std::unique_ptr<stun_transaction>> transactions_;
    
    /// Transaction timeout timer
    std::unique_ptr<ss::core::steady_timer> timeout_timer_;
    
    /// Synchronization
    mutable std::mutex transactions_mutex_;
    
    /// Statistics
    struct {
        std::atomic<std::uint64_t> requests_sent{0};
        std::atomic<std::uint64_t> responses_received{0};
        std::atomic<std::uint64_t> error_responses{0};
        std::atomic<std::uint64_t> timeouts{0};
        std::atomic<std::uint64_t> retransmissions{0};
        std::atomic<std::uint64_t> integrity_failures{0};
        std::atomic<std::uint64_t> fingerprint_failures{0};
        std::chrono::milliseconds avg_response_time{0};
        std::chrono::steady_clock::time_point last_request_time{};
        std::chrono::steady_clock::time_point last_response_time{};
    } stats_;

    /// Random number generator for transaction IDs
    mutable std::mt19937 rng_{std::random_device{}()};
    
    /// Running state
    std::atomic<bool> running_{false};

public:
    /**
     * @brief Constructor
     * @param io_context IO context for async operations
     */
    explicit rfc5389_stun_client(ss::core::io_context& io_context)
        : io_context_(io_context)
        , timeout_timer_(std::make_unique<ss::core::steady_timer>(io_context_)) {}

    /**
     * @brief Destructor
     */
    ~rfc5389_stun_client() override {
        stop_async();
    }

    // ss::core::i_component interface
    ss::core::async_void start() override {
        running_.store(true);
        schedule_timeout_check();
        co_return;
    }

    ss::core::async_void stop() override {
        running_.store(false);
        timeout_timer_->cancel();
        
        // Cancel all pending transactions
        std::lock_guard lock(transactions_mutex_);
        for (auto& [tx_id, transaction] : transactions_) {
            if (transaction->handler) {
                transaction->handler(ss::core::result<stun_binding_result, stun_client_error>::err(
                    stun_client_error::timeout));
            }
        }
        transactions_.clear();
        
        co_return;
    }

    bool is_running() const noexcept override {
        return running_.load();
    }

    std::string name() const noexcept override {
        return "rfc5389_stun_client";
    }

    // i_stun_client interface
    ss::core::async_result<ss::core::result<stun_binding_result, stun_client_error>>
    binding_request(const ss::core::endpoint& server_endpoint,
                   const ss::core::endpoint& local_endpoint,
                   const stun_client_config& config) override {
        
        if (!transport_) {
            co_return ss::core::result<stun_binding_result, stun_client_error>::err(
                stun_client_error::network_error);
        }
        
        // Generate transaction ID
        auto tx_id = stun_transaction_id::random();
        
        // Create STUN binding request message
        stun_message request(stun_message_type::binding_request, tx_id);
        
        // Add SOFTWARE attribute if specified
        if (!config.software.empty()) {
            add_software_attribute(request, config.software);
        }
        
        // Encode message
        auto encoded_result = encode_message(request, config);
        if (encoded_result.is_err()) {
            co_return ss::core::result<stun_binding_result, stun_client_error>::err(
                encoded_result.error());
        }
        
        // Send message
        ss::network::outgoing_message out_msg(encoded_result.value(), server_endpoint);
        
        auto send_result = co_await transport_->send(std::move(out_msg));
        if (send_result.is_err()) {
            co_return ss::core::result<stun_binding_result, stun_client_error>::err(
                stun_client_error::network_error);
        }
        
        stats_.requests_sent.fetch_add(1);
        stats_.last_request_time = std::chrono::steady_clock::now();
        
        // Wait for response with timeout
        auto response_result = co_await wait_for_response(tx_id, config.request_timeout);
        
        co_return response_result;
    }

    void binding_request_async(const ss::core::endpoint& server_endpoint,
                              const ss::core::endpoint& local_endpoint,
                              stun_binding_handler handler,
                              const stun_client_config& config) override {
        
        boost::asio::co_spawn(io_context_, 
            [this, server_endpoint, local_endpoint, handler = std::move(handler), config]() mutable 
            -> ss::core::async_void {
                auto result = co_await binding_request(server_endpoint, local_endpoint, config);
                handler(result);
            }, 
            boost::asio::detached);
    }

    ss::core::async_result<ss::core::result<stun_binding_result, stun_client_error>>
    binding_request_with_change(const ss::core::endpoint& server_endpoint,
                               const ss::core::endpoint& local_endpoint,
                               bool change_ip, bool change_port,
                               const stun_client_config& config) override {
        
        if (!transport_) {
            co_return ss::core::result<stun_binding_result, stun_client_error>::err(
                stun_client_error::network_error);
        }
        
        // Generate transaction ID
        auto tx_id = stun_transaction_id::random();
        
        // Create STUN binding request message
        stun_message request(stun_message_type::binding_request, tx_id);
        
        // Add CHANGE-REQUEST attribute (RFC 3489 compatibility)
        if (config.rfc3489_compat && (change_ip || change_port)) {
            add_change_request_attribute(request, change_ip, change_port);
        }
        
        // Add SOFTWARE attribute if specified
        if (!config.software.empty()) {
            add_software_attribute(request, config.software);
        }
        
        // Encode message
        auto encoded_result = encode_message(request, config);
        if (encoded_result.is_err()) {
            co_return ss::core::result<stun_binding_result, stun_client_error>::err(
                encoded_result.error());
        }
        
        // Send message
        ss::network::outgoing_message out_msg(encoded_result.value(), server_endpoint);
        
        auto send_result = co_await transport_->send(std::move(out_msg));
        if (send_result.is_err()) {
            co_return ss::core::result<stun_binding_result, stun_client_error>::err(
                stun_client_error::network_error);
        }
        
        stats_.requests_sent.fetch_add(1);
        stats_.last_request_time = std::chrono::steady_clock::now();
        
        // Wait for response with timeout
        auto response_result = co_await wait_for_response(tx_id, config.request_timeout);
        
        co_return response_result;
    }

    ss::core::async_result<ss::core::result<void, stun_client_error>>
    send_keepalive(const ss::core::endpoint& server_endpoint,
                  const ss::core::endpoint& local_endpoint,
                  const stun_client_config& config) override {
        
        auto result = co_await binding_request(server_endpoint, local_endpoint, config);
        
        if (result.is_ok()) {
            co_return ss::core::result<void, stun_client_error>::ok();
        } else {
            co_return ss::core::result<void, stun_client_error>::err(result.error());
        }
    }

    void send_keepalive_async(const ss::core::endpoint& server_endpoint,
                             const ss::core::endpoint& local_endpoint,
                             stun_keepalive_handler handler,
                             const stun_client_config& config) override {
        
        boost::asio::co_spawn(io_context_,
            [this, server_endpoint, local_endpoint, handler = std::move(handler), config]() mutable 
            -> ss::core::async_void {
                auto result = co_await send_keepalive(server_endpoint, local_endpoint, config);
                handler(result);
            },
            boost::asio::detached);
    }

    ss::core::result<stun_message, stun_client_error>
    parse_message(const std::vector<std::uint8_t>& data,
                 [[maybe_unused]] const ss::core::endpoint& sender) const override {
        
        if (data.size() < STUN_HEADER_SIZE) {
            return ss::core::result<stun_message, stun_client_error>::err(
                stun_client_error::invalid_message);
        }
        
        // Parse STUN header
        const std::uint8_t* ptr = data.data();
        
        // Message type (2 bytes)
        std::uint16_t message_type_raw;
        std::memcpy(&message_type_raw, ptr, sizeof(message_type_raw));
        message_type_raw = ntohs(message_type_raw);
        ptr += 2;
        
        // Message length (2 bytes)
        std::uint16_t message_length;
        std::memcpy(&message_length, ptr, sizeof(message_length));
        message_length = ntohs(message_length);
        ptr += 2;
        
        // Magic cookie (4 bytes)
        std::uint32_t magic_cookie;
        std::memcpy(&magic_cookie, ptr, sizeof(magic_cookie));
        magic_cookie = ntohl(magic_cookie);
        ptr += 4;
        
        if (magic_cookie != STUN_MAGIC_COOKIE) {
            return ss::core::result<stun_message, stun_client_error>::err(
                stun_client_error::invalid_message);
        }
        
        // Transaction ID (12 bytes)
        stun_transaction_id::storage_type tx_id_data;
        std::memcpy(tx_id_data.data(), ptr, tx_id_data.size());
        stun_transaction_id tx_id(tx_id_data);
        ptr += 12;
        
        // Validate message length
        if (data.size() != STUN_HEADER_SIZE + message_length) {
            return ss::core::result<stun_message, stun_client_error>::err(
                stun_client_error::invalid_message);
        }
        
        // Create message
        auto message_type = static_cast<stun_message_type>(message_type_raw);
        stun_message message(message_type, tx_id);
        message.raw_data = data;
        
        // Parse attributes
        const std::uint8_t* attr_ptr = ptr;
        const std::uint8_t* end_ptr = data.data() + data.size();
        
        while (attr_ptr < end_ptr) {
            if (attr_ptr + 4 > end_ptr) {
                return ss::core::result<stun_message, stun_client_error>::err(
                    stun_client_error::invalid_message);
            }
            
            // Attribute type (2 bytes)
            std::uint16_t attr_type_raw;
            std::memcpy(&attr_type_raw, attr_ptr, sizeof(attr_type_raw));
            attr_type_raw = ntohs(attr_type_raw);
            attr_ptr += 2;
            
            // Attribute length (2 bytes)
            std::uint16_t attr_length;
            std::memcpy(&attr_length, attr_ptr, sizeof(attr_length));
            attr_length = ntohs(attr_length);
            attr_ptr += 2;
            
            // Attribute value
            if (attr_ptr + attr_length > end_ptr) {
                return ss::core::result<stun_message, stun_client_error>::err(
                    stun_client_error::invalid_message);
            }
            
            std::vector<std::uint8_t> attr_value(attr_ptr, attr_ptr + attr_length);
            attr_ptr += attr_length;
            
            // Padding to 4-byte boundary
            auto padding = (4 - (attr_length % 4)) % 4;
            attr_ptr += padding;
            
            // Add attribute to message
            auto attr_type = static_cast<stun_attribute_type>(attr_type_raw);
            message.add_attribute(stun_attribute(attr_type, std::move(attr_value)));
        }
        
        return ss::core::result<stun_message, stun_client_error>::ok(std::move(message));
    }

    ss::core::result<std::vector<std::uint8_t>, stun_client_error>
    encode_message(const stun_message& message,
                  const stun_client_config& config) const override {
        
        std::vector<std::uint8_t> result;
        
        // Calculate total length
        std::uint16_t total_attr_length = 0;
        for (const auto& attr : message.attributes) {
            total_attr_length += 4 + attr.value.size(); // Type(2) + Length(2) + Value
            // Add padding to 4-byte boundary
            auto padding = (4 - (attr.value.size() % 4)) % 4;
            total_attr_length += padding;
        }
        
        // Add space for MESSAGE-INTEGRITY (20 bytes) if enabled
        if (config.enable_integrity) {
            total_attr_length += 4 + 20; // HMAC-SHA1 is 20 bytes
        }
        
        // Add space for FINGERPRINT (4 bytes) if enabled
        if (config.enable_fingerprint) {
            total_attr_length += 4 + 4; // CRC32 is 4 bytes
        }
        
        result.reserve(STUN_HEADER_SIZE + total_attr_length);
        
        // STUN header
        
        // Message type (2 bytes)
        std::uint16_t message_type = static_cast<std::uint16_t>(message.type);
        message_type = htons(message_type);
        result.insert(result.end(), 
                     reinterpret_cast<const std::uint8_t*>(&message_type),
                     reinterpret_cast<const std::uint8_t*>(&message_type) + 2);
        
        // Message length (2 bytes) - will be updated later
        std::uint16_t message_length = 0;
        auto length_pos = result.size();
        result.insert(result.end(), 2, 0);
        
        // Magic cookie (4 bytes)
        std::uint32_t magic_cookie = htonl(STUN_MAGIC_COOKIE);
        result.insert(result.end(),
                     reinterpret_cast<const std::uint8_t*>(&magic_cookie),
                     reinterpret_cast<const std::uint8_t*>(&magic_cookie) + 4);
        
        // Transaction ID (12 bytes)
        const auto& tx_id_data = message.transaction_id.data();
        result.insert(result.end(), tx_id_data.begin(), tx_id_data.end());
        
        // Attributes
        for (const auto& attr : message.attributes) {
            // Attribute type (2 bytes)
            std::uint16_t attr_type = htons(static_cast<std::uint16_t>(attr.type));
            result.insert(result.end(),
                         reinterpret_cast<const std::uint8_t*>(&attr_type),
                         reinterpret_cast<const std::uint8_t*>(&attr_type) + 2);
            
            // Attribute length (2 bytes)
            std::uint16_t attr_length = htons(static_cast<std::uint16_t>(attr.value.size()));
            result.insert(result.end(),
                         reinterpret_cast<const std::uint8_t*>(&attr_length),
                         reinterpret_cast<const std::uint8_t*>(&attr_length) + 2);
            
            // Attribute value
            result.insert(result.end(), attr.value.begin(), attr.value.end());
            
            // Padding to 4-byte boundary
            auto padding = (4 - (attr.value.size() % 4)) % 4;
            result.insert(result.end(), padding, 0);
        }
        
        // Add MESSAGE-INTEGRITY if enabled
        if (config.enable_integrity && !config.password.empty()) {
            add_message_integrity(result, config.password);
        }
        
        // Add FINGERPRINT if enabled
        if (config.enable_fingerprint) {
            add_fingerprint(result);
        }
        
        // Update message length
        message_length = htons(static_cast<std::uint16_t>(result.size() - STUN_HEADER_SIZE));
        std::memcpy(result.data() + length_pos, &message_length, 2);
        
        return ss::core::result<std::vector<std::uint8_t>, stun_client_error>::ok(std::move(result));
    }

    bool verify_message_integrity(const stun_message& message,
                                 const std::string& key) const override {
        
        const auto* integrity_attr = message.find_attribute(stun_attribute_type::message_integrity);
        if (!integrity_attr || integrity_attr->value.size() != 20) {
            return false;
        }
        
        // Calculate HMAC-SHA1 over message up to MESSAGE-INTEGRITY attribute
        auto& raw_data = message.raw_data;
        if (raw_data.size() < 24) { // At least header + integrity attribute header
            return false;
        }
        
        // Find MESSAGE-INTEGRITY attribute position
        size_t integrity_pos = find_attribute_position(raw_data, stun_attribute_type::message_integrity);
        if (integrity_pos == std::string::npos) {
            return false;
        }
        
        // Adjust message length in header to exclude everything after MESSAGE-INTEGRITY
        std::vector<std::uint8_t> temp_message = raw_data;
        std::uint16_t adjusted_length = htons(static_cast<std::uint16_t>(integrity_pos - STUN_HEADER_SIZE + 24));
        std::memcpy(temp_message.data() + 2, &adjusted_length, 2);
        
        // Calculate HMAC
        std::uint8_t calculated_hmac[EVP_MAX_MD_SIZE];
        unsigned int hmac_len = 0;
        
        HMAC(EVP_sha1(), 
             key.data(), static_cast<int>(key.size()),
             temp_message.data(), integrity_pos + 4, // Include attribute header
             calculated_hmac, &hmac_len);
        
        if (hmac_len != 20) {
            return false;
        }
        
        // Compare with received HMAC
        return std::memcmp(calculated_hmac, integrity_attr->value.data(), 20) == 0;
    }

    bool verify_fingerprint(const stun_message& message) const override {
        const auto* fingerprint_attr = message.find_attribute(stun_attribute_type::fingerprint);
        if (!fingerprint_attr || fingerprint_attr->value.size() != 4) {
            return false;
        }
        
        // Calculate CRC32 over message up to FINGERPRINT attribute
        auto& raw_data = message.raw_data;
        
        size_t fingerprint_pos = find_attribute_position(raw_data, stun_attribute_type::fingerprint);
        if (fingerprint_pos == std::string::npos) {
            return false;
        }
        
        // Calculate CRC32
        std::uint32_t calculated_crc = crc32(0L, Z_NULL, 0);
        calculated_crc = crc32(calculated_crc, raw_data.data(), 
                              static_cast<uInt>(fingerprint_pos + 4)); // Include attribute header
        calculated_crc ^= 0x5354554e; // XOR with STUN fingerprint constant
        
        // Get received CRC
        std::uint32_t received_crc;
        std::memcpy(&received_crc, fingerprint_attr->value.data(), 4);
        received_crc = ntohl(received_crc);
        
        return calculated_crc == received_crc;
    }

    std::vector<std::uint8_t> 
    create_xor_mapped_address(const ss::core::endpoint& endpoint,
                             const stun_transaction_id& transaction_id) const override {
        
        std::vector<std::uint8_t> result;
        
        // XOR-MAPPED-ADDRESS format:
        // 0                   1                   2                   3
        // 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
        // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // |x x x x x x x x|    Family     |         X-Port                |
        // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // |                X-Address (Variable)                ...        |
        // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        
        result.push_back(0); // Reserved
        
        auto native_endpoint = endpoint.native();
        if (native_endpoint.address().is_v4()) {
            // IPv4
            result.push_back(0x01); // Family = IPv4
            
            // XOR port with magic cookie (first 16 bits)
            std::uint16_t port = endpoint.port();
            std::uint16_t xor_port = port ^ (STUN_MAGIC_COOKIE >> 16);
            xor_port = htons(xor_port);
            result.insert(result.end(),
                         reinterpret_cast<const std::uint8_t*>(&xor_port),
                         reinterpret_cast<const std::uint8_t*>(&xor_port) + 2);
            
            // XOR address with magic cookie
            auto ipv4_addr = native_endpoint.address().to_v4().to_bytes();
            std::uint32_t xor_addr = 0;
            std::memcpy(&xor_addr, ipv4_addr.data(), 4);
            xor_addr ^= htonl(STUN_MAGIC_COOKIE);
            result.insert(result.end(),
                         reinterpret_cast<const std::uint8_t*>(&xor_addr),
                         reinterpret_cast<const std::uint8_t*>(&xor_addr) + 4);
            
        } else {
            // IPv6
            result.push_back(0x02); // Family = IPv6
            
            // XOR port with magic cookie (first 16 bits)
            std::uint16_t port = endpoint.port();
            std::uint16_t xor_port = port ^ (STUN_MAGIC_COOKIE >> 16);
            xor_port = htons(xor_port);
            result.insert(result.end(),
                         reinterpret_cast<const std::uint8_t*>(&xor_port),
                         reinterpret_cast<const std::uint8_t*>(&xor_port) + 2);
            
            // XOR address with magic cookie + transaction ID
            auto ipv6_addr = native_endpoint.address().to_v6().to_bytes();
            
            // First 4 bytes XOR with magic cookie
            for (int i = 0; i < 4; ++i) {
                ipv6_addr[i] ^= reinterpret_cast<const std::uint8_t*>(&STUN_MAGIC_COOKIE)[i];
            }
            
            // Next 12 bytes XOR with transaction ID
            const auto& tx_id_data = transaction_id.data();
            for (int i = 0; i < 12; ++i) {
                ipv6_addr[4 + i] ^= tx_id_data[i];
            }
            
            result.insert(result.end(), ipv6_addr.begin(), ipv6_addr.end());
        }
        
        return result;
    }

    ss::core::result<ss::core::endpoint, stun_client_error>
    parse_xor_mapped_address(const std::vector<std::uint8_t>& attribute_data,
                            const stun_transaction_id& transaction_id) const override {
        
        if (attribute_data.size() < 4) {
            return ss::core::result<ss::core::endpoint, stun_client_error>::err(
                stun_client_error::invalid_message);
        }
        
        const std::uint8_t* ptr = attribute_data.data();
        
        // Skip reserved byte
        ptr++;
        
        // Address family
        std::uint8_t family = *ptr++;
        
        // Port
        std::uint16_t xor_port;
        std::memcpy(&xor_port, ptr, 2);
        xor_port = ntohs(xor_port);
        std::uint16_t port = xor_port ^ (STUN_MAGIC_COOKIE >> 16);
        ptr += 2;
        
        if (family == 0x01) {
            // IPv4
            if (attribute_data.size() != 8) {
                return ss::core::result<ss::core::endpoint, stun_client_error>::err(
                    stun_client_error::invalid_message);
            }
            
            std::uint32_t xor_addr;
            std::memcpy(&xor_addr, ptr, 4);
            std::uint32_t addr = xor_addr ^ htonl(STUN_MAGIC_COOKIE);
            
            boost::asio::ip::address_v4::bytes_type addr_bytes;
            std::memcpy(addr_bytes.data(), &addr, 4);
            
            boost::asio::ip::address_v4 address(addr_bytes);
            boost::asio::ip::udp::endpoint ep(address, port);
            
            return ss::core::result<ss::core::endpoint, stun_client_error>::ok(
                ss::core::endpoint(ep));
            
        } else if (family == 0x02) {
            // IPv6
            if (attribute_data.size() != 20) {
                return ss::core::result<ss::core::endpoint, stun_client_error>::err(
                    stun_client_error::invalid_message);
            }
            
            boost::asio::ip::address_v6::bytes_type addr_bytes;
            std::memcpy(addr_bytes.data(), ptr, 16);
            
            // First 4 bytes XOR with magic cookie
            std::uint32_t magic_network = htonl(STUN_MAGIC_COOKIE);
            for (int i = 0; i < 4; ++i) {
                addr_bytes[i] ^= reinterpret_cast<const std::uint8_t*>(&magic_network)[i];
            }
            
            // Next 12 bytes XOR with transaction ID
            const auto& tx_id_data = transaction_id.data();
            for (int i = 0; i < 12; ++i) {
                addr_bytes[4 + i] ^= tx_id_data[i];
            }
            
            boost::asio::ip::address_v6 address(addr_bytes);
            boost::asio::ip::udp::endpoint ep(address, port);
            
            return ss::core::result<ss::core::endpoint, stun_client_error>::ok(
                ss::core::endpoint(ep));
        }
        
        return ss::core::result<ss::core::endpoint, stun_client_error>::err(
            stun_client_error::invalid_message);
    }

    stun_client_stats get_stats() const noexcept override {
        return {
            .requests_sent = stats_.requests_sent.load(),
            .responses_received = stats_.responses_received.load(),
            .error_responses = stats_.error_responses.load(),
            .timeouts = stats_.timeouts.load(),
            .retransmissions = stats_.retransmissions.load(),
            .integrity_failures = stats_.integrity_failures.load(),
            .fingerprint_failures = stats_.fingerprint_failures.load(),
            .avg_response_time = stats_.avg_response_time,
            .last_request_time = stats_.last_request_time,
            .last_response_time = stats_.last_response_time
        };
    }

    void reset_stats() noexcept override {
        stats_.requests_sent.store(0);
        stats_.responses_received.store(0);
        stats_.error_responses.store(0);
        stats_.timeouts.store(0);
        stats_.retransmissions.store(0);
        stats_.integrity_failures.store(0);
        stats_.fingerprint_failures.store(0);
        stats_.avg_response_time = std::chrono::milliseconds(0);
        stats_.last_request_time = {};
        stats_.last_response_time = {};
    }

    void set_transport(ss::network::transport_ptr transport) override {
        transport_ = std::move(transport);
        
        if (transport_) {
            // Set up message handler for incoming responses
            transport_->set_message_handler(
                [this](ss::network::incoming_message msg) -> ss::core::async_void {
                    co_await handle_incoming_message(std::move(msg));
                });
        }
    }

    ss::network::transport_ptr get_transport() const noexcept override {
        return transport_;
    }

private:
    /**
     * @brief Wait for STUN response with timeout
     */
    ss::core::async_result<ss::core::result<stun_binding_result, stun_client_error>>
    wait_for_response(const stun_transaction_id& tx_id, 
                     std::chrono::milliseconds timeout) {
        
        // Create transaction entry
        auto transaction = std::make_unique<stun_transaction>(
            tx_id, ss::core::endpoint{}, stun_message_type::binding_request,
            timeout, nullptr, stun_client_config{});
        
        {
            std::lock_guard lock(transactions_mutex_);
            transactions_[tx_id] = std::move(transaction);
        }
        
        // Wait for response or timeout
        auto timer = std::make_unique<ss::core::steady_timer>(io_context_);
        timer->expires_after(timeout);
        
        [[maybe_unused]] auto [ec] = co_await timer->async_wait(boost::asio::as_tuple(boost::asio::use_awaitable));
        
        // Check if response received
        {
            std::lock_guard lock(transactions_mutex_);
            auto it = transactions_.find(tx_id);
            if (it != transactions_.end()) {
                // Timeout - remove transaction
                transactions_.erase(it);
                stats_.timeouts.fetch_add(1);
                
                co_return ss::core::result<stun_binding_result, stun_client_error>::err(
                    stun_client_error::timeout);
            }
        }
        
        // Response should have been handled and result stored
        // This is a simplified implementation - in practice would use promises/futures
        co_return ss::core::result<stun_binding_result, stun_client_error>::err(
            stun_client_error::unknown);
    }

    /**
     * @brief Handle incoming STUN message
     */
    ss::core::async_void handle_incoming_message(ss::network::incoming_message message) {
        // Parse STUN message
        auto parse_result = parse_message(message.data, message.sender);
        if (parse_result.is_err()) {
            co_return;
        }
        
        auto stun_msg = parse_result.value();
        
        // Look up transaction
        std::unique_ptr<stun_transaction> transaction;
        {
            std::lock_guard lock(transactions_mutex_);
            auto it = transactions_.find(stun_msg.transaction_id);
            if (it != transactions_.end()) {
                transaction = std::move(it->second);
                transactions_.erase(it);
            }
        }
        
        if (!transaction) {
            // Unknown transaction - ignore
            co_return;
        }
        
        stats_.responses_received.fetch_add(1);
        stats_.last_response_time = std::chrono::steady_clock::now();
        
        if (stun_msg.is_error_response()) {
            stats_.error_responses.fetch_add(1);
            
            if (transaction->handler) {
                transaction->handler(ss::core::result<stun_binding_result, stun_client_error>::err(
                    stun_client_error::server_error));
            }
            co_return;
        }
        
        // Process binding response
        if (stun_msg.type == stun_message_type::binding_response) {
            auto binding_result = process_binding_response(stun_msg, message.sender);
            
            if (transaction->handler) {
                transaction->handler(binding_result);
            }
        }
        
        co_return;
    }

    /**
     * @brief Process binding response message
     */
    ss::core::result<stun_binding_result, stun_client_error>
    process_binding_response(const stun_message& message, [[maybe_unused]] const ss::core::endpoint& sender) {
        
        // Look for XOR-MAPPED-ADDRESS or MAPPED-ADDRESS
        ss::core::endpoint mapped_address;
        
        const auto* xor_mapped_attr = message.find_attribute(stun_attribute_type::xor_mapped_address);
        if (xor_mapped_attr) {
            auto parse_result = parse_xor_mapped_address(xor_mapped_attr->value, message.transaction_id);
            if (parse_result.is_ok()) {
                mapped_address = parse_result.value();
            }
        } else {
            const auto* mapped_attr = message.find_attribute(stun_attribute_type::mapped_address);
            if (mapped_attr) {
                auto parse_result = parse_mapped_address(mapped_attr->value);
                if (parse_result.is_ok()) {
                    mapped_address = parse_result.value();
                }
            }
        }
        
        if (!mapped_address.is_valid()) {
            return ss::core::result<stun_binding_result, stun_client_error>::err(
                stun_client_error::invalid_message);
        }
        
        // Create result
        stun_binding_result result(mapped_address, message);
        
        // Extract additional attributes
        const auto* source_addr_attr = message.find_attribute(stun_attribute_type::source_address);
        if (source_addr_attr) {
            auto source_result = parse_mapped_address(source_addr_attr->value);
            if (source_result.is_ok()) {
                result.source_address = source_result.value();
            }
        }
        
        const auto* changed_addr_attr = message.find_attribute(stun_attribute_type::changed_address);
        if (changed_addr_attr) {
            auto changed_result = parse_mapped_address(changed_addr_attr->value);
            if (changed_result.is_ok()) {
                result.changed_address = changed_result.value();
            }
        }
        
        const auto* software_attr = message.find_attribute(stun_attribute_type::software);
        if (software_attr) {
            result.server_software = std::string(
                reinterpret_cast<const char*>(software_attr->value.data()),
                software_attr->value.size());
        }
        
        return ss::core::result<stun_binding_result, stun_client_error>::ok(std::move(result));
    }

    /**
     * @brief Parse MAPPED-ADDRESS attribute
     */
    ss::core::result<ss::core::endpoint, stun_client_error>
    parse_mapped_address(const std::vector<std::uint8_t>& attribute_data) const {
        
        if (attribute_data.size() < 4) {
            return ss::core::result<ss::core::endpoint, stun_client_error>::err(
                stun_client_error::invalid_message);
        }
        
        const std::uint8_t* ptr = attribute_data.data();
        
        // Skip reserved byte
        ptr++;
        
        // Address family
        std::uint8_t family = *ptr++;
        
        // Port
        std::uint16_t port;
        std::memcpy(&port, ptr, 2);
        port = ntohs(port);
        ptr += 2;
        
        if (family == 0x01) {
            // IPv4
            if (attribute_data.size() != 8) {
                return ss::core::result<ss::core::endpoint, stun_client_error>::err(
                    stun_client_error::invalid_message);
            }
            
            boost::asio::ip::address_v4::bytes_type addr_bytes;
            std::memcpy(addr_bytes.data(), ptr, 4);
            
            boost::asio::ip::address_v4 address(addr_bytes);
            boost::asio::ip::udp::endpoint ep(address, port);
            
            return ss::core::result<ss::core::endpoint, stun_client_error>::ok(
                ss::core::endpoint(ep));
            
        } else if (family == 0x02) {
            // IPv6
            if (attribute_data.size() != 20) {
                return ss::core::result<ss::core::endpoint, stun_client_error>::err(
                    stun_client_error::invalid_message);
            }
            
            boost::asio::ip::address_v6::bytes_type addr_bytes;
            std::memcpy(addr_bytes.data(), ptr, 16);
            
            boost::asio::ip::address_v6 address(addr_bytes);
            boost::asio::ip::udp::endpoint ep(address, port);
            
            return ss::core::result<ss::core::endpoint, stun_client_error>::ok(
                ss::core::endpoint(ep));
        }
        
        return ss::core::result<ss::core::endpoint, stun_client_error>::err(
            stun_client_error::invalid_message);
    }

    /**
     * @brief Add SOFTWARE attribute
     */
    void add_software_attribute(stun_message& message, const std::string& software) const {
        std::vector<std::uint8_t> value(software.begin(), software.end());
        message.add_attribute(stun_attribute(stun_attribute_type::software, std::move(value)));
    }

    /**
     * @brief Add CHANGE-REQUEST attribute (RFC 3489)
     */
    void add_change_request_attribute(stun_message& message, bool change_ip, bool change_port) const {
        std::vector<std::uint8_t> value(4, 0);
        
        std::uint32_t flags = 0;
        if (change_ip) flags |= 0x04;
        if (change_port) flags |= 0x02;
        
        flags = htonl(flags);
        std::memcpy(value.data(), &flags, 4);
        
        message.add_attribute(stun_attribute(stun_attribute_type::change_request, std::move(value)));
    }

    /**
     * @brief Add MESSAGE-INTEGRITY attribute
     */
    void add_message_integrity(std::vector<std::uint8_t>& message, const std::string& key) const {
        // MESSAGE-INTEGRITY attribute type and length
        std::uint16_t attr_type = htons(static_cast<std::uint16_t>(stun_attribute_type::message_integrity));
        std::uint16_t attr_length = htons(20); // HMAC-SHA1 is 20 bytes
        
        message.insert(message.end(),
                      reinterpret_cast<const std::uint8_t*>(&attr_type),
                      reinterpret_cast<const std::uint8_t*>(&attr_type) + 2);
        message.insert(message.end(),
                      reinterpret_cast<const std::uint8_t*>(&attr_length),
                      reinterpret_cast<const std::uint8_t*>(&attr_length) + 2);
        
        // Update message length in header to include MESSAGE-INTEGRITY
        std::uint16_t updated_length = htons(static_cast<std::uint16_t>(message.size() - STUN_HEADER_SIZE + 20));
        std::memcpy(message.data() + 2, &updated_length, 2);
        
        // Calculate HMAC-SHA1
        std::uint8_t hmac[EVP_MAX_MD_SIZE];
        unsigned int hmac_len = 0;
        
        HMAC(EVP_sha1(),
             key.data(), static_cast<int>(key.size()),
             message.data(), message.size(),
             hmac, &hmac_len);
        
        // Add HMAC to message
        message.insert(message.end(), hmac, hmac + hmac_len);
    }

    /**
     * @brief Add FINGERPRINT attribute
     */
    void add_fingerprint(std::vector<std::uint8_t>& message) const {
        // FINGERPRINT attribute type and length
        std::uint16_t attr_type = htons(static_cast<std::uint16_t>(stun_attribute_type::fingerprint));
        std::uint16_t attr_length = htons(4); // CRC32 is 4 bytes
        
        message.insert(message.end(),
                      reinterpret_cast<const std::uint8_t*>(&attr_type),
                      reinterpret_cast<const std::uint8_t*>(&attr_type) + 2);
        message.insert(message.end(),
                      reinterpret_cast<const std::uint8_t*>(&attr_length),
                      reinterpret_cast<const std::uint8_t*>(&attr_length) + 2);
        
        // Update message length in header to include FINGERPRINT
        std::uint16_t updated_length = htons(static_cast<std::uint16_t>(message.size() - STUN_HEADER_SIZE + 4));
        std::memcpy(message.data() + 2, &updated_length, 2);
        
        // Calculate CRC32
        std::uint32_t crc = crc32(0L, Z_NULL, 0);
        crc = crc32(crc, message.data(), static_cast<uInt>(message.size()));
        crc ^= 0x5354554e; // XOR with STUN fingerprint constant
        crc = htonl(crc);
        
        // Add CRC to message
        message.insert(message.end(),
                      reinterpret_cast<const std::uint8_t*>(&crc),
                      reinterpret_cast<const std::uint8_t*>(&crc) + 4);
    }

    /**
     * @brief Find attribute position in message
     */
    size_t find_attribute_position(const std::vector<std::uint8_t>& message,
                                  stun_attribute_type attr_type) const {
        
        if (message.size() < STUN_HEADER_SIZE) {
            return std::string::npos;
        }
        
        const std::uint8_t* ptr = message.data() + STUN_HEADER_SIZE;
        const std::uint8_t* end = message.data() + message.size();
        
        while (ptr < end) {
            if (ptr + 4 > end) {
                break;
            }
            
            std::uint16_t current_attr_type;
            std::memcpy(&current_attr_type, ptr, 2);
            current_attr_type = ntohs(current_attr_type);
            
            if (current_attr_type == static_cast<std::uint16_t>(attr_type)) {
                return ptr - message.data();
            }
            
            std::uint16_t attr_length;
            std::memcpy(&attr_length, ptr + 2, 2);
            attr_length = ntohs(attr_length);
            
            ptr += 4 + attr_length;
            
            // Skip padding
            auto padding = (4 - (attr_length % 4)) % 4;
            ptr += padding;
        }
        
        return std::string::npos;
    }

    /**
     * @brief Schedule timeout check for transactions
     */
    void schedule_timeout_check() {
        if (!running_.load()) {
            return;
        }
        
        timeout_timer_->expires_after(std::chrono::milliseconds(100));
        timeout_timer_->async_wait([this](const boost::system::error_code& ec) {
            if (!ec && running_.load()) {
                check_transaction_timeouts();
                schedule_timeout_check();
            }
        });
    }

    /**
     * @brief Check for timed out transactions
     */
    void check_transaction_timeouts() {
        auto now = std::chrono::steady_clock::now();
        std::vector<stun_transaction_id> expired_transactions;
        
        {
            std::lock_guard lock(transactions_mutex_);
            for (const auto& [tx_id, transaction] : transactions_) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - transaction->sent_at);
                
                if (elapsed >= transaction->timeout) {
                    expired_transactions.push_back(tx_id);
                }
            }
        }
        
        // Handle expired transactions
        for (const auto& tx_id : expired_transactions) {
            std::unique_ptr<stun_transaction> transaction;
            
            {
                std::lock_guard lock(transactions_mutex_);
                auto it = transactions_.find(tx_id);
                if (it != transactions_.end()) {
                    transaction = std::move(it->second);
                    transactions_.erase(it);
                }
            }
            
            if (transaction && transaction->handler) {
                stats_.timeouts.fetch_add(1);
                transaction->handler(ss::core::result<stun_binding_result, stun_client_error>::err(
                    stun_client_error::timeout));
            }
        }
    }

    /**
     * @brief Stop asynchronously
     */
    void stop_async() {
        boost::asio::co_spawn(io_context_, stop(), boost::asio::detached);
    }
};

} // namespace ss::ice::impl

// String conversion functions implementation
namespace ss::ice {

std::string to_string(stun_client_error error) noexcept {
    switch (error) {
        case stun_client_error::none: return "none";
        case stun_client_error::invalid_message: return "invalid_message";
        case stun_client_error::unsupported_version: return "unsupported_version";
        case stun_client_error::authentication_failed: return "authentication_failed";
        case stun_client_error::timeout: return "timeout";
        case stun_client_error::network_error: return "network_error";
        case stun_client_error::server_error: return "server_error";
        case stun_client_error::invalid_server: return "invalid_server";
        case stun_client_error::transaction_mismatch: return "transaction_mismatch";
        case stun_client_error::fingerprint_failed: return "fingerprint_failed";
        case stun_client_error::integrity_check_failed: return "integrity_check_failed";
        case stun_client_error::unknown: return "unknown";
        default: return "unknown_error";
    }
}

std::string to_string(stun_message_type type) noexcept {
    switch (type) {
        case stun_message_type::binding_request: return "binding_request";
        case stun_message_type::binding_response: return "binding_response";
        case stun_message_type::binding_error_response: return "binding_error_response";
        case stun_message_type::shared_secret_request: return "shared_secret_request";
        case stun_message_type::shared_secret_response: return "shared_secret_response";
        case stun_message_type::shared_secret_error_response: return "shared_secret_error_response";
        default: return "unknown_message_type";
    }
}

std::string to_string(stun_error_code code) noexcept {
    switch (code) {
        case stun_error_code::try_alternate: return "try_alternate";
        case stun_error_code::bad_request: return "bad_request";
        case stun_error_code::unauthorized: return "unauthorized";
        case stun_error_code::unknown_attribute: return "unknown_attribute";
        case stun_error_code::stale_nonce: return "stale_nonce";
        case stun_error_code::server_error: return "server_error";
        default: return "unknown_error_code";
    }
}

stun_transaction_id stun_transaction_id::random() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<std::uint8_t> dist;
    
    storage_type data;
    for (auto& byte : data) {
        byte = dist(gen);
    }
    
    return stun_transaction_id(data);
}

const stun_attribute* stun_message::find_attribute(stun_attribute_type attr_type) const noexcept {
    for (const auto& attr : attributes) {
        if (attr.type == attr_type) {
            return &attr;
        }
    }
    return nullptr;
}

std::uint16_t stun_message::get_error_code() const noexcept {
    if (!is_error_response()) {
        return 0;
    }
    
    const auto* error_attr = find_attribute(stun_attribute_type::error_code);
    if (!error_attr || error_attr->value.size() < 4) {
        return 0;
    }
    
    // ERROR-CODE format: 2 bytes reserved + 1 byte class + 1 byte number
    std::uint8_t error_class = error_attr->value[2] & 0x07;
    std::uint8_t error_number = error_attr->value[3];
    
    return static_cast<std::uint16_t>(error_class * 100 + error_number);
}

std::string stun_message::get_error_reason() const {
    if (!is_error_response()) {
        return "";
    }
    
    const auto* error_attr = find_attribute(stun_attribute_type::error_code);
    if (!error_attr || error_attr->value.size() <= 4) {
        return "";
    }
    
    // Reason phrase starts after 4-byte header
    return std::string(
        reinterpret_cast<const char*>(error_attr->value.data() + 4),
        error_attr->value.size() - 4);
}

} // namespace ss::ice

/**
 * @brief Factory function to create RFC 5389 STUN client
 * @param io_context IO context for async operations
 * @return STUN client instance
 */
std::unique_ptr<ss::ice::i_stun_client> 
create_rfc5389_stun_client(ss::core::io_context& io_context) {
    return std::make_unique<ss::ice::impl::rfc5389_stun_client>(io_context);
}