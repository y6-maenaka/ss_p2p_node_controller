#include <ss_p2p/message.hpp>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <sstream>

namespace ss {

// Static member initialization
std::atomic<message::id_type> message::next_id_{1};

// Message header implementation
std::array<uint8_t, message_header::wire_size> message_header::serialize() const noexcept {
    std::array<uint8_t, wire_size> buffer{};
    size_t offset = 0;
    
    // Magic number (4 bytes)
    std::memcpy(buffer.data() + offset, &magic_number, sizeof(magic_number));
    offset += sizeof(magic_number);
    
    // Version (2 bytes)
    uint16_t version_val = static_cast<uint16_t>(version);
    std::memcpy(buffer.data() + offset, &version_val, sizeof(version_val));
    offset += sizeof(version_val);
    
    // Type (1 byte)
    uint8_t type_val = static_cast<uint8_t>(type);
    std::memcpy(buffer.data() + offset, &type_val, sizeof(type_val));
    offset += sizeof(type_val);
    
    // Priority (1 byte)
    uint8_t priority_val = static_cast<uint8_t>(priority);
    std::memcpy(buffer.data() + offset, &priority_val, sizeof(priority_val));
    offset += sizeof(priority_val);
    
    // Format (1 byte)
    uint8_t format_val = static_cast<uint8_t>(format);
    std::memcpy(buffer.data() + offset, &format_val, sizeof(format_val));
    offset += sizeof(format_val);
    
    // Reserved padding (3 bytes)
    offset += 3;
    
    // Payload size (4 bytes)
    std::memcpy(buffer.data() + offset, &payload_size, sizeof(payload_size));
    offset += sizeof(payload_size);
    
    // Checksum (4 bytes)
    std::memcpy(buffer.data() + offset, &checksum, sizeof(checksum));
    offset += sizeof(checksum);
    
    // Sequence ID (8 bytes)
    std::memcpy(buffer.data() + offset, &sequence_id, sizeof(sequence_id));
    offset += sizeof(sequence_id);
    
    // Timestamp (8 bytes) - convert to microseconds since epoch
    auto duration = timestamp.time_since_epoch();
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    std::memcpy(buffer.data() + offset, &micros, sizeof(micros));
    
    return buffer;
}

result<message_header> message_header::deserialize(buffer_span data) noexcept {
    if (data.size() < wire_size) {
        return std::nullopt;
    }
    
    message_header header;
    size_t offset = 0;
    
    // Magic number
    std::memcpy(&header.magic_number, data.data() + offset, sizeof(header.magic_number));
    offset += sizeof(header.magic_number);
    
    if (header.magic_number != 0x53535032) {
        return std::nullopt;
    }
    
    // Version
    uint16_t version_val;
    std::memcpy(&version_val, data.data() + offset, sizeof(version_val));
    header.version = static_cast<message_version>(version_val);
    offset += sizeof(version_val);
    
    // Type
    uint8_t type_val;
    std::memcpy(&type_val, data.data() + offset, sizeof(type_val));
    header.type = static_cast<message_type>(type_val);
    offset += sizeof(type_val);
    
    // Priority
    uint8_t priority_val;
    std::memcpy(&priority_val, data.data() + offset, sizeof(priority_val));
    header.priority = static_cast<message_priority>(priority_val);
    offset += sizeof(priority_val);
    
    // Format
    uint8_t format_val;
    std::memcpy(&format_val, data.data() + offset, sizeof(format_val));
    header.format = static_cast<serialization_format>(format_val);
    offset += sizeof(format_val);
    
    // Skip reserved padding
    offset += 3;
    
    // Payload size
    std::memcpy(&header.payload_size, data.data() + offset, sizeof(header.payload_size));
    offset += sizeof(header.payload_size);
    
    // Checksum
    std::memcpy(&header.checksum, data.data() + offset, sizeof(header.checksum));
    offset += sizeof(header.checksum);
    
    // Sequence ID
    std::memcpy(&header.sequence_id, data.data() + offset, sizeof(header.sequence_id));
    offset += sizeof(header.sequence_id);
    
    // Timestamp
    int64_t micros;
    std::memcpy(&micros, data.data() + offset, sizeof(micros));
    header.timestamp = time_point_t{std::chrono::microseconds{micros}};
    
    return header.is_valid() ? std::make_optional(header) : std::nullopt;
}

bool message_header::is_valid() const noexcept {
    return magic_number == 0x53535032 &&
           version == message_version::current &&
           payload_size <= constants::max_message_size &&
           static_cast<uint8_t>(type) > 0 &&
           static_cast<uint8_t>(priority) <= 3 &&
           static_cast<uint8_t>(format) <= 3;
}

// Message payload implementation
message_payload::message_payload(std::string data) : data_(std::move(data)) {}
message_payload::message_payload(nlohmann::json data) : data_(std::move(data)) {}
message_payload::message_payload(std::vector<uint8_t> data) : data_(std::move(data)) {}

size_t message_payload::estimated_size() const noexcept {
    return std::visit([](const auto& value) -> size_t {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return 0;
        } else if constexpr (std::is_same_v<T, std::string>) {
            return value.size();
        } else if constexpr (std::is_same_v<T, nlohmann::json>) {
            return value.dump().size();
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            return value.size();
        }
        return 0;
    }, data_);
}

