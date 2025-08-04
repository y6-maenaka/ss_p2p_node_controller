#pragma once

#include <memory>
#include <string>
#include <vector>
#include <array>
#include <optional>
#include <variant>
#include <chrono>
#include <span>
#include <cstdint>
#include <type_traits>
#include <atomic>

#include <json.hpp>
#include "message_pool.fwd.hpp"

namespace ss {

// Application identifier type
using app_id = std::array<char, 8>;

// Message type enumeration for type safety
enum class message_type : uint8_t {
    ping = 1,
    pong = 2,
    find_node = 3,
    find_node_response = 4,
    store = 5,
    store_response = 6,
    ice_candidate = 7,
    ice_response = 8,
    signaling_request = 9,
    signaling_response = 10,
    user_data = 100
};

// Message priority for queue management
enum class message_priority : uint8_t {
    low = 0,
    normal = 1,
    high = 2,
    critical = 3
};

// Serialization format options
enum class serialization_format : uint8_t {
    json = 1,
    binary = 2,
    compressed = 3
};

// Message header structure for wire protocol
struct message_header {
    uint32_t magic_number = 0x53535032; // 'SSP2'
    message_version version = message_version::current;
    message_type type;
    message_priority priority;
    serialization_format format;
    uint32_t payload_size;
    uint32_t checksum;
    uint64_t sequence_id;
    time_point_t timestamp;
    
    static constexpr size_t wire_size = 32; // Fixed size on wire
    
    // Serialize header to binary format
    std::array<uint8_t, wire_size> serialize() const noexcept;
    
    // Deserialize header from binary format
    static result<message_header> deserialize(buffer_span data) noexcept;
    
    // Validate header integrity
    bool is_valid() const noexcept;
};

// Message payload with type safety
class message_payload {
public:
    using value_type = std::variant<
        std::monostate,     // empty
        std::string,        // string data
        nlohmann::json,     // JSON data
        std::vector<uint8_t> // binary data
    >;
    
    message_payload() = default;
    explicit message_payload(std::string data);
    explicit message_payload(nlohmann::json data);
    explicit message_payload(std::vector<uint8_t> data);
    
    // Type-safe accessors
    template<typename T>
    result<T> get() const noexcept;
    
    template<typename T>
    void set(T&& value) noexcept;
    
    // Size estimation
    size_t estimated_size() const noexcept;
    
    // Serialization
    std::vector<uint8_t> serialize(serialization_format format) const;
    static result<message_payload> deserialize(buffer_span data, serialization_format format) noexcept;
    
private:
    value_type data_;
};

// Main message class with RAII and type safety
class message {
public:
    using id_type = uint64_t;
    using app_id = ss::app_id;
    
    // Construction
    message(message_type type, message_priority priority = message_priority::normal);
    message(message_type type, message_payload payload, message_priority priority = message_priority::normal);
    
    // Disable copy, enable move
    message(const message&) = delete;
    message& operator=(const message&) = delete;
    message(message&&) noexcept = default;
    message& operator=(message&&) noexcept = default;
    
    // Accessors
    id_type id() const noexcept { return header_.sequence_id; }
    message_type type() const noexcept { return header_.type; }
    message_priority priority() const noexcept { return header_.priority; }
    time_point_t timestamp() const noexcept { return header_.timestamp; }
    const message_payload& payload() const noexcept { return payload_; }
    message_payload& payload() noexcept { return payload_; }
    
    // Mutators
    void set_priority(message_priority priority) noexcept { header_.priority = priority; }
    void set_payload(message_payload payload) noexcept { payload_ = std::move(payload); }
    
    // Serialization with zero-copy design
    std::vector<uint8_t> serialize(serialization_format format = serialization_format::binary) const;
    static result<message> deserialize(buffer_span data) noexcept;
    
    // Validation
    bool is_valid() const noexcept;
    size_t total_size() const noexcept;
    
    // Utility
    void update_checksum() noexcept;
    bool verify_checksum() const noexcept;
    
private:
    message_header header_;
    message_payload payload_;
    
    static std::atomic<id_type> next_id_;
    
    // Private constructor for deserialization
    message(message_header header, message_payload payload);
    
    // Checksum calculation
    uint32_t calculate_checksum() const noexcept;
};

// Template implementations
template<typename T>
result<T> message_payload::get() const noexcept {
    if (std::holds_alternative<T>(data_)) {
        return std::get<T>(data_);
    }
    return std::nullopt;
}

template<typename T>
void message_payload::set(T&& value) noexcept {
    data_ = std::forward<T>(value);
}

// Utility functions for message handling
namespace message_utils {
    
    // Create typed messages
    message_ptr create_ping_message();
    message_ptr create_pong_message();
    message_ptr create_find_node_message(const std::string& target_id);
    message_ptr create_user_data_message(nlohmann::json data);
    
    // Message validation
    bool is_system_message(message_type type) noexcept;
    bool requires_response(message_type type) noexcept;
    
    // Size utilities
    constexpr size_t max_payload_size() noexcept { return constants::max_message_size - message_header::wire_size; }
    
} // namespace message_utils

} // namespace ss