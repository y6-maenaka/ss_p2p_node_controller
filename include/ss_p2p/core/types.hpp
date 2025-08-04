#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <memory>
#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

namespace ss::core {

/**
 * @brief 160-bit node identifier used in Kademlia DHT
 * 
 * Provides a type-safe wrapper around a 160-bit (20-byte) identifier
 * with comparison operators and XOR distance calculation.
 */
class node_id {
public:
    static constexpr size_t SIZE = 20; // 160 bits / 8 = 20 bytes
    using storage_type = std::array<std::uint8_t, SIZE>;

    /**
     * @brief Default constructor - creates zero-initialized node ID
     */
    constexpr node_id() noexcept : data_{} {}

    /**
     * @brief Construct from byte array
     * @param data Raw byte data
     */
    explicit constexpr node_id(const storage_type& data) noexcept : data_(data) {}

    /**
     * @brief Copy constructor
     */
    constexpr node_id(const node_id&) noexcept = default;

    /**
     * @brief Move constructor
     */
    constexpr node_id(node_id&&) noexcept = default;

    /**
     * @brief Copy assignment operator
     */
    constexpr node_id& operator=(const node_id&) noexcept = default;

    /**
     * @brief Move assignment operator
     */
    constexpr node_id& operator=(node_id&&) noexcept = default;

    /**
     * @brief Destructor
     */
    ~node_id() = default;

    /**
     * @brief Get raw data access
     * @return Reference to underlying storage
     */
    constexpr const storage_type& data() const noexcept { return data_; }

    /**
     * @brief Get mutable raw data access
     * @return Mutable reference to underlying storage
     */
    constexpr storage_type& data() noexcept { return data_; }

    /**
     * @brief Calculate XOR distance to another node ID
     * @param other The other node ID
     * @return XOR distance as a new node_id
     */
    constexpr node_id distance(const node_id& other) const noexcept {
        node_id result;
        for (size_t i = 0; i < SIZE; ++i) {
            result.data_[i] = data_[i] ^ other.data_[i];
        }
        return result;
    }

    /**
     * @brief Equality comparison
     */
    constexpr bool operator==(const node_id& other) const noexcept {
        return data_ == other.data_;
    }

    /**
     * @brief Inequality comparison
     */
    constexpr bool operator!=(const node_id& other) const noexcept {
        return !(*this == other);
    }

    /**
     * @brief Less-than comparison for ordering
     */
    constexpr bool operator<(const node_id& other) const noexcept {
        return data_ < other.data_;
    }

    /**
     * @brief Convert to hexadecimal string representation
     * @return Hex string
     */
    std::string to_hex() const;

    /**
     * @brief Create node_id from hexadecimal string
     * @param hex_str Hexadecimal string (40 characters)
     * @return node_id instance
     * @throws std::invalid_argument if string format is invalid
     */
    static node_id from_hex(const std::string& hex_str);

    /**
     * @brief Generate random node ID
     * @return Randomly generated node_id
     */
    static node_id random();

private:
    storage_type data_;
};

/**
 * @brief Peer identifier - alias for node_id with semantic meaning
 */
using peer_id = node_id;

/**
 * @brief Message identifier for tracking requests/responses
 */
class message_id {
public:
    using value_type = std::uint64_t;

    /**
     * @brief Default constructor - creates zero ID
     */
    constexpr message_id() noexcept : value_(0) {}

    /**
     * @brief Construct from value
     * @param value Message ID value
     */
    explicit constexpr message_id(value_type value) noexcept : value_(value) {}

    /**
     * @brief Get the underlying value
     */
    constexpr value_type value() const noexcept { return value_; }

    /**
     * @brief Equality comparison
     */
    constexpr bool operator==(const message_id& other) const noexcept {
        return value_ == other.value_;
    }

    /**
     * @brief Inequality comparison
     */
    constexpr bool operator!=(const message_id& other) const noexcept {
        return value_ != other.value_;
    }

    /**
     * @brief Less-than comparison for ordering
     */
    constexpr bool operator<(const message_id& other) const noexcept {
        return value_ < other.value_;
    }

