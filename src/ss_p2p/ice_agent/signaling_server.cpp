#include <ss_p2p/ice_agent/signaling_server.hpp>
#include <ss_p2p/ice_agent/ice_observer.hpp>
#include <ss_p2p/ice_agent/ice_agent.hpp>
#include <ss_p2p/core/result.hpp>
#include <ss_p2p/ice/i_nat_traversal.hpp>
#include <ss_p2p/ice/factory.hpp>
#include <ss_p2p/network/factory.hpp>
#include <ss_p2p/utils.hpp>
#include <chrono>
#include <thread>
#include <future>

namespace ss
{
namespace ice
{

// Implementation class that wraps the new NAT traversal interface
class signaling_server::impl 
{
public:
    impl(asio::io_context& io_ctx, const core::endpoint& local_ep, ss_logger* logger)
        : io_context(io_ctx)
        , local_endpoint(local_ep)
        , logger(logger)
    {
        // Create NAT traversal instance
        auto ice_factory = ss::ice::get_ice_factory();
        nat_traversal = ice_factory->create_nat_traversal();
        
        // Create transport for signaling
        auto transport_factory = ss::network::get_transport_factory();
        transport = transport_factory->create_udp_transport(io_context);
    }
    
    ~impl() = default;
    
    // Process signaling message using new architecture
    core::result<void, std::string> process_signaling(
        const ice_message& msg,
        const core::endpoint& from_ep)
    {
        try {
            auto signaling_controller = msg.get_signaling_message_controller();
            auto sub_protocol = signaling_controller.get_sub_protocol();
            
            switch (sub_protocol) {
                case ice_message::signaling::sub_protocol_t::request:
                    return handle_signaling_request(msg, from_ep);
                    
                case ice_message::signaling::sub_protocol_t::relay:
                    return handle_signaling_relay(msg, from_ep);
                    
                case ice_message::signaling::sub_protocol_t::response:
                    return handle_signaling_response(msg, from_ep);
                    
                default:
                    return core::result<void, std::string>::error(
                        "Unknown signaling sub-protocol"
                    );
            }
        }
        catch (const std::exception& e) {
            return core::result<void, std::string>::error(
                std::string("Signaling processing error: ") + e.what()
            );
        }
    }
    
    // Send signaling message with NAT traversal
    std::future<core::result<void, std::string>>
    send_signaling(const core::endpoint& dest_ep, const std::string& payload)
    {
        auto promise = std::make_shared<std::promise<core::result<void, std::string>>>();
        auto future = promise->get_future();
        
        // Check if we can connect directly
        auto cached_nat = nat_traversal->get_cached_nat_info(local_endpoint);
        if (cached_nat.has_value()) {
            // Try direct connection first
            auto peer_id = core::peer_id::from_endpoint(dest_ep);
            
            nat_traversal->establish_connection(
                local_endpoint,
                peer_id,
                {} // Use default signaling servers
            ).then([promise, payload, this](auto result) {
                if (result.is_success()) {
                    // Connection established, send payload
                    auto send_result = transport->send_to(
                        std::vector<uint8_t>(payload.begin(), payload.end()),
                        result.value().remote_endpoint
                    );
                    
                    if (send_result.is_success()) {
                        promise->set_value(core::result<void, std::string>::success());
                    } else {
                        promise->set_value(core::result<void, std::string>::error(
                            send_result.error()
                        ));
                    }
                } else {
                    promise->set_value(core::result<void, std::string>::error(
                        "Failed to establish connection: " + to_string(result.error())
                    ));
                }
            });
        } else {
            // No NAT info cached, need to detect first
            promise->set_value(core::result<void, std::string>::error(
                "NAT detection required before signaling"
            ));
        }
        
        return future;
    }
    
private:
    // Handle incoming signaling request
    core::result<void, std::string> handle_signaling_request(
        const ice_message& msg,
        const core::endpoint& from_ep)
    {
        // Extract destination from message
        auto signaling_controller = msg.get_signaling_message_controller();
        auto dest_ep = signaling_controller.get_dest_endpoint();
        
        // Check if destination is us
        core::endpoint dest{dest_ep.address().to_string(), dest_ep.port()};
        if (dest == local_endpoint) {
            // Send response back
            return send_signaling_response(msg, from_ep);
        }
        
        // Otherwise relay the message
        return relay_signaling(msg, dest, from_ep);
    }
    
    // Handle signaling relay
    core::result<void, std::string> handle_signaling_relay(
        const ice_message& msg,
        const core::endpoint& from_ep)
    {
        auto signaling_controller = msg.get_signaling_message_controller();
        auto dest_ep = signaling_controller.get_dest_endpoint();
        
        // Check TTL
        int ttl = signaling_controller.get_ttl();
        if (ttl <= 0) {
            return core::result<void, std::string>::error("TTL expired");
        }
        
        // Continue relaying
        core::endpoint dest{dest_ep.address().to_string(), dest_ep.port()};
        return relay_signaling(msg, dest, from_ep);
    }
    
    // Handle signaling response
    core::result<void, std::string> handle_signaling_response(
        const ice_message& msg,
        const core::endpoint& from_ep)
    {
        // Response handling is done by observers
        return core::result<void, std::string>::success();
    }
    
    // Send signaling response
    core::result<void, std::string> send_signaling_response(
        const ice_message& request,
        const core::endpoint& to_ep)
    {
        // Create response message
        ice_message response = request;
        auto controller = response.get_signaling_message_controller();
        controller.set_sub_protocol(ice_message::signaling::sub_protocol_t::response);
        
        // Encode and send
        auto encoded = response.encode();
        auto data = std::vector<uint8_t>(encoded.begin(), encoded.end());
        
        auto send_result = transport->send_to(data, to_ep);
        if (send_result.is_error()) {
            return core::result<void, std::string>::error(send_result.error());
        }
        
        return core::result<void, std::string>::success();
    }
    
    // Relay signaling message
    core::result<void, std::string> relay_signaling(
        const ice_message& msg,
        const core::endpoint& dest_ep,
        const core::endpoint& from_ep)
    {
        // Use NAT traversal to find best path
        auto send_future = send_signaling(dest_ep, msg.encode());
        
        // For now, return success (async operation in progress)
        return core::result<void, std::string>::success();
    }
    
public:
    asio::io_context& io_context;
    core::endpoint local_endpoint;
    ss_logger* logger;
    
    // New architecture components
    nat_traversal_ptr nat_traversal;
    network::transport_ptr transport;
    
    // Metrics
    std::atomic<uint64_t> requests_handled{0};
    std::atomic<uint64_t> relays_performed{0};
    std::atomic<uint64_t> responses_sent{0};
};

// Constructor
signaling_server::signaling_server(
    io_context &io_ctx,
    sender &sender,
    ice_sender &ice_sender,
    ip::udp::endpoint &glob_self_ep,
    direct_routing_table_controller &d_routing_table_controller,
    ice_observer_strage &obs_storage,
    ss_logger *logger)
    : _io_ctx(io_ctx)
    , _sender(sender)
    , _ice_sender(ice_sender)
    , _glob_self_ep(glob_self_ep)
    , _d_routing_table_controller(d_routing_table_controller)
    , _obs_storage(obs_storage)
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
                    "(@signaling_server)",
                    "Initialized with new architecture adapter");
    }
}

// Destructor
signaling_server::~signaling_server() = default;

// Move constructor
signaling_server::signaling_server(signaling_server&&) noexcept = default;

// Move assignment
signaling_server& signaling_server::operator=(signaling_server&&) noexcept = default;

int signaling_server::income_message(std::shared_ptr<message> msg, ip::udp::endpoint &ep)
{
    std::unique_lock<std::shared_mutex> lock(_mtx);
    
    // Extract ICE message
    auto ice_param = msg->get_param("ice_agent");
    if (!ice_param) {
        return -1; // Not an ICE message
    }
    
    ice_message ice_msg(ice_param.value());
    auto signaling_controller = ice_msg.get_signaling_message_controller();
    
    // Process with new implementation
    core::endpoint from_ep{ep.address().to_string(), ep.port()};
    auto result = pImpl->process_signaling(ice_msg, from_ep);
    
    if (result.is_error() && _logger) {
        _logger->log(ss_logger::log_level::WARNING,
                    "(@signaling_server)",
                    "Signaling processing error:",
                    result.error().c_str());
    }
    
    // Also process with legacy system for compatibility
    auto sub_protocol = signaling_controller.get_sub_protocol();
    
    if (sub_protocol == ice_message::signaling::sub_protocol_t::request) {
        // Check if destination is us
        ip::udp::endpoint dest_ep = signaling_controller.get_dest_endpoint();
        
        if (dest_ep == _ice_sender.get_self_endpoint()) {
            _messages_delivered++;
            
            // Send response
            ip::udp::endpoint src_ep = signaling_controller.get_src_endpoint();
            
            auto msg_controller = ice_msg.get_signaling_message_controller();
            msg_controller.set_sub_protocol(ice_message::signaling::sub_protocol_t::response);
            
            observer<class signaling_response> sgnl_response_obs(
                _io_ctx, _sender, _ice_sender, _glob_self_ep, _d_routing_table_controller
            );
            
            auto enc_msg = ice_msg.encode();
            _sender.async_send(src_ep, "ice_agent", enc_msg,
                std::bind(&signaling_response::init,
                    *(sgnl_response_obs.get()),
                    std::placeholders::_1)
            );
            
            _obs_storage.add_observer<class signaling_response>(sgnl_response_obs);
            return 0;
        }
        
        // Otherwise relay
        _messages_relayed++;
    }
    
    // Legacy relay handling
    if (sub_protocol == ice_message::signaling::sub_protocol_t::request ||
        sub_protocol == ice_message::signaling::sub_protocol_t::relay) {
        
        ip::udp::endpoint dest_ep = signaling_controller.get_dest_endpoint();
        
        // Create relay observer
        observer<class signaling_relay> sgnl_relay_obs(
            _io_ctx, _sender, _ice_sender, _glob_self_ep, _d_routing_table_controller
        );
        
        signaling_controller.add_relay_endpoint(_ice_sender.get_self_endpoint());
        signaling_controller.set_sub_protocol(ice_message::signaling::sub_protocol_t::relay);
        
        // Check routing table
        if (_d_routing_table_controller.is_exist(dest_ep)) {
            _ice_sender.async_ice_send(dest_ep, ice_msg,
                std::bind(&signaling_relay::init, *(sgnl_relay_obs.get()))
            );
            
            if (_logger) {
                _logger->log_packet(
                    logger::log_level::INFO,
                    ss_logger::packet_direction::OUTGOING,
                    dest_ep,
                    "(@signaling_server)",
                    "signaling relay fin"
                );
            }
            
            _obs_storage.add_observer<class signaling_relay>(sgnl_relay_obs);
            return 0;
        }
        
        // Check TTL
        int ttl = signaling_controller.get_ttl();
        if (ttl <= 0) {
            return 0; // Drop message
        }
        
        // Collect relay endpoints
        std::vector<ip::udp::endpoint> relay_eps = signaling_controller.get_relay_endpoints();
        auto forward_eps = _d_routing_table_controller.collect_endpoint(dest_ep, 3, relay_eps);
        
        for (auto& fwd_ep : forward_eps) {
            signaling_controller.add_relay_endpoint(fwd_ep);
        }
        
        // Forward to multiple nodes
        for (auto& fwd_ep : forward_eps) {
            _ice_sender.async_ice_send(fwd_ep, ice_msg,
                std::bind(&signaling_relay::init, *(sgnl_relay_obs.get()))
            );
            
            if (_logger) {
                _logger->log_packet(
                    logger::log_level::INFO,
                    ss_logger::packet_direction::OUTGOING,
                    fwd_ep,
                    "(@signaling_server)",
                    "signaling relay"
                );
            }
        }
        
        _obs_storage.add_observer<class signaling_relay>(sgnl_relay_obs);
    }
    
    return 0;
}

void signaling_server::on_send_done(const boost::system::error_code &ec)
{
    if (ec) {
        _relay_failures++;
        if (_logger) {
            _logger->log(ss_logger::log_level::WARNING,
                        "(@signaling_server)",
                        "Send error:",
                        ec.message().c_str());
        }
    }
}

ice_message signaling_server::format_relay_msg(ice_message &base_msg)
{
    auto msg_controller = base_msg.get_signaling_message_controller();
    msg_controller.add_relay_endpoint(_ice_sender.get_self_endpoint());
    return base_msg;
}

void signaling_server::signaling_send(ip::udp::endpoint &dest_ep, std::string root_param, json payload)
{
    std::unique_lock<std::shared_mutex> lock(_mtx);
    
    // Try new architecture first
    core::endpoint dest{dest_ep.address().to_string(), dest_ep.port()};
    auto future = pImpl->send_signaling(dest, payload.dump());
    
    // Also use legacy method for compatibility
    if (_d_routing_table_controller.is_exist(dest_ep)) {
        // Direct send
        _sender.async_send(dest_ep, root_param, payload,
            std::bind(&signaling_server::on_send_done, this, std::placeholders::_1)
        );
        return;
    }
    
    // Create signaling request observer
    observer<class signaling_request> sgnl_req_obs(
        _io_ctx, _sender, _ice_sender, _glob_self_ep, _d_routing_table_controller
    );
    sgnl_req_obs.init(dest_ep, root_param, payload);
    
    _obs_storage.add_observer<class signaling_request>(sgnl_req_obs);
}

signaling_server::s_send_func signaling_server::get_signaling_send_func()
{
    return std::bind(
        &signaling_server::signaling_send,
        this,
        std::placeholders::_1,
        std::placeholders::_2,
        std::placeholders::_3
    );
}

} // namespace ice
} // namespace ss