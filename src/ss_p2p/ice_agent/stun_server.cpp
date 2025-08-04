#include <ss_p2p/ice_agent/stun_server.hpp>
#include <ss_p2p/ice_agent/ice_observer.hpp>
#include <ss_p2p/core/result.hpp>
#include <ss_p2p/ice/i_stun_client.hpp>
#include <ss_p2p/ice/factory.hpp>
#include <ss_p2p/network/factory.hpp>
#include <ss_p2p/utils.hpp>
#include <chrono>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <future>
#include <random>
#include <algorithm>

namespace ss
{
namespace ice
{

// sr_object implementation
sr_object::sr_object() : _state(pending), _is_async(false)
{
}

std::optional<ip::udp::endpoint> sr_object::sync_get()
{
    if (_future_result.valid()) {
        // Wait for new architecture result
        auto result = _future_result.get();
        if (result.is_success()) {
            auto ep = result.value();
            _global_ep = ip::udp::endpoint(
                ip::address::from_string(ep.address),
                ep.port
            );
            _state = done;
            return {_global_ep};
        } else {
            _state = notfound;
            return {};
        }
    }
    
    // Legacy fallback
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _cv.wait(lock, [this] { return _state.load() != pending; });
    }
    
    if (this->get_state() == state_t::done) {
        return {_global_ep};
    } else {
        return {};
    }
}

void sr_object::async_get(on_nat_traversal_success_handler handler)
{
    this->handler = handler;
    _is_async = true;
    
    if (_future_result.valid()) {
        // Check if already complete
        if (_future_result.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto result = _future_result.get();
            if (result.is_success()) {
                auto ep = result.value();
                _global_ep = ip::udp::endpoint(
                    ip::address::from_string(ep.address),
                    ep.port
                );
                _state = done;
                handler({_global_ep});
            } else {
                _state = notfound;
                handler({});
            }
        }
    }
}

sr_object sr_object::_error_()
{
    sr_object sr_obj;
    sr_obj._state = notfound;
    return sr_obj;
}

sr_object sr_object::_pending_()
{
    sr_object sr_obj;
    sr_obj._state = pending;
    return sr_obj;
}

sr_object::state_t sr_object::get_state() const
{
    return _state.load();
}

void sr_object::update_state(state_t s, std::optional<ip::udp::endpoint> ep)
{
    std::lock_guard<std::mutex> lock(_mtx);
    _state = s;
    if (ep.has_value()) {
        _global_ep = ep.value();
    }
    _cv.notify_all();
    
    // Call async handler if set
    if (_is_async && handler && s != pending) {
        if (s == done) {
            handler({_global_ep});
        } else {
            handler({});
        }
    }
}

// Implementation class that wraps the new STUN client
class stun_server::impl 
{
public:
    impl(asio::io_context& io_ctx, ss_logger* logger)
        : io_context(io_ctx)
        , logger(logger)
    {
        // Create transport for STUN operations
        auto transport_factory = ss::network::get_transport_factory();
        transport = transport_factory->create_udp_transport(io_context);
        
        // Create STUN client
        auto ice_factory = ss::ice::get_ice_factory();
        stun_client = ice_factory->create_stun_client(transport);
        
        // Initialize with default configuration
        default_config.software = "ss_p2p_stun_server/1.0";
        default_config.enable_fingerprint = true;
        default_config.max_retries = 3;
        default_config.request_timeout = std::chrono::milliseconds(3000);
    }
    
    ~impl() = default;
    
    // Send binding request using new architecture
    std::future<core::result<core::endpoint, std::string>> 
    send_binding_request(const std::vector<ip::udp::endpoint>& servers)
    {
        // Convert to new endpoint format
        std::vector<core::endpoint> new_servers;
        for (const auto& ep : servers) {
            new_servers.push_back(core::endpoint{
                ep.address().to_string(),
                ep.port()
            });
        }
        
        // Use promise/future for async operation
        auto promise = std::make_shared<std::promise<core::result<core::endpoint, std::string>>>();
        auto future = promise->get_future();
        
        // Pick random server
        if (!new_servers.empty()) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, new_servers.size() - 1);
            auto server = new_servers[dis(gen)];
            
            // Get local endpoint from transport
            auto local_ep_result = transport->get_local_endpoint();
            if (local_ep_result.is_error()) {
                promise->set_value(core::result<core::endpoint, std::string>::error(
                    "Failed to get local endpoint: " + local_ep_result.error()
                ));
                return future;
            }
            
