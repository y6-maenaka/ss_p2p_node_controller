#pragma once

#include "./network/transport_factory.hpp"
#include "./network/i_transport.hpp"
#include "./core/interfaces.hpp"
#include "./core/result.hpp"
#include "./core/types.hpp"

#include <memory>
#include <variant>
#include <string>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <boost/asio.hpp>

namespace ss {

/**
 * @brief Legacy UDP socket manager adapter for backward compatibility
 * 
 * This class wraps the new network::transport_factory to maintain API compatibility
 * with existing code while providing modern resource management and error handling.
 * 
 * Thread Safety: All public methods are thread-safe
 * Exception Safety: Strong guarantee - operations either succeed completely or have no effect
 */
class udp_socket_manager final {
public:
    /// Result type for operations that can fail
    template<typename T = void>
    using result = ss::core::result<T, std::string>;

    /**
     * @brief Constructor
     * @param endpoint Local endpoint to bind to
     * @param io_context IO context for async operations
     * @throws std::runtime_error if binding fails
     */
    explicit udp_socket_manager(
        const boost::asio::ip::udp::endpoint& endpoint,
        ss::core::io_context& io_context
    );

    /**
     * @brief Destructor - automatically cleans up resources
     */
    ~udp_socket_manager() noexcept;

    // Non-copyable, non-movable for legacy compatibility
    udp_socket_manager(const udp_socket_manager&) = delete;
    udp_socket_manager& operator=(const udp_socket_manager&) = delete;
    udp_socket_manager(udp_socket_manager&&) = delete;
    udp_socket_manager& operator=(udp_socket_manager&&) = delete;

    /**
     * @brief Get IO context reference
     * @return Reference to the IO context
     */
    ss::core::io_context& io_ctx() noexcept;

    /**
     * @brief Get legacy socket reference
     * @return Reference to the underlying Boost.Asio UDP socket
     * @note This method exists for backward compatibility
     */
    boost::asio::ip::udp::socket& self_sock();

    /**
     * @brief Get local endpoint
     * @return Local endpoint the socket is bound to
     */
    boost::asio::ip::udp::endpoint local_endpoint() const noexcept;

    /**
     * @brief Check if socket is open
     * @return true if socket is open, false otherwise
     */
    bool is_open() const noexcept;

    /**
     * @brief Close the socket
     */
    void close() noexcept;

    /**
     * @brief Get socket statistics
     * @return Transport statistics
     */
    ss::network::transport_stats get_stats() const noexcept;

    /**
     * @brief Reset socket statistics
     */
    void reset_stats() noexcept;

private:
    /// IO context reference
    ss::core::io_context& io_context_;
    
    /// Transport factory
    std::unique_ptr<ss::network::transport_factory> transport_factory_;
    
    /// Underlying transport
    std::unique_ptr<ss::network::i_transport> transport_;
    
    /// Legacy socket wrapper - maintains a Boost.Asio socket for compatibility
    std::unique_ptr<boost::asio::ip::udp::socket> legacy_socket_;
    
    /// Local endpoint
    boost::asio::ip::udp::endpoint local_endpoint_;
    
    /// Thread safety mutex
    mutable std::mutex mutex_;
    
    /// Initialization state
    std::atomic<bool> initialized_;

    /**
     * @brief Initialize the socket manager
     */
    void initialize();

    /**
     * @brief Synchronize transport state with legacy socket
     */
    void sync_transport_with_socket();
};

/**
 * @brief Legacy TCP socket manager (placeholder for future implementation)
 * @note Currently not implemented - kept for API compatibility
 */
class tcp_socket_manager final {
public:
    explicit tcp_socket_manager(
        const boost::asio::ip::tcp::endpoint& endpoint,
        ss::core::io_context& io_context
    );

    ~tcp_socket_manager() noexcept;

    // Non-copyable, non-movable
    tcp_socket_manager(const tcp_socket_manager&) = delete;
    tcp_socket_manager& operator=(const tcp_socket_manager&) = delete;
    tcp_socket_manager(tcp_socket_manager&&) = delete;
    tcp_socket_manager& operator=(tcp_socket_manager&&) = delete;

    ss::core::io_context& io_ctx() noexcept;
    boost::asio::ip::tcp::socket& self_sock();
    boost::asio::ip::tcp::endpoint local_endpoint() const noexcept;
    bool is_open() const noexcept;
    void close() noexcept;

private:
    ss::core::io_context& io_context_;
    std::unique_ptr<boost::asio::ip::tcp::socket> socket_;
    boost::asio::ip::tcp::endpoint local_endpoint_;
    mutable std::mutex mutex_;
};

/**
 * @brief Union type for socket managers (legacy compatibility)
 */
using union_socket_manager = std::variant<udp_socket_manager, tcp_socket_manager>;

/**
 * @brief Concept for allowed socket manager types
 */
template<typename T>
concept AllowedSocketManagerTypes = std::is_same_v<T, udp_socket_manager> || 
                                    std::is_same_v<T, tcp_socket_manager>;

/**
 * @brief Union type for sockets (legacy compatibility)
 */
using union_socket = std::variant<boost::asio::ip::udp::socket, boost::asio::ip::tcp::socket>;

/**
 * @brief Concept for allowed socket types
 */
template<typename T>
concept AllowedSocketTypes = std::is_same_v<T, boost::asio::ip::udp::socket> || 
                            std::is_same_v<T, boost::asio::ip::tcp::socket>;

/**
 * @brief Generic socket manager wrapper (legacy compatibility)
 * 
 * This class provides a unified interface for different socket types
 * while maintaining backward compatibility with existing code.
 */
class socket_manager final {
public:
    /// Socket type enumeration
    enum class sock_type {
        udp,
        tcp
    };

    /**
     * @brief Constructor from socket manager
     * @param from Socket manager instance
     */
    template<AllowedSocketManagerTypes T>
    explicit socket_manager(T& from) : socket_manager_(from) {}

    /**
     * @brief Get socket type
     * @return Socket type
     */
    sock_type get_sock_type() const noexcept;

    /**
     * @brief Check if socket is UDP
     * @return true if UDP socket, false otherwise
     */
    bool is_udp() const noexcept;

    /**
     * @brief Check if socket is TCP
     * @return true if TCP socket, false otherwise
     */
    bool is_tcp() const noexcept;

    /**
     * @brief Get UDP socket manager
     * @return Reference to UDP socket manager
     * @throws std::bad_variant_access if not UDP
     */
    udp_socket_manager& as_udp();

    /**
     * @brief Get TCP socket manager
     * @return Reference to TCP socket manager
     * @throws std::bad_variant_access if not TCP
     */
    tcp_socket_manager& as_tcp();

    /**
     * @brief Get UDP socket manager (const)
     * @return Const reference to UDP socket manager
     * @throws std::bad_variant_access if not UDP
     */
    const udp_socket_manager& as_udp() const;

    /**
     * @brief Get TCP socket manager (const)
     * @return Const reference to TCP socket manager
     * @throws std::bad_variant_access if not TCP
     */
    const tcp_socket_manager& as_tcp() const;

private:
    /// Union socket manager instance
    union_socket_manager socket_manager_;
};

} // namespace ss