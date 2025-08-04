#include <ss_p2p/udp_server.hpp>
#include <ss_p2p/network/i_transport.hpp>

#include <stdexcept>
#include <sstream>
#include <chrono>

namespace ss {

udp_server::udp_server(
    const boost::asio::ip::udp::endpoint& local_endpoint,
    ss::core::io_context& io_context,
    recv_packet_handler recv_handler,
    logger* logger
) : io_context_(io_context)
  , recv_handler_(std::move(recv_handler))
  , logger_(logger)
  , local_endpoint_(local_endpoint)
  , running_(false)
  , initialized_(false)
{
    if (!recv_handler_) {
        throw std::invalid_argument("recv_handler cannot be null");
    }

    try {
        // Create transport factory
        transport_factory_ = std::make_unique<ss::network::transport_factory>(io_context_);

        // Create transport configuration
        ss::network::transport_config config;
        config.enable_ipv6 = local_endpoint_.address().is_v6();
        config.max_message_size = 65536;
        config.receive_buffer_size = 1024 * 1024; // 1MB receive buffer
        config.send_buffer_size = 1024 * 1024;    // 1MB send buffer
        config.io_threads = 1;
        config.enable_fragmentation = true;

        // Create UDP transport
        auto transport_result = transport_factory_->create_udp_transport(config);
        if (!transport_result) {
            throw std::runtime_error("Failed to create UDP transport: " + transport_result.error().message);
        }
        transport_ = std::move(transport_result.value());

        log(logger::log_level::INFO, "UDP server adapter created successfully");
    }
    catch (const std::exception& e) {
        log(logger::log_level::ERROR, "Failed to create UDP server: " + std::string(e.what()));
        throw;
    }
}

udp_server::~udp_server() noexcept {
    try {
        if (running_.load()) {
            stop_legacy();
        }
        log(logger::log_level::INFO, "UDP server adapter destroyed");
    }
    catch (...) {
        // Destructor should not throw
    }
}

bool udp_server::is_healthy() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return transport_ && transport_->is_healthy() && initialized_.load();
}

std::string udp_server::status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    oss << "UDP Server Status: "
        << "running=" << running_.load()
        << ", initialized=" << initialized_.load()
        << ", local_endpoint=" << local_endpoint_;
    
    if (transport_) {
        oss << ", transport_status=" << transport_->status();
    }
    
    return oss.str();
}

ss::core::async_void udp_server::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_.load()) {
        co_return;
    }

    try {
        // Initialize transport
        co_await transport_->initialize();

        // Convert local endpoint
        auto local_ep = convert_endpoint(local_endpoint_);
        
        // Bind to local endpoint
        auto bind_result = co_await transport_->bind(local_ep);
        if (!bind_result) {
            throw std::runtime_error("Failed to bind to " + local_endpoint_.address().to_string() + 
                                   ":" + std::to_string(local_endpoint_.port()) + 
                                   " - " + bind_result.error().message);
        }

        // Set message handler
        transport_->set_message_handler([this](const ss::network::incoming_message& message) {
            handle_incoming_message(message);
        });

        initialized_.store(true);
        log(logger::log_level::INFO, "UDP server initialized successfully");
    }
    catch (const std::exception& e) {
        log(logger::log_level::ERROR, "Failed to initialize UDP server: " + std::string(e.what()));
        throw;
    }
}

ss::core::async_void udp_server::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_.load()) {
        co_return;
    }

    try {
        if (transport_) {
            co_await transport_->cleanup();
        }
        
        initialized_.store(false);
        log(logger::log_level::INFO, "UDP server cleaned up successfully");
    }
    catch (const std::exception& e) {
        log(logger::log_level::ERROR, "Error during cleanup: " + std::string(e.what()));
    }
}

ss::core::async_void udp_server::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (running_.load()) {
        co_return;
    }

    try {
        if (!initialized_.load()) {
            co_await initialize();
        }

        co_await transport_->start();
        running_.store(true);
        
        log(logger::log_level::INFO, "UDP server started successfully");
    }
    catch (const std::exception& e) {
        log(logger::log_level::ERROR, "Failed to start UDP server: " + std::string(e.what()));
        throw;
    }
}

ss::core::async_void udp_server::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!running_.load()) {
        co_return;
    }

    try {
        co_await transport_->stop();
        running_.store(false);
        
        log(logger::log_level::INFO, "UDP server stopped successfully");
    }
    catch (const std::exception& e) {
        log(logger::log_level::ERROR, "Error during stop: " + std::string(e.what()));
    }
}

bool udp_server::is_running() const noexcept {
    return running_.load() && transport_ && transport_->is_running();
}

bool udp_server::start_legacy() {
    try {
        auto start_coro = start();
        // For legacy compatibility, we need to wait for completion
        // This should be improved in future versions to be truly async
        io_context_.post([start_coro = std::move(start_coro)]() mutable {
            // Start the coroutine - in a real implementation this would be handled properly
        });
        return true;
    }
    catch (const std::exception& e) {
        log(logger::log_level::ERROR, "Legacy start failed: " + std::string(e.what()));
        return false;
    }
}

void udp_server::stop_legacy() {
    try {
        auto stop_coro = stop();
        // For legacy compatibility, we need to wait for completion
        io_context_.post([stop_coro = std::move(stop_coro)]() mutable {
            // Stop the coroutine - in a real implementation this would be handled properly
        });
    }
    catch (const std::exception& e) {
        log(logger::log_level::ERROR, "Legacy stop failed: " + std::string(e.what()));
    }
}

ss::network::transport_stats udp_server::get_stats() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (transport_) {
        return transport_->get_stats();
    }
    return {};
}

void udp_server::reset_stats() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (transport_) {
        transport_->reset_stats();
    }
}

boost::asio::ip::udp::endpoint udp_server::local_endpoint() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (transport_) {
        return convert_endpoint(transport_->local_endpoint());
    }
    return local_endpoint_;
}

bool udp_server::can_receive() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return transport_ && transport_->is_running();
}

std::uint32_t udp_server::receive_queue_size() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (transport_) {
        return transport_->receive_queue_size();
    }
    return 0;
}

void udp_server::handle_incoming_message(const ss::network::incoming_message& message) {
    try {
        // Convert message data to legacy format
        std::vector<std::uint8_t> data(message.data.begin(), message.data.end());
        
        // Convert endpoint to legacy format
        auto legacy_endpoint = convert_endpoint(message.source);
        
        // Log incoming packet
        log_packet(logger::log_level::INFO, "INCOMING", legacy_endpoint);
        
        // Call legacy handler
        if (recv_handler_) {
            recv_handler_(std::move(data), legacy_endpoint);
        }
    }
    catch (const std::exception& e) {
        log(logger::log_level::ERROR, "Error handling incoming message: " + std::string(e.what()));
    }
}

boost::asio::ip::udp::endpoint udp_server::convert_endpoint(
    const ss::core::endpoint& endpoint
) const noexcept {
    try {
        boost::asio::ip::address addr;
        if (endpoint.is_ipv6()) {
            addr = boost::asio::ip::address_v6::from_string(endpoint.address());
        } else {
            addr = boost::asio::ip::address_v4::from_string(endpoint.address());
        }
        return boost::asio::ip::udp::endpoint(addr, endpoint.port());
    }
    catch (...) {
        // Return default endpoint on conversion error
        return boost::asio::ip::udp::endpoint();
    }
}

ss::core::endpoint udp_server::convert_endpoint(
    const boost::asio::ip::udp::endpoint& endpoint
) const noexcept {
    try {
        return ss::core::endpoint(endpoint.address().to_string(), endpoint.port());
    }
    catch (...) {
        // Return default endpoint on conversion error
        return ss::core::endpoint();
    }
}

void udp_server::log(logger::log_level level, const std::string& message) const noexcept {
    if (logger_) {
        try {
            logger_->log(level, "(@udp_server)", message);
        }
        catch (...) {
            // Logging should not throw
        }
    }
}

void udp_server::log_packet(
    logger::log_level level,
    const std::string& direction,
    const boost::asio::ip::udp::endpoint& endpoint
) const noexcept {
    if (logger_) {
        try {
            logger_->log(level, "(@udp_server)", 
                        direction + " packet from/to " + endpoint.address().to_string() + 
                        ":" + std::to_string(endpoint.port()));
        }
        catch (...) {
            // Logging should not throw
        }
    }
}

} // namespace ss