            // Send async binding request
            stun_client->binding_request_async(
                server,
                local_ep_result.value(),
                [promise](core::result<stun_binding_result, stun_client_error> result) {
                    if (result.is_success()) {
                        promise->set_value(core::result<core::endpoint, std::string>::success(
                            result.value().mapped_address
                        ));
                    } else {
                        promise->set_value(core::result<core::endpoint, std::string>::error(
                            to_string(result.error())
                        ));
                    }
                },
                default_config
            );
        } else {
            promise->set_value(core::result<core::endpoint, std::string>::error(
                "No STUN servers provided"
            ));
        }
        
        return future;
    }
    
    // Process incoming STUN message
    core::result<void, std::string> process_stun_message(
        const message& msg,
        const ip::udp::endpoint& from_ep)
    {
        // Extract STUN data from message
        auto stun_data = msg.get_param("stun_data");
        if (!stun_data) {
            return core::result<void, std::string>::error("No STUN data in message");
        }
        
        // Convert to binary data
        std::vector<uint8_t> data;
        // Note: Actual conversion would depend on message format
        
        // Parse and validate STUN message
        core::endpoint sender{from_ep.address().to_string(), from_ep.port()};
        auto parse_result = stun_client->parse_message(data, sender);
        
        if (parse_result.is_error()) {
            return core::result<void, std::string>::error(
                "Failed to parse STUN message: " + to_string(parse_result.error())
            );
        }
        
        // Handle based on message type
        auto stun_msg = parse_result.value();
        switch (stun_msg.type) {
            case stun_message_type::binding_request:
                // In P2P mode, we act as STUN server for others
                return handle_binding_request(stun_msg, sender);
                
            case stun_message_type::binding_response:
                // Response handled by observers
                return core::result<void, std::string>::success();
                
            default:
                return core::result<void, std::string>::error(
                    "Unsupported STUN message type: " + to_string(stun_msg.type)
                );
        }
    }
    
private:
    // Handle incoming binding request (act as STUN server)
    core::result<void, std::string> handle_binding_request(
        const stun_message& request,
        const core::endpoint& from_ep)
    {
        // Create binding response
        stun_message response(
            stun_message_type::binding_response,
            request.transaction_id
        );
        
        // Add XOR-MAPPED-ADDRESS attribute
        auto xor_addr = stun_client->create_xor_mapped_address(
            from_ep,
            request.transaction_id
        );
        response.add_attribute(stun_attribute(
            stun_attribute_type::xor_mapped_address,
            xor_addr
        ));
        
        // Add SOFTWARE attribute
        std::string software = default_config.software;
        response.add_attribute(stun_attribute(
            stun_attribute_type::software,
            std::vector<uint8_t>(software.begin(), software.end())
        ));
        
        // Encode response
        auto encode_result = stun_client->encode_message(response, default_config);
        if (encode_result.is_error()) {
            return core::result<void, std::string>::error(
                "Failed to encode STUN response: " + to_string(encode_result.error())
            );
        }
        
        // Send response back
        auto send_result = transport->send_to(
            encode_result.value(),
            from_ep
        );
        
        if (send_result.is_error()) {
            return core::result<void, std::string>::error(
                "Failed to send STUN response: " + send_result.error()
            );
        }
        
        return core::result<void, std::string>::success();
    }
    
public:
    asio::io_context& io_context;
    ss_logger* logger;
    
    // New architecture components
    network::transport_ptr transport;
    stun_client_ptr stun_client;
    stun_client_config default_config;
    
    // Metrics
    std::atomic<uint64_t> binding_requests_sent{0};
    std::atomic<uint64_t> binding_responses_received{0};
    std::atomic<uint64_t> request_timeouts{0};
};

// Constructor
stun_server::stun_server(
    asio::io_context &io_ctx,
    class sender &sender,
    class ice_sender &ice_sender,
    ss::kademlia::direct_routing_table_controller &d_routing_table_controller,
    ice_observer_strage &obs_storage,
    ss_logger *logger)
    : _io_ctx(io_ctx)
    , _sender(sender)
    , _ice_sender(ice_sender)
    , _d_routing_table_controller(d_routing_table_controller)
    , _obs_storage(obs_storage)
    , _logger(logger)
{
    pImpl = std::make_unique<impl>(io_ctx, logger);
    
    if (_logger) {
        _logger->log(ss_logger::log_level::INFO,
                    "(@stun_server)",
                    "Initialized with new architecture adapter");
    }
}

