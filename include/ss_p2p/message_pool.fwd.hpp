#pragma once

#include <memory>
#include <functional>
#include <chrono>
#include <optional>
#include <span>
#include <boost/asio/ip/udp.hpp>

namespace ss {

// Forward declarations for message system
class message;
class message_pool;
class message_buffer;
class message_hub;
class peer;

// Type aliases for common types
using message_ptr = std::shared_ptr<message>;
using message_buffer_ptr = std::shared_ptr<message_buffer>;
using message_hub_ptr = std::shared_ptr<message_hub>;
using peer_ptr = std::shared_ptr<peer>;

// Network related aliases
using endpoint_t = boost::asio::ip::udp::endpoint;
using time_point_t = std::chrono::steady_clock::time_point;
using duration_t = std::chrono::milliseconds;

// Buffer and span types
using buffer_span = std::span<const uint8_t>;
using mutable_buffer_span = std::span<uint8_t>;

// Callback types
using message_handler_t = std::function<void(peer_ptr, message_ptr)>;
using endpoint_to_peer_func_t = std::function<peer_ptr(const endpoint_t&)>;

// Message versioning
enum class message_version : uint16_t {
    v1_0 = 0x0100,
    current = v1_0
};

// Result types for error handling
template<typename T>
using result = std::optional<T>;

// Constants
namespace constants {
    constexpr size_t default_buffer_size = 8 * 1024 * 1024; // 8MB
    constexpr size_t max_message_size = 64 * 1024; // 64KB
    constexpr duration_t default_message_timeout{300000}; // 5 minutes
    constexpr size_t default_pool_size = 1024;
}

} // namespace ss