    /**
     * @brief Generate next message ID (thread-safe)
     * @return New unique message_id
     */
    static message_id generate();

private:
    value_type value_;
};

/**
 * @brief Network endpoint with strong typing
 */
class endpoint {
public:
    using native_type = boost::asio::ip::udp::endpoint;

    /**
     * @brief Default constructor
     */
    endpoint() = default;

    /**
     * @brief Construct from Boost.Asio endpoint
     * @param ep Boost.Asio UDP endpoint
     */
    explicit endpoint(const native_type& ep) noexcept : endpoint_(ep) {}

    /**
     * @brief Construct from address and port
     * @param addr IP address string
     * @param port Port number
     */
    endpoint(const std::string& addr, std::uint16_t port);

    /**
     * @brief Get native Boost.Asio endpoint
     */
    const native_type& native() const noexcept { return endpoint_; }

    /**
     * @brief Get mutable native Boost.Asio endpoint
     */
    native_type& native() noexcept { return endpoint_; }

    /**
     * @brief Get IP address string
     */
    std::string address() const { return endpoint_.address().to_string(); }

    /**
     * @brief Get port number
     */
    std::uint16_t port() const noexcept { return endpoint_.port(); }

    /**
     * @brief Check if endpoint is valid (not default-constructed)
     */
    bool is_valid() const noexcept { return endpoint_.port() != 0; }

    /**
     * @brief Equality comparison
     */
    bool operator==(const endpoint& other) const noexcept {
        return endpoint_ == other.endpoint_;
    }

    /**
     * @brief Inequality comparison
     */
    bool operator!=(const endpoint& other) const noexcept {
        return !(*this == other);
    }

    /**
     * @brief Less-than comparison for ordering
     */
    bool operator<(const endpoint& other) const noexcept {
        if (endpoint_.address() != other.endpoint_.address()) {
            return endpoint_.address() < other.endpoint_.address();
        }
        return endpoint_.port() < other.endpoint_.port();
    }

    /**
     * @brief Convert to string representation
     */
    std::string to_string() const;

private:
    native_type endpoint_;
};

/**
 * @brief Async result type using Boost.Asio awaitable
 * @tparam T Result value type
 */
template<typename T>
using async_result = boost::asio::awaitable<T>;

/**
 * @brief Async void result type
 */
using async_void = boost::asio::awaitable<void>;

/**
 * @brief Type alias for IO context
 */
using io_context = boost::asio::io_context;

/**
 * @brief Type alias for executor
 */
using executor_type = boost::asio::io_context::executor_type;

/**
 * @brief Type alias for steady timer
 */
using steady_timer = boost::asio::steady_timer;

/**
 * @brief Type alias for UDP socket
 */
using udp_socket = boost::asio::ip::udp::socket;

/**
 * @brief Component error codes
 */
enum class component_error {
    success = 0,
    initialization_failed,
    already_initialized,
    not_initialized,
    shutdown_failed,
    timeout,
    network_error,
    invalid_state,
    resource_exhausted
};

} // namespace ss::core

/**
 * @brief Hash specialization for node_id
 */
template<>
struct std::hash<ss::core::node_id> {
    std::size_t operator()(const ss::core::node_id& id) const noexcept {
        const auto& data = id.data();
        std::size_t hash = 0;
        for (size_t i = 0; i < ss::core::node_id::SIZE; i += sizeof(std::size_t)) {
            std::size_t chunk = 0;
            std::memcpy(&chunk, &data[i], 
                       std::min(sizeof(std::size_t), ss::core::node_id::SIZE - i));
            hash ^= std::hash<std::size_t>{}(chunk) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
        return hash;
    }
};

/**
 * @brief Hash specialization for message_id
 */
template<>
struct std::hash<ss::core::message_id> {
    std::size_t operator()(const ss::core::message_id& id) const noexcept {
        return std::hash<ss::core::message_id::value_type>{}(id.value());
    }
};

/**
 * @brief Hash specialization for endpoint
 */
template<>
struct std::hash<ss::core::endpoint> {
    std::size_t operator()(const ss::core::endpoint& ep) const noexcept {
        auto addr_hash = std::hash<std::string>{}(ep.address());
        auto port_hash = std::hash<std::uint16_t>{}(ep.port());
        return addr_hash ^ (port_hash << 1);
    }
};