// Destructor
stun_server::~stun_server() = default;

// Move constructor
stun_server::stun_server(stun_server&&) noexcept = default;

// Move assignment
stun_server& stun_server::operator=(stun_server&&) noexcept = default;

int stun_server::income_message(std::shared_ptr<message> msg, ip::udp::endpoint &ep)
{
    std::unique_lock<std::shared_mutex> lock(_mtx);
    
    // Update metrics
    _responses_received++;
    
    // Extract ICE message
    auto ice_param = msg->get_param("ice_agent");
    if (!ice_param) {
        return -1; // Not an ICE message
    }
    
    ice_message ice_msg(ice_param.value());
    observer_id obs_id = ice_msg.get_observer_id();
    
    // Try to find existing observer
    auto observers = _obs_storage.find_observer<binding_response>(obs_id);
    if (!observers.empty()) {
        auto br = observers.front();
        return br->income_message(*msg, ep);
    }
    
    // Process with new implementation
    auto result = pImpl->process_stun_message(*msg, ep);
    if (result.is_error() && _logger) {
        _logger->log(ss_logger::log_level::WARNING,
                    "(@stun_server)",
                    "STUN processing error:",
                    result.error().c_str());
    }
    
    // Create legacy observer for compatibility
    binding_response br(&_io_ctx, &_sender, &_ice_sender, &_d_routing_table_controller, 
                       obs_id, &_obs_storage, _logger);
    return br.income_message(*msg, ep);
}

void stun_server::on_send_done(const boost::system::error_code &ec, std::size_t bytes_transferred)
{
    if (ec && _logger) {
        _logger->log(ss_logger::log_level::WARNING,
                    "(@stun_server)",
                    "Send error:",
                    ec.message().c_str());
    }
    (void)(bytes_transferred);
}

stun_server::sr_ptr stun_server::binding_request(std::vector<ip::udp::endpoint>& boot_eps)
{
    std::unique_lock<std::shared_mutex> lock(_mtx);
    
    // Update metrics
    _requests_sent++;
    
    // Create sr_object to track request
    auto sr_obj = std::make_shared<sr_object>();
    
    // Start async request with new implementation
    sr_obj->_future_result = pImpl->send_binding_request(boot_eps);
    
    // Also send legacy binding request for compatibility
    if (!boot_eps.empty()) {
        binding_request br(&_io_ctx, &_sender, &_ice_sender, sr_obj, _logger);
        
        // Send to each endpoint
        for (auto& ep : boot_eps) {
            br.send(ep);
        }
        
        // Register observer
        _obs_storage.register_observer<binding_request>(
            std::make_shared<binding_request>(br)
        );
    }
    
    if (_logger) {
        _logger->log(ss_logger::log_level::INFO,
                    "(@stun_server)",
                    "Sent binding request to",
                    std::to_string(boot_eps.size()).c_str(),
                    "servers");
    }
    
    return sr_obj;
}

stun_server::sr_ptr stun_server::binding_request(
    std::vector<ip::udp::endpoint>& boot_eps,
    std::vector<ip::udp::endpoint>& retry_eps)
{
    std::unique_lock<std::shared_mutex> lock(_mtx);
    
    // Update metrics  
    _requests_sent++;
    
    // Create sr_object to track request
    auto sr_obj = std::make_shared<sr_object>();
    
    // Combine all endpoints
    std::vector<ip::udp::endpoint> all_eps(boot_eps);
    all_eps.insert(all_eps.end(), retry_eps.begin(), retry_eps.end());
    
    // Start async request with new implementation
    sr_obj->_future_result = pImpl->send_binding_request(all_eps);
    
    // Also send legacy binding request for compatibility
    if (!boot_eps.empty()) {
        binding_request br(&_io_ctx, &_sender, &_ice_sender, sr_obj, _logger, &retry_eps);
        
        // Send to primary endpoints
        for (auto& ep : boot_eps) {
            br.send(ep);
        }
        
        // Register observer
        _obs_storage.register_observer<binding_request>(
            std::make_shared<binding_request>(br)
        );
    }
    
    if (_logger) {
        _logger->log(ss_logger::log_level::INFO,
                    "(@stun_server)",
                    "Sent binding request to",
                    std::to_string(all_eps.size()).c_str(),
                    "servers (including retries)");
    }
    
    return sr_obj;
}

} // namespace ice
} // namespace ss