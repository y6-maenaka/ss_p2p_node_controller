#include <ss_p2p/sender.hpp>
#include <ss_p2p/network/transport_factory.hpp>
#include <iostream>
#include <future>

namespace ss {

sender::sender(std::shared_ptr<network::i_transport> transport, message::app_id app_id)
    : _transport(std::move(transport))
    , _app_id(app_id) {
    if (!_transport) {
        throw std::invalid_argument("Transport cannot be null");
    }
}

bool sender::sync_send(endpoint dest_ep, message& msg) {
    std::lock_guard<std::mutex> lock(_mutex);
    
    try {
        // Encode message
        auto encoded_msg = message::encode(msg);
        
        // Convert to new transport format
        network::outgoing_message transport_msg(
            std::vector<std::uint8_t>(encoded_msg.begin(), encoded_msg.end()),
            convert_endpoint(dest_ep)
        );
        
        // Use promise/future for synchronous operation
        std::promise<bool> result_promise;
        auto result_future = result_promise.get_future();
        
        _transport->send_async(std::move(transport_msg),
            [&result_promise](core::result<void, network::transport_error> result) {
                result_promise.set_value(result.is_success());
            });
        
        // Wait for completion with timeout
        auto status = result_future.wait_for(std::chrono::seconds(5));
        if (status == std::future_status::timeout) {
            return false;
        }
        
        return result_future.get();
        
    } catch (const std::exception& e) {
        return false;
    }
}

bool sender::sync_send(endpoint dest_ep, std::string param, json& payload) {
    // Create legacy message
    message msg(_app_id);
    msg.set_param(param, payload);
    
    return sync_send(dest_ep, msg);
}

bool sender::is_ready() const noexcept {
    std::lock_guard<std::mutex> lock(_mutex);
    return _transport && _transport->is_connected();
}

core::endpoint sender::convert_endpoint(const endpoint& ep) const {
    return core::endpoint(ep);
}

sender::error_code sender::convert_transport_error(network::transport_error error) {
    using boost::asio::error;
    
    switch (error) {
        case network::transport_error::none:
            return error_code{};
        case network::transport_error::network_failure:
            return error::network_down;
        case network::transport_error::connection_refused:
            return error::connection_refused;
        case network::transport_error::timeout:
            return error::timed_out;
        case network::transport_error::host_unreachable:
            return error::host_unreachable;
        case network::transport_error::port_unreachable:
            return error::make_error_code(boost::asio::error::connection_refused);
        case network::transport_error::connection_reset:
            return error::connection_reset;
        case network::transport_error::connection_aborted:
            return error::connection_aborted;
        case network::transport_error::no_route_to_host:
            return error::no_route_to_host;
        case network::transport_error::address_in_use:
            return error::address_in_use;
        case network::transport_error::address_not_available:
            return error::address_not_available;
        case network::transport_error::message_too_large:
            return error::message_size;
        case network::transport_error::not_connected:
            return error::not_connected;
        case network::transport_error::already_connected:
            return error::already_connected;
        case network::transport_error::invalid_argument:
            return error::invalid_argument;
        case network::transport_error::permission_denied:
            return error::access_denied;
        case network::transport_error::out_of_memory:
            return error::no_memory;
        case network::transport_error::shutdown_in_progress:
            return error::shut_down;
        default:
            return error::operation_aborted;
    }
}

void sender::on_send_done(const error_code& ec) {
    #ifdef SS_VERBOSE
    if (!ec) {
        // Success - could add debug logging here
    } else {
        std::cout << "(sender) send failure: " << ec.message() << std::endl;
    }
    #endif
}

} // namespace ss