std::vector<uint8_t> message_payload::serialize(serialization_format format) const {
    return std::visit([format](const auto& value) -> std::vector<uint8_t> {
        using T = std::decay_t<decltype(value)>;
        
        if constexpr (std::is_same_v<T, std::monostate>) {
            return {};
        } else if constexpr (std::is_same_v<T, std::string>) {
            return std::vector<uint8_t>(value.begin(), value.end());
        } else if constexpr (std::is_same_v<T, nlohmann::json>) {
            switch (format) {
                case serialization_format::json: {
                    auto json_str = value.dump();
                    return std::vector<uint8_t>(json_str.begin(), json_str.end());
                }
                case serialization_format::binary:
                    return nlohmann::json::to_bson(value);
                case serialization_format::compressed: {
                    // For now, just use BSON (could add compression later)
                    return nlohmann::json::to_bson(value);
                }
            }
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            return value;
        }
        return {};
    }, data_);
}

result<message_payload> message_payload::deserialize(buffer_span data, serialization_format format) noexcept {
    try {
        message_payload payload;
        
        switch (format) {
            case serialization_format::json: {
                std::string json_str(data.begin(), data.end());
                payload.data_ = nlohmann::json::parse(json_str);
                break;
            }
            case serialization_format::binary: {
                std::vector<uint8_t> bson_data(data.begin(), data.end());
                payload.data_ = nlohmann::json::from_bson(bson_data);
                break;
            }
            case serialization_format::compressed: {
                // For now, treat as BSON
                std::vector<uint8_t> bson_data(data.begin(), data.end());
                payload.data_ = nlohmann::json::from_bson(bson_data);
                break;
            }
            default:
                return std::nullopt;
        }
        
        return payload;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

// Message implementation
message::message(message_type type, message_priority priority)
    : header_{}, payload_{} {
    header_.type = type;
    header_.priority = priority;
    header_.format = serialization_format::binary;
    header_.sequence_id = next_id_.fetch_add(1, std::memory_order_relaxed);
    header_.timestamp = std::chrono::steady_clock::now();
    header_.payload_size = 0;
    header_.checksum = 0;
}

message::message(message_type type, message_payload payload, message_priority priority)
    : message(type, priority) {
    payload_ = std::move(payload);
    header_.payload_size = static_cast<uint32_t>(payload_.estimated_size());
    update_checksum();
}

message::message(message_header header, message_payload payload)
    : header_(header), payload_(std::move(payload)) {}

std::vector<uint8_t> message::serialize(serialization_format format) const {
    // Update header with current payload info
    auto mutable_header = header_;
    mutable_header.format = format;
    mutable_header.payload_size = static_cast<uint32_t>(payload_.estimated_size());
    
    // Serialize payload first to get actual size
    auto payload_data = payload_.serialize(format);
    mutable_header.payload_size = static_cast<uint32_t>(payload_data.size());
    
    // Calculate checksum
    mutable_header.checksum = 0; // Clear for calculation
    auto header_data = mutable_header.serialize();
    
    // Simple checksum: XOR of all bytes
    uint32_t checksum = 0;
    for (auto byte : header_data) {
        checksum ^= static_cast<uint32_t>(byte);
    }
    for (auto byte : payload_data) {
        checksum ^= static_cast<uint32_t>(byte);
    }
    
    mutable_header.checksum = checksum;
    header_data = mutable_header.serialize();
    
    // Combine header and payload
    std::vector<uint8_t> result;
    result.reserve(header_data.size() + payload_data.size());
    result.insert(result.end(), header_data.begin(), header_data.end());
    result.insert(result.end(), payload_data.begin(), payload_data.end());
    
    return result;
}

result<message> message::deserialize(buffer_span data) noexcept {
    if (data.size() < message_header::wire_size) {
        return std::nullopt;
    }
    
    // Deserialize header
    auto header_result = message_header::deserialize(data.subspan(0, message_header::wire_size));
    if (!header_result) {
        return std::nullopt;
    }
    
    auto header = *header_result;
    
    // Validate total size
    if (data.size() < message_header::wire_size + header.payload_size) {
        return std::nullopt;
    }
    
    // Deserialize payload
    auto payload_data = data.subspan(message_header::wire_size, header.payload_size);
    auto payload_result = message_payload::deserialize(payload_data, header.format);
    if (!payload_result) {
        return std::nullopt;
    }
    
    // Create message and verify checksum
    message msg(header, *payload_result);
    if (!msg.verify_checksum()) {
        return std::nullopt;
    }
    
    return msg;
}

bool message::is_valid() const noexcept {
    return header_.is_valid() && verify_checksum();
}

size_t message::total_size() const noexcept {
    return message_header::wire_size + payload_.estimated_size();
}

void message::update_checksum() noexcept {
    header_.checksum = calculate_checksum();
}

bool message::verify_checksum() const noexcept {
    auto saved_checksum = header_.checksum;
    return calculate_checksum() == saved_checksum;
}

uint32_t message::calculate_checksum() const noexcept {
    // Create temporary header with checksum cleared
    auto temp_header = header_;
    temp_header.checksum = 0;
    auto header_data = temp_header.serialize();
    auto payload_data = payload_.serialize(header_.format);
    
    // Calculate XOR checksum
    uint32_t checksum = 0;
    for (auto byte : header_data) {
        checksum ^= static_cast<uint32_t>(byte);
    }
    for (auto byte : payload_data) {
        checksum ^= static_cast<uint32_t>(byte);
    }
    
    return checksum;
}

// Utility functions implementation
namespace message_utils {

message_ptr create_ping_message() {
    return std::make_shared<message>(message_type::ping, message_priority::normal);
}

message_ptr create_pong_message() {
    return std::make_shared<message>(message_type::pong, message_priority::normal);
}

message_ptr create_find_node_message(const std::string& target_id) {
    nlohmann::json data;
    data["target_id"] = target_id;
    message_payload payload(data);
    return std::make_shared<message>(message_type::find_node, std::move(payload), message_priority::high);
}

message_ptr create_user_data_message(nlohmann::json data) {
    message_payload payload(std::move(data));
    return std::make_shared<message>(message_type::user_data, std::move(payload), message_priority::normal);
}

bool is_system_message(message_type type) noexcept {
    return static_cast<uint8_t>(type) < 100;
}

bool requires_response(message_type type) noexcept {
    switch (type) {
        case message_type::ping:
        case message_type::find_node:
        case message_type::store:
        case message_type::ice_candidate:
        case message_type::signaling_request:
            return true;
        default:
            return false;
    }
}

} // namespace message_utils

} // namespace ss