#include <ss_p2p/node_controller.hpp>
#include <ss_p2p/application/i_p2p_node.hpp>
#include <ss_p2p/core/types.hpp>
#include <ss_p2p/core/result.hpp>
#include <ss_p2p/ss_logger.hpp>
#include <utils.hpp>

#include <stdexcept>
#include <chrono>
#include <iostream>
#include <cassert>

namespace ss {

// ========================================
// Constructor and Destructor
// ========================================

node_controller::node_controller(
    ip::udp::endpoint& self_ep,
    std::shared_ptr<io_context> io_ctx
) : io_context_(io_ctx)
  , local_endpoint_(self_ep)
  , global_endpoint_(ip::address::from_string("0.0.0.0"), 0)
  , running_(false)
  , initialized_(false)
{
    try {
        initialize_p2p_node();
    } catch (const std::exception& e) {
        set_error("Failed to initialize P2P node: " + std::string(e.what()));
        throw;
    }
}

node_controller::~node_controller() {
    try {
        cleanup();
    } catch (const std::exception& e) {
        // Log error but don't throw from destructor
        std::cerr << "Error during node_controller cleanup: " << e.what() << std::endl;
    }
}

// Move constructor
node_controller::node_controller(node_controller&& other) noexcept
    : p2p_node_(std::move(other.p2p_node_))
    , io_context_(std::move(other.io_context_))
    , local_endpoint_(other.local_endpoint_)
    , global_endpoint_(other.global_endpoint_)
    , running_(other.running_.load())
    , initialized_(other.initialized_.load())
    , daemon_thread_(std::move(other.daemon_thread_))
    , tick_timer_(std::move(other.tick_timer_))
    , socket_manager_(std::move(other.socket_manager_))
    , message_pool_(std::move(other.message_pool_))
    , message_hub_(std::move(other.message_hub_))
    , last_error_(std::move(other.last_error_))
{
    other.running_.store(false);
    other.initialized_.store(false);
}

// Move assignment operator
node_controller& node_controller::operator=(node_controller&& other) noexcept {
    if (this != &other) {
        cleanup();
        
        p2p_node_ = std::move(other.p2p_node_);
        io_context_ = std::move(other.io_context_);
        local_endpoint_ = other.local_endpoint_;
        global_endpoint_ = other.global_endpoint_;
        running_.store(other.running_.load());
        initialized_.store(other.initialized_.load());
        daemon_thread_ = std::move(other.daemon_thread_);
        tick_timer_ = std::move(other.tick_timer_);
        socket_manager_ = std::move(other.socket_manager_);
        message_pool_ = std::move(other.message_pool_);
        message_hub_ = std::move(other.message_hub_);
        last_error_ = std::move(other.last_error_);
        
        other.running_.store(false);
        other.initialized_.store(false);
    }
    return *this;
}

// ========================================
// Legacy API Implementation
// ========================================

const ip::udp::socket& node_controller::self_sock() {
    std::shared_lock lock(state_mutex_);
    
    if (!socket_manager_) {
        setup_legacy_objects();
    }
    
    return socket_manager_->get_socket();
}

void node_controller::init(std::vector<ip::udp::endpoint> boot_eps) {
    std::unique_lock lock(state_mutex_);
    
    if (initialized_.load()) {
        return; // Already initialized
    }
    
    try {
        if (!p2p_node_) {
            throw std::runtime_error("P2P node not available");
        }
        
        // Create configuration for the new architecture
        application::node_config config;
        config.local_endpoint = core::endpoint(local_endpoint_.address().to_string(), local_endpoint_.port());
        
        // Convert bootstrap endpoints
        for (const auto& ep : boot_eps) {
            config.bootstrap_nodes.emplace_back(ep.address().to_string(), ep.port());
        }
        
        config.enable_nat_traversal = true;
        config.enable_encryption = true;
        config.max_connections = 1000;
        config.connection_timeout = std::chrono::milliseconds(5000);
        config.dht_refresh_interval = std::chrono::milliseconds(60000);
        
        // Initialize the P2P node using coroutine (block until complete)
        auto init_future = boost::asio::co_spawn(*io_context_, 
            p2p_node_->initialize(config), 
            boost::asio::use_future);
        
        init_future.get(); // Block until initialization completes
        
        initialized_.store(true);
        
    } catch (const std::exception& e) {
        set_error("Initialization failed: " + std::string(e.what()));
        throw;
    }
}

void node_controller::start(std::vector<ip::udp::endpoint> boot_eps) {
    std::unique_lock lock(state_mutex_);
    
    if (running_.load()) {
        return; // Already running
    }
    
    try {
        // Initialize if not already done
        if (!initialized_.load()) {
            lock.unlock();
            init(boot_eps);
            lock.lock();
        }
        
        if (!p2p_node_) {
            throw std::runtime_error("P2P node not available");
        }
        
        // Start the daemon thread that runs the IO context
        daemon_thread_ = std::thread([this, boot_eps]() {
            try {
                // Start the P2P node
                auto start_future = boost::asio::co_spawn(*io_context_, 
                    p2p_node_->start(), 
                    boost::asio::use_future);
                
                start_future.get(); // Block until start completes
                
                // Join network if bootstrap nodes provided
                if (!boot_eps.empty()) {
                    auto bootstrap_endpoints = convert_endpoints(boot_eps);
                    auto join_future = boost::asio::co_spawn(*io_context_, 
                        p2p_node_->join_network(bootstrap_endpoints), 
                        boost::asio::use_future);
                    join_future.get();
                }
                
                // Start periodic maintenance (legacy tick functionality)
                call_tick();
                
                // Run the IO context
                io_context_->run();
                
            } catch (const std::exception& e) {
                set_error("Daemon thread error: " + std::string(e.what()));
                running_.store(false);
            }
        });
        
        running_.store(true);
        
        // Setup legacy timer
        tick_timer_ = std::make_unique<boost::asio::deadline_timer>(*io_context_);
        
        last_error_.clear();
        
    } catch (const std::exception& e) {
        set_error("Start failed: " + std::string(e.what()));
        throw;
    }
}

void node_controller::stop() {
    std::unique_lock lock(state_mutex_);
    
    if (!running_.load()) {
        return; // Already stopped
    }
    
    try {
        running_.store(false);
        
        // Cancel tick timer
        if (tick_timer_) {
            tick_timer_->cancel();
        }
        
        // Stop P2P node
        if (p2p_node_) {
            auto stop_future = boost::asio::co_spawn(*io_context_, 
                p2p_node_->stop(), 
                boost::asio::use_future);
            
            // Wait for stop with timeout
            if (stop_future.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
                std::cerr << "Warning: P2P node stop timeout" << std::endl;
            }
        }
        
        // Stop IO context
        io_context_->stop();
        
        // Join daemon thread
        if (daemon_thread_.joinable()) {
            daemon_thread_.join();
        }
        
        last_error_.clear();
        
    } catch (const std::exception& e) {
        set_error("Stop failed: " + std::string(e.what()));
    }
}

peer node_controller::get_peer(const ip::udp::endpoint& ep) {
    std::shared_lock lock(state_mutex_);
    
    if (!message_pool_) {
        setup_legacy_objects();
    }
    
    // Create a legacy peer object that interfaces with the new architecture
    // This is a compatibility shim - the actual peer communication goes through the new system
    return peer(ep, message_pool_->get_peer_message_buffer(peer::calc_peer_id(ep)), nullptr);
}

peer::ref node_controller::get_peer_ref(const ip::udp::endpoint ep) {
    std::shared_lock lock(state_mutex_);
    
    if (!message_pool_) {
        setup_legacy_objects();
    }
    
    // Create a shared peer reference for compatibility
    return std::make_shared<peer>(ep, message_pool_->get_peer_message_buffer(peer::calc_peer_id(ep)), nullptr);
}

udp_socket_manager& node_controller::get_socket_manager() {
    std::shared_lock lock(state_mutex_);
    
    if (!socket_manager_) {
        setup_legacy_objects();
    }
    
    return *socket_manager_;
}

kademlia::direct_routing_table_controller& node_controller::get_direct_routing_table_controller() {
    // This method requires access to the old kademlia system
    // For now, throw an exception indicating this needs to be migrated
    throw std::runtime_error("get_direct_routing_table_controller() not available in new architecture - please migrate to new API");
}

message_pool::message_hub& node_controller::get_message_hub() {
    std::shared_lock lock(state_mutex_);
    
    if (!message_hub_) {
        setup_legacy_objects();
    }
    
    return *message_hub_;
}

multicast_manager node_controller::get_multicast_manager() {
    // Multicast manager requires routing table access
    // For compatibility, create a stub implementation
    throw std::runtime_error("get_multicast_manager() not available in new architecture - please migrate to new API");
}

void node_controller::update_global_self_endpoint(ip::udp::endpoint ep) {
    std::unique_lock lock(state_mutex_);
    
    global_endpoint_ = ep;
    
    // The new architecture handles global endpoint management internally
    // This is kept for compatibility but doesn't need to do much
}

void node_controller::on_command_input(std::vector<std::string> inputs) {
    if (inputs.empty()) return;
    
    auto command = inputs[0];
    
    if (command == "stop") {
        stop();
        return;
    }
    
#ifdef SS_DEBUG
    std::cout << "### ";
    for (const auto& input : inputs) {
        std::cout << input << " ";
    }
    std::cout << std::endl;
#endif
    
    if (command == "send" && inputs.size() >= 3) {
        try {
            auto peer_addr_str = inputs[1];
            auto peer_ep = str_to_endpoint(peer_addr_str);
            auto payload = inputs[2];
            
            // Use new architecture to send message
            if (p2p_node_ && running_.load()) {
                ss::message msg;
                msg.set_app_id(_id);
                msg.set_param("payload", payload);
                
                auto send_future = boost::asio::co_spawn(*io_context_,
                    p2p_node_->send_message(msg, core::endpoint(peer_ep.address().to_string(), peer_ep.port())),
                    boost::asio::use_future);
                
                // Don't block - fire and forget for compatibility
                boost::asio::post(*io_context_, [send_future = std::move(send_future)]() mutable {
                    try {
                        send_future.get();
                    } catch (const std::exception& e) {
                        std::cerr << "Send message error: " << e.what() << std::endl;
                    }
                });
            }
        } catch (const std::exception& e) {
            std::cerr << "Command send error: " << e.what() << std::endl;
        }
    }
}

std::optional<ip::udp::endpoint> node_controller::sync_get_global_address(std::vector<ip::udp::endpoint> boot_eps) {
    std::shared_lock lock(state_mutex_);
    
    if (!p2p_node_ || !running_.load()) {
        return std::nullopt;
    }
    
    try {
        // Use ping to bootstrap nodes to discover global address
        if (!boot_eps.empty()) {
            auto bootstrap_ep = boot_eps[0];
            auto ping_future = boost::asio::co_spawn(*io_context_,
                p2p_node_->ping_peer(core::endpoint(bootstrap_ep.address().to_string(), bootstrap_ep.port())),
                boost::asio::use_future);
            
            auto latency = ping_future.get();
            if (latency.count() > 0) {
                // If ping succeeded, return our configured local endpoint as global
                // The new architecture handles NAT traversal internally
                return local_endpoint_;
            }
        }
        
        return std::nullopt;
        
    } catch (const std::exception& e) {
        set_error("Global address discovery failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

// ========================================
// Debug Interface (SS_DEBUG only)
// ========================================

#ifdef SS_DEBUG
kademlia::k_routing_table& node_controller::get_routing_table() {
    throw std::runtime_error("get_routing_table() not available in new architecture - use get_peers() instead");
}

ice::ice_agent& node_controller::get_ice_agent() {
    throw std::runtime_error("get_ice_agent() not available in new architecture - NAT traversal is handled internally");
}

kademlia::dht_manager& node_controller::get_dht_manager() {
    throw std::runtime_error("get_dht_manager() not available in new architecture - use find_peers() instead");
}

message_pool& node_controller::get_message_pool() {
    std::shared_lock lock(state_mutex_);
    
    if (!message_pool_) {
        setup_legacy_objects();
    }
    
    return *message_pool_;
}
#endif

// ========================================
// Additional API for compatibility
// ========================================

bool node_controller::is_running() const noexcept {
    return running_.load();
}

ip::udp::endpoint node_controller::get_self_endpoint() const noexcept {
    std::shared_lock lock(state_mutex_);
    return local_endpoint_;
}

// ========================================
// Protected Methods
// ========================================

void node_controller::tick() {
    if (!running_.load()) {
        return;
    }
    
    try {
        // Perform maintenance using new architecture
        if (p2p_node_) {
            boost::asio::co_spawn(*io_context_, 
                p2p_node_->perform_maintenance(), 
                boost::asio::detached);
        }
        
        // Schedule next tick
        call_tick();
        
    } catch (const std::exception& e) {
        set_error("Tick error: " + std::string(e.what()));
    }
}

void node_controller::call_tick(std::time_t tick_time_s) {
    if (!tick_timer_ || !running_.load()) {
        return;
    }
    
    tick_timer_->expires_from_now(boost::posix_time::seconds(tick_time_s));
    tick_timer_->async_wait([this](const boost::system::error_code& ec) {
        if (!ec && running_.load()) {
            tick();
        }
    });
}

void node_controller::on_receive_packet(std::vector<std::uint8_t> raw_msg, ip::udp::endpoint& ep) {
    // This is handled internally by the new architecture
    // Keep for compatibility but delegate to message handlers if needed
    
    try {
        auto msg = message::decode(raw_msg);
        
        // Store in message pool for legacy compatibility
        if (message_pool_) {
            auto shared_msg = std::make_shared<message>(msg);
            message_pool_->store(shared_msg, ep);
        }
        
    } catch (const std::exception& e) {
        set_error("Packet processing error: " + std::string(e.what()));
    }
}

void node_controller::requires_routing(bool b) {
    // This was used to enable/disable DHT routing in the old architecture
    // In the new architecture, this is controlled by configuration
    // Keep for compatibility but it doesn't need to do anything
}

// ========================================
// Private Helper Methods
// ========================================

void node_controller::initialize_p2p_node() {
    if (!io_context_) {
        throw std::runtime_error("IO context not available");
    }
    
    // Create the new modular P2P node
    application::node_config config;
    config.local_endpoint = core::endpoint(local_endpoint_.address().to_string(), local_endpoint_.port());
    config.enable_nat_traversal = true;
    config.enable_encryption = true;
    
    p2p_node_ = application::create_p2p_node(*io_context_, config);
    
    if (!p2p_node_) {
        throw std::runtime_error("Failed to create P2P node");
    }
}

std::vector<core::endpoint> node_controller::convert_endpoints(
    const std::vector<ip::udp::endpoint>& legacy_endpoints
) const {
    std::vector<core::endpoint> endpoints;
    endpoints.reserve(legacy_endpoints.size());
    
    for (const auto& ep : legacy_endpoints) {
        endpoints.emplace_back(ep.address().to_string(), ep.port());
    }
    
    return endpoints;
}

void node_controller::setup_legacy_objects() {
    // Lazy initialization of legacy compatibility objects
    if (!socket_manager_) {
        socket_manager_ = std::make_unique<udp_socket_manager>(local_endpoint_, std::ref(*io_context_));
    }
    
    if (!message_pool_) {
        message_pool_ = std::make_unique<message_pool>(*io_context_, 
            std::bind(&node_controller::get_peer_ref, this, std::placeholders::_1), 
            nullptr, true);
    }
    
    if (!message_hub_) {
        message_hub_ = std::make_unique<message_pool::message_hub>(
            message_pool_->get_message_hub());
    }
}

void node_controller::cleanup() {
    if (running_.load()) {
        stop();
    }
    
    // Clean up legacy objects
    message_hub_.reset();
    message_pool_.reset();
    socket_manager_.reset();
    tick_timer_.reset();
    
    // Clean up P2P node
    p2p_node_.reset();
    
    initialized_.store(false);
}

void node_controller::set_error(const std::string& error) const {
    std::unique_lock lock(state_mutex_);
    last_error_ = error;
    std::cerr << "node_controller error: " << error << std::endl;
}

} // namespace ss