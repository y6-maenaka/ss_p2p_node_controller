#include <ss_p2p/ice_agent/ice_agent.hpp>
#include <ss_p2p/observer.hpp>
#include <ss_p2p/core/result.hpp>
#include <ss_p2p/ice/i_nat_traversal.hpp>
#include <ss_p2p/ice/i_stun_client.hpp>
#include <chrono>
#include <thread>

namespace ss
{
namespace ice
{

// Implementation class that wraps the new ICE module
class ice_agent::impl 
{
public:
    impl(asio::io_context& io_ctx, const core::endpoint& local_ep, ss_logger* logger)
        : io_context(io_ctx)
        , local_endpoint(local_ep)
        , logger(logger)
    {
        // Initialize new ICE module components here
        // Note: Actual initialization would depend on the new module's API
    }
    
    ~impl() = default;
    
    // Process ICE message using new architecture
    core::result<void, std::string> process_message(
        const ice_message& msg,
        const core::endpoint& from_ep)
    {
        try {
            // Delegate to new ICE module
            // This is a placeholder - actual implementation would use new module
            return core::result<void, std::string>::success();
        }
        catch (const std::exception& e) {
            return core::result<void, std::string>::error(
                std::string("ICE processing error: ") + e.what()
            );
        }
    }
    
    void update_global_endpoint(const core::endpoint& ep)
    {
        global_endpoint = ep;
    }
    
private:
    asio::io_context& io_context;
    core::endpoint local_endpoint;
    core::endpoint global_endpoint;
    ss_logger* logger;
    
    // New ICE module components would be here
    // std::unique_ptr<ice::i_nat_traversal> nat_traversal;
    // std::unique_ptr<ice::i_stun_client> stun_client;
};

// Constructor - initialize legacy components and new implementation
ice_agent::ice_agent(
    asio::io_context &io_ctx,
    udp_socket_manager &sock_manager,
    ip::udp::endpoint &glob_self_ep,
    message::app_id id,
    sender &sender,
    ss::kademlia::direct_routing_table_controller &d_routing_table_controller,
    ss_logger *logger)
    : _sock_manager(sock_manager)
    , _glob_self_ep(glob_self_ep)
    , _app_id(id)
    , _sender(sender)
    , _ice_sender(sock_manager, glob_self_ep, id)
    , _obs_storage(io_ctx)
    , _sgnl_server(io_ctx, sender, _ice_sender, glob_self_ep, d_routing_table_controller, _obs_storage, logger)
    , _stun_server(io_ctx, sender, _ice_sender, d_routing_table_controller, _obs_storage, logger)
    , _logger(logger)
{
    // Convert legacy endpoint to new format
    core::endpoint local_ep{
        glob_self_ep.address().to_string(),
        glob_self_ep.port()
    };
    
    // Initialize new implementation
    pImpl = std::make_unique<impl>(io_ctx, local_ep, logger);
    
    if (_logger) {
        _logger->log(ss_logger::log_level::INFO, 
                    "(@ice_agent)", 
                    "Initialized with new architecture adapter");
    }
}

// Destructor
ice_agent::~ice_agent() = default;

// Move constructor
ice_agent::ice_agent(ice_agent&&) noexcept = default;

// Move assignment
ice_agent& ice_agent::operator=(ice_agent&&) noexcept = default;

// Process incoming message
int ice_agent::income_message(std::shared_ptr<message> msg, ip::udp::endpoint &ep)
{
    std::unique_lock<std::shared_mutex> lock(_mtx);
    
    // Update metrics
    _messages_processed++;
    
    // Check for ICE parameters
    auto ice_param = msg->get_param("ice_agent");
    if (!ice_param) {
        return 0; // Not an ICE message
    }
    
    ice_message ice_msg(*ice_param);
    const auto protocol = ice_msg.get_protocol();
    
    // Process using legacy system for compatibility
    auto call_observer_income_message = [&](auto &obs) {
        return obs->income_message(*msg, ep);
    };
    
    if (protocol == ice_message::protocol_t::signaling) {
        _signaling_messages++;
        
        observer_id obs_id = ice_msg.get_observer_id();
        
        // Try to find observers in order
        if (auto obs_vec = _obs_storage.find_observer<signaling_relay>(obs_id); !obs_vec.empty()) {
            return call_observer_income_message(*obs_vec.begin());
        }
        if (auto obs_vec = _obs_storage.find_observer<signaling_request>(obs_id); !obs_vec.empty()) {
            return call_observer_income_message(*obs_vec.begin());
        }
        if (auto obs_vec = _obs_storage.find_observer<signaling_response>(obs_id); !obs_vec.empty()) {
            return call_observer_income_message(*obs_vec.begin());
        }
        
        // Fall back to signaling server
        _sgnl_server.income_message(msg, ep);
    }
    else if (protocol == ice_message::protocol_t::stun) {
        _stun_messages++;
        
        observer_id obs_id = ice_msg.get_observer_id();
        
        if (auto obs_vec = _obs_storage.find_observer<binding_request>(obs_id); !obs_vec.empty()) {
            return call_observer_income_message(*obs_vec.begin());
        }
        
        _stun_server.income_message(msg, ep);
    }
    
    // Also process with new implementation
    core::endpoint from_ep{ep.address().to_string(), ep.port()};
    auto result = pImpl->process_message(ice_msg, from_ep);
    
    if (result.is_error() && _logger) {
        _logger->log(ss_logger::log_level::WARNING,
                    "(@ice_agent)",
                    "New ICE processing error:",
                    result.error().c_str());
    }
    
    return 0;
}

// Get signaling send function
signaling_server::s_send_func ice_agent::get_signaling_send_func()
{
    std::shared_lock<std::shared_mutex> lock(_mtx);
    return _sgnl_server.get_signaling_send_func();
}

// Update global endpoint
void ice_agent::update_global_self_endpoint(ip::udp::endpoint &ep)
{
    std::unique_lock<std::shared_mutex> lock(_mtx);
    
    ip::udp::endpoint prev_global_self_ep = _glob_self_ep;
    _glob_self_ep = ep;
    
    // Update new implementation
    core::endpoint new_ep{ep.address().to_string(), ep.port()};
    pImpl->update_global_endpoint(new_ep);
    
    if (_logger) {
        _logger->log(ss_logger::log_level::INFO,
                    "(@ice_agent)",
                    "Updated global self endpoint",
                    endpoint_to_str(prev_global_self_ep).c_str(),
                    " -> ",
                    endpoint_to_str(_glob_self_ep).c_str());
    }
}

// Get STUN server
stun_server& ice_agent::get_stun_server()
{
    std::shared_lock<std::shared_mutex> lock(_mtx);
    return _stun_server;
}

// Get ICE sender
ice_sender& ice_agent::get_ice_sender()
{
    std::shared_lock<std::shared_mutex> lock(_mtx);
    return _ice_sender;
}

#if SS_DEBUG
// Get observer storage (debug only)
ice_observer_strage& ice_agent::get_observer_strage()
{
    std::shared_lock<std::shared_mutex> lock(_mtx);
    return _obs_storage;
}
#endif

} // namespace ice
} // namespace ss