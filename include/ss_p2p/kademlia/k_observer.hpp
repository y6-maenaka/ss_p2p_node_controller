#pragma once

#include "../observer.hpp"
#include "../message.hpp"
#include "../core/types.hpp"
#include "./k_node.hpp"

#include <memory>
#include <functional>
#include <chrono>
#include <string>

#include "boost/asio.hpp"

namespace ss::kademlia {

/**
 * @brief Legacy observer constants
 */
constexpr unsigned int DEFAULT_PING_RESPONSE_TIMEOUT_s = 5;

class rpc_manager;

/**
 * @brief Legacy k_observer adapter - wraps modern observer system
 * 
 * This adapter provides backward compatibility for the legacy Kademlia observer
 * by wrapping the new ss::base_observer implementation. All operations are 
 * delegated to the underlying modern implementation.
 * 
 * RAII Principles:
 * - Automatic resource management through base class
 * - Exception-safe operations
 * - Proper timer and callback management
 */
class k_observer : public ss::base_observer {
public:
    /**
     * @brief Constructor
     * @param io_ctx IO context for async operations
     * @param t_name Observer type name
     */
    k_observer(boost::asio::io_context& io_ctx, const std::string& t_name);

    /**
     * @brief Virtual destructor
     */
    virtual ~k_observer() = default;

protected:
    /**
     * @brief Protected copy constructor (deleted)
     */
    k_observer(const k_observer&) = delete;

    /**
     * @brief Protected move constructor (deleted)
     */
    k_observer(k_observer&&) = delete;

    /**
     * @brief Protected copy assignment (deleted)
     */
    k_observer& operator=(const k_observer&) = delete;

    /**
     * @brief Protected move assignment (deleted)
     */
    k_observer& operator=(k_observer&&) = delete;
};

/**
 * @brief Legacy ping observer adapter
 * 
 * Handles PING/PONG operations with timeout management and callbacks.
 * Provides backward compatibility with the legacy ping observer API.
 */
class ping : public k_observer {
public:
    using on_pong_handler = std::function<void(boost::asio::ip::udp::endpoint)>;
    using on_timeout_handler = std::function<void(boost::asio::ip::udp::endpoint)>;

    /**
     * @brief Constructor
     * @param io_ctx IO context for async operations
     * @param ep Target endpoint
     * @param pong_handler Callback for successful pong
     * @param timeout_handler Callback for timeout
     */
    ping(boost::asio::io_context& io_ctx, 
         boost::asio::ip::udp::endpoint ep,
         on_pong_handler pong_handler, 
         on_timeout_handler timeout_handler);

    /**
     * @brief Handle incoming message
     * @param msg Incoming message
     * @param ep Sender endpoint
     * @return Processing result
     */
    int income_message(message& msg, boost::asio::ip::udp::endpoint& ep);

    /**
     * @brief Handle timeout
     * @param ec Error code from timer
     */
    void timeout(const boost::system::error_code& ec);

    /**
     * @brief Initialize observer
     */
    void init();

    /**
     * @brief Print observer state (debug)
     */
    void print() const;

private:
    /// Whether pong has arrived
    bool is_pong_arrived_ : 1;
    
    /// Timeout timer
    std::unique_ptr<boost::asio::steady_timer> timer_;
    
    /// Target endpoint
    boost::asio::ip::udp::endpoint dest_ep_;
    
    /// Pong success callback
    on_pong_handler pong_handler_;
    
    /// Timeout callback
    on_timeout_handler timeout_handler_;
    
    /// IO context reference
    boost::asio::io_context& io_ctx_;
};

/**
 * @brief Legacy find_node observer adapter
 * 
 * Handles FIND_NODE operations with response processing and callbacks.
 * Provides backward compatibility with the legacy find_node observer API.
 */
class find_node : public k_observer {
public:
    using on_response_handler = std::function<void(boost::asio::ip::udp::endpoint)>;

    /**
     * @brief Constructor
     * @param io_ctx IO context for async operations
     * @param response_handler Callback for each found node
     */
    find_node(boost::asio::io_context& io_ctx, on_response_handler response_handler);

    /**
     * @brief Initialize observer
     */
    void init();

    /**
     * @brief Handle incoming message
     * @param msg Incoming message
     * @param ep Sender endpoint
     * @return Processing result
     */
    int income_message(message& msg, boost::asio::ip::udp::endpoint& ep);

    /**
     * @brief Print observer state (debug)
     */
    void print() const;

private:
    /// Response callback
    on_response_handler response_handler_;
};

/**
 * @brief Type alias for observer pointer (legacy compatibility)
 */
using base_observer_ptr = std::shared_ptr<base_observer>;

/**
 * @brief Generate unique handler ID (legacy compatibility)
 * @return Unique identifier string
 */
std::string generate_h_id();

} // namespace ss::kademlia