#include <ss_p2p/socket_manager.hpp>

#include <stdexcept>
#include <sstream>

namespace ss {

// UDP Socket Manager Implementation
udp_socket_manager::udp_socket_manager(
    const boost::asio::ip::udp::endpoint& endpoint,
    ss::core::io_context& io_context
) : io_context_(io_context)
  , local_endpoint_(endpoint)
  , initialized_(false)
{
    try {
        // Create transport factory
        transport_factory_ = std::make_unique<ss::network::transport_factory>(io_context_);
        
        // Create legacy socket for backward compatibility
        legacy_socket_ = std::make_unique<boost::asio::ip::udp::socket>(io_context_);
        
        // Open and bind legacy socket
        legacy_socket_->open(endpoint.protocol());
        legacy_socket_->bind(endpoint);
        
        initialize();
    }
    catch (const std::exception& e) {
        throw std::runtime_error("Failed to create UDP socket manager: " + std::string(e.what()));
    }
}

udp_socket_manager::~udp_socket_manager() noexcept {
    try {
        close();
    }
    catch (...) {
        // Destructor should not throw
    }
}

ss::core::io_context& udp_socket_manager::io_ctx() noexcept {
    return io_context_;
}

boost::asio::ip::udp::socket& udp_socket_manager::self_sock() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!legacy_socket_) {
        throw std::runtime_error("Socket has been closed");
    }
    return *legacy_socket_;
}

boost::asio::ip::udp::endpoint udp_socket_manager::local_endpoint() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return local_endpoint_;
}

bool udp_socket_manager::is_open() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return legacy_socket_ && legacy_socket_->is_open();
}

void udp_socket_manager::close() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        if (legacy_socket_ && legacy_socket_->is_open()) {
            legacy_socket_->close();
        }
        
        if (transport_) {
            // Note: In a real implementation, we would properly await the async cleanup
            // For now, we'll reset the transport
            transport_.reset();
        }
        
        initialized_.store(false);
    }
    catch (...) {
        // Close should not throw
    }
}

ss::network::transport_stats udp_socket_manager::get_stats() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (transport_) {
        return transport_->get_stats();
    }
    return {};
}

void udp_socket_manager::reset_stats() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (transport_) {
        transport_->reset_stats();
    }
}

void udp_socket_manager::initialize() {
    if (initialized_.load()) {
        return;
    }

    try {
        // Create transport configuration
        ss::network::transport_config config;
        config.enable_ipv6 = local_endpoint_.address().is_v6();
        config.max_message_size = 65536;
        config.receive_buffer_size = 1024 * 1024; // 1MB
        config.send_buffer_size = 1024 * 1024;    // 1MB
        config.io_threads = 1;
        config.enable_fragmentation = true;

        // Create UDP transport
        auto transport_result = transport_factory_->create_udp_transport(config);
        if (!transport_result) {
            throw std::runtime_error("Failed to create UDP transport: " + transport_result.error().message);
        }
        transport_ = std::move(transport_result.value());

        // Note: In a full implementation, we would initialize and bind the transport
        // For legacy compatibility, we'll let the legacy socket handle the actual binding
        
        initialized_.store(true);
    }
    catch (const std::exception& e) {
        throw std::runtime_error("Failed to initialize socket manager: " + std::string(e.what()));
    }
}

void udp_socket_manager::sync_transport_with_socket() {
    // This method would synchronize the transport state with the legacy socket
    // For now, it's a placeholder for future implementation
}

// TCP Socket Manager Implementation (minimal implementation for compatibility)
tcp_socket_manager::tcp_socket_manager(
    const boost::asio::ip::tcp::endpoint& endpoint,
    ss::core::io_context& io_context
) : io_context_(io_context)
  , local_endpoint_(endpoint)
{
    try {
        socket_ = std::make_unique<boost::asio::ip::tcp::socket>(io_context_);
        socket_->open(endpoint.protocol());
        
        // For TCP, we typically don't bind to a specific port unless it's a server
        // This implementation is minimal and would need to be expanded for full TCP support
    }
    catch (const std::exception& e) {
        throw std::runtime_error("Failed to create TCP socket manager: " + std::string(e.what()));
    }
}

tcp_socket_manager::~tcp_socket_manager() noexcept {
    try {
        close();
    }
    catch (...) {
        // Destructor should not throw
    }
}

ss::core::io_context& tcp_socket_manager::io_ctx() noexcept {
    return io_context_;
}

boost::asio::ip::tcp::socket& tcp_socket_manager::self_sock() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!socket_) {
        throw std::runtime_error("Socket has been closed");
    }
    return *socket_;
}

boost::asio::ip::tcp::endpoint tcp_socket_manager::local_endpoint() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return local_endpoint_;
}

bool tcp_socket_manager::is_open() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return socket_ && socket_->is_open();
}

void tcp_socket_manager::close() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        if (socket_ && socket_->is_open()) {
            socket_->close();
        }
    }
    catch (...) {
        // Close should not throw
    }
}

// Generic Socket Manager Implementation
socket_manager::sock_type socket_manager::get_sock_type() const noexcept {
    return std::visit([](const auto& manager) -> sock_type {
        using T = std::decay_t<decltype(manager)>;
        if constexpr (std::is_same_v<T, udp_socket_manager>) {
            return sock_type::udp;
        } else if constexpr (std::is_same_v<T, tcp_socket_manager>) {
            return sock_type::tcp;
        }
    }, socket_manager_);
}

bool socket_manager::is_udp() const noexcept {
    return get_sock_type() == sock_type::udp;
}

bool socket_manager::is_tcp() const noexcept {
    return get_sock_type() == sock_type::tcp;
}

udp_socket_manager& socket_manager::as_udp() {
    return std::get<udp_socket_manager>(socket_manager_);
}

tcp_socket_manager& socket_manager::as_tcp() {
    return std::get<tcp_socket_manager>(socket_manager_);
}

const udp_socket_manager& socket_manager::as_udp() const {
    return std::get<udp_socket_manager>(socket_manager_);
}

const tcp_socket_manager& socket_manager::as_tcp() const {
    return std::get<tcp_socket_manager>(socket_manager_);
}

} // namespace ss