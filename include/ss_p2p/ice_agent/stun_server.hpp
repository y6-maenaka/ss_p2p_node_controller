#ifndef DF2CC8B7_BABA_4168_BC62_6B54504BC58F
#define DF2CC8B7_BABA_4168_BC62_6B54504BC58F

#include <iostream>
#include <memory>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <thread>
#include <chrono>
#include <atomic>
#include <future>

// Forward declarations
namespace ss {
    class sender;
    class ss_logger;
    struct message;
    
    namespace core {
        template<typename T, typename E> class result;
        struct endpoint;
    }
    
    namespace kademlia {
        class direct_routing_table_controller;
    }
    
    namespace ice {
        class i_stun_client;
        struct stun_binding_request;
        struct stun_binding_response;
    }
}

// Legacy includes
#include <ss_p2p/ice_agent/ice_observer_storage.hpp>
#include <ss_p2p/message.hpp>
#include <ss_p2p/sender.hpp>
#include <ss_p2p/ss_logger.hpp>

#include "boost/asio.hpp"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;

namespace ss
{
namespace ice
{

class ice_sender;

/**
 * @brief STUN Reserved Object - manages binding request state
 * 
 * Legacy adapter for STUN binding requests. Internally uses
 * the new i_stun_client interface.
 */
struct sr_object
{
    using on_nat_traversal_success_handler = std::function<void(std::optional<ip::udp::endpoint>)>;
    
    sr_object();
    ~sr_object() = default;
    
    enum state_t
    {
        done = 0,    ///< Successfully retrieved external endpoint
        pending,     ///< Request in progress
        notfound     ///< Failed to retrieve endpoint
    };
    
    /**
     * @brief Synchronously get external endpoint (blocks)
     * @return External endpoint if successful
     * @warning Do not call from node_controller thread
     */
    std::optional<ip::udp::endpoint> sync_get();
    
    /**
     * @brief Asynchronously get external endpoint
     * @param handler Callback with result
     */
    void async_get(on_nat_traversal_success_handler handler);
    
    /**
     * @brief Create error object
     * @return Error sr_object
     */
    static sr_object _error_();
    
    /**
     * @brief Create pending object
     * @return Pending sr_object
     */
    static sr_object _pending_();
    
    /**
     * @brief Check if async mode
     * @return True if async
     */
    bool is_async() const { return _is_async; }
    
    /**
     * @brief Get current state
     * @return Current state
     */
    state_t get_state() const;
    
    /**
     * @brief Update state
     * @param s New state
     * @param ep Optional endpoint
     */
    void update_state(state_t s, std::optional<ip::udp::endpoint> ep = std::nullopt);
    
    // Async handler
    on_nat_traversal_success_handler handler;

private:
    mutable std::mutex _mtx;
    std::condition_variable _cv;
    ip::udp::endpoint _global_ep;
    std::atomic<state_t> _state{pending};
    bool _is_async = false;
    
    // Future for new architecture integration
    std::future<core::result<core::endpoint, std::string>> _future_result;
};

/**
 * @brief Legacy STUN server adapter
 * 
 * This class wraps the new ice::i_stun_client interface to provide
 * backward compatibility with the legacy API.
 * 
 * @deprecated Use the new ice::i_stun_client interface directly
 */
class stun_server
{
public:
    using sr_ptr = std::shared_ptr<sr_object>;
    
    /**
     * @brief Construct STUN server
     * @param io_ctx IO context
     * @param sender Legacy sender
     * @param ice_sender ICE sender
     * @param d_routing_table_controller DHT controller
     * @param obs_strage Observer storage
     * @param logger Logger instance
     */
    stun_server(
        asio::io_context &io_ctx,
        class sender &sender,
        class ice_sender &ice_sender,
        ss::kademlia::direct_routing_table_controller &d_routing_table_controller,
        ice_observer_strage &obs_strage,
        ss_logger* logger
    );
    
    ~stun_server();
    
    // Disable copy
    stun_server(const stun_server&) = delete;
    stun_server& operator=(const stun_server&) = delete;
    
    // Enable move
    stun_server(stun_server&&) noexcept;
    stun_server& operator=(stun_server&&) noexcept;
    
    /**
     * @brief Process incoming STUN message
     * @param msg Message
     * @param ep Source endpoint
     * @return 0 on success
     */
    int income_message(std::shared_ptr<message> msg, ip::udp::endpoint &ep);
    
    /**
     * @brief Send completion handler
     * @param ec Error code
     * @param bytes_transferred Bytes sent
     */
    void on_send_done(const boost::system::error_code &ec, std::size_t bytes_transferred);
    
    /**
     * @brief Send binding request
     * @param boot_eps Bootstrap endpoints
     * @return Reserved object for tracking request
     */
    sr_ptr binding_request(std::vector<ip::udp::endpoint>& boot_eps);
    
    /**
     * @brief Send binding request with retry endpoints
     * @param boot_eps Bootstrap endpoints
     * @param retry_eps Retry endpoints
     * @return Reserved object for tracking request
     */
    sr_ptr binding_request(
        std::vector<ip::udp::endpoint>& boot_eps,
        std::vector<ip::udp::endpoint>& retry_eps
    );

private:
    class impl;
    std::unique_ptr<impl> pImpl;
    
    // Legacy components
    asio::io_context& _io_ctx;
    sender& _sender;
    ice_sender& _ice_sender;
    ss::kademlia::direct_routing_table_controller& _d_routing_table_controller;
    ice_observer_strage& _obs_strage;
    ss_logger* _logger;
    
    // Thread safety
    mutable std::shared_mutex _mtx;
    
    // Metrics
    std::atomic<uint64_t> _requests_sent{0};
    std::atomic<uint64_t> _responses_received{0};
    std::atomic<uint64_t> _timeouts{0};
};

} // namespace ice
} // namespace ss

#endif