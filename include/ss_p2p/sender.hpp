#pragma once

#include <ss_p2p/network/i_transport.hpp>
#include <ss_p2p/message.hpp>
#include <ss_p2p/core/result.hpp>
#include <ss_p2p/core/types.hpp>
#include <json.hpp>

#include <memory>
#include <functional>
#include <mutex>
#include <future>
#include <chrono>
#include <string>
#include <boost/asio.hpp>

namespace ss {

/**
 * @brief Legacy sender adapter for network::i_transport
 * 
 * Provides backward compatibility with the original sender API while
 * internally using the new transport layer. Thread-safe for concurrent operations.
 */
class sender {
public:
    using endpoint = boost::asio::ip::udp::endpoint;
    using json = nlohmann::json;
    using error_code = boost::system::error_code;
    
    /**
     * @brief Legacy async send handler type
     */
    template<typename SuccessHandler>
    using legacy_handler = SuccessHandler;
    
    /**
     * @brief Send function type for compatibility
     */
    using send_func = std::function<void(endpoint& dest_ep, std::string param, json payload)>;

    /**
     * @brief Constructor with transport and app ID
     * @param transport Network transport instance
     * @param app_id Application identifier for messages
     */
    explicit sender(std::shared_ptr<network::i_transport> transport, message::app_id app_id);
    
    /**
     * @brief Destructor
     */
    ~sender() = default;

    /**
     * @brief Async send with parameter and JSON payload (legacy API)
     * @param dest_ep Destination endpoint
     * @param param Message parameter
     * @param payload JSON payload
     * @param handler Completion handler
     */
    template<typename SuccessHandler>
    void async_send(endpoint dest_ep, std::string param, const json& payload, SuccessHandler handler) {
        try {
            // Create legacy message
            message msg(_app_id);
            msg.set_param(param, payload);
            
            // Encode message
            auto encoded_msg = message::encode(msg);
            
            // Convert to new transport format
            network::outgoing_message transport_msg(
                std::vector<std::uint8_t>(encoded_msg.begin(), encoded_msg.end()),
                convert_endpoint(dest_ep)
            );
            
            // Send via transport with adapter callback
            _transport->send_async(std::move(transport_msg), 
                [handler = std::move(handler)](core::result<void, network::transport_error> result) {
                    if (result.is_success()) {
                        // Success case - call with no error
                        error_code ec;
                        handler(ec, 0); // Assume handler expects (error_code, bytes_transferred)
                    } else {
                        // Error case - convert transport error to boost error
                        error_code ec = convert_transport_error(result.error());
                        handler(ec, 0);
                    }
                });
                
        } catch (const std::exception& e) {
            // Handle synchronous errors
            error_code ec = boost::asio::error::invalid_argument;
            handler(ec, 0);
        }
    }
    
    /**
     * @brief Async send with message object (legacy API)
     * @param dest_ep Destination endpoint
     * @param msg Message object
     * @param handler Completion handler
     */
    template<typename SuccessHandler>
    void async_send(endpoint dest_ep, message& msg, SuccessHandler handler) {
        try {
            // Encode message
            auto encoded_msg = message::encode(msg);
            
            // Convert to new transport format
            network::outgoing_message transport_msg(
                std::vector<std::uint8_t>(encoded_msg.begin(), encoded_msg.end()),
                convert_endpoint(dest_ep)
            );
            
            // Send via transport with adapter callback
            _transport->send_async(std::move(transport_msg),
                [handler = std::move(handler)](core::result<void, network::transport_error> result) {
                    if (result.is_success()) {
                        error_code ec;
                        handler(ec, 0);
                    } else {
                        error_code ec = convert_transport_error(result.error());
                        handler(ec, 0);
                    }
                });
                
        } catch (const std::exception& e) {
            error_code ec = boost::asio::error::invalid_argument;
            handler(ec, 0);
        }
    }

    /**
     * @brief Synchronous send with message object
     * @param dest_ep Destination endpoint
     * @param msg Message object
     * @return true on success, false on failure
     */
    bool sync_send(endpoint dest_ep, message& msg);
    
    /**
     * @brief Synchronous send with parameter and JSON payload
     * @param dest_ep Destination endpoint
     * @param param Message parameter
     * @param payload JSON payload
     * @return true on success, false on failure
     */
    bool sync_send(endpoint dest_ep, std::string param, json& payload);
    
    /**
     * @brief Get underlying transport (for advanced usage)
     * @return Shared pointer to transport
     */
    std::shared_ptr<network::i_transport> get_transport() const noexcept {
        return _transport;
    }
    
    /**
     * @brief Check if sender is ready for operations
     * @return true if transport is connected and ready
     */
    bool is_ready() const noexcept;
    
private:
    /**
     * @brief Convert boost endpoint to core endpoint
     * @param ep Boost UDP endpoint
     * @return Core endpoint
     */
    core::endpoint convert_endpoint(const endpoint& ep) const;
    
    /**
     * @brief Convert transport error to boost error code
     * @param error Transport error
     * @return Boost error code
     */
    static error_code convert_transport_error(network::transport_error error);
    
    /**
     * @brief Legacy completion callback (for internal use)
     * @param ec Error code from operation
     */
    void on_send_done(const error_code& ec);

    /// Network transport instance
    std::shared_ptr<network::i_transport> _transport;
    /// Application ID for messages
    message::app_id _app_id;
    /// Mutex for thread safety
    mutable std::mutex _mutex;
};

} // namespace ss
