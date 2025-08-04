#ifndef A3401654_2A6C_4D83_ACCD_0FAAE564EA70
#define A3401654_2A6C_4D83_ACCD_0FAAE564EA70

#include <functional>
#include <string>
#include <memory>
#include <mutex>
#include <atomic>

// Forward declarations
namespace ss {
    class udp_socket_manager;
    class sender;
    class ss_logger;
    struct message;
    
    namespace kademlia {
        class direct_routing_table_controller;
    }
    
    namespace core {
        template<typename T, typename E> class result;
        struct endpoint;
        struct peer_id;
    }
    
    namespace ice {
        class i_nat_traversal;
        class i_stun_client;
        class ice_candidate;
    }
}

// Legacy includes for compatibility
#include <ss_p2p/ice_agent/ice_observer_storage.hpp>
#include <ss_p2p/ice_agent/ice_message.hpp>
#include <ss_p2p/ice_agent/ice_sender.hpp>
#include <ss_p2p/ice_agent/signaling_server.hpp>
#include <ss_p2p/ice_agent/stun_server.hpp>

#include "boost/asio.hpp"

namespace asio = boost::asio;
namespace ip = boost::asio::ip;

namespace ss
{
namespace ice
{

/**
 * @brief Legacy ICE agent adapter for backward compatibility
 * 
 * This class wraps the new ice::i_nat_traversal interface to provide
 * backward compatibility with the legacy API. Internal implementation
 * uses the new modular ICE architecture.
 * 
 * @deprecated Use the new ice::i_nat_traversal interface directly
 */
class ice_agent
{
public:
    using s_send_func = signaling_server::s_send_func;
    
    /**
     * @brief Construct legacy ICE agent
     * @param io_ctx IO context for async operations
     * @param sock_manager Socket manager (legacy)
     * @param glob_self_ep Global self endpoint
     * @param id Application ID
     * @param sender Message sender (legacy)
     * @param d_routing_table_controller DHT routing table controller
     * @param logger Logger instance
     */
    ice_agent(
        asio::io_context &io_ctx,
        udp_socket_manager &sock_manager,
        ip::udp::endpoint &glob_self_ep,
        message::app_id id,
        sender &sender,
        ss::kademlia::direct_routing_table_controller &d_routing_table_controller,
        ss_logger *logger
    );
    
    ~ice_agent();
    
    // Disable copy
    ice_agent(const ice_agent&) = delete;
    ice_agent& operator=(const ice_agent&) = delete;
    
    // Enable move
    ice_agent(ice_agent&&) noexcept;
    ice_agent& operator=(ice_agent&&) noexcept;
    
    /**
     * @brief Hello message (legacy, no-op)
     * @deprecated No longer needed
     */
    void hello() {}
    
    /**
     * @brief Process incoming ICE message
     * @param msg Incoming message
     * @param ep Source endpoint
     * @return 0 on success, error code otherwise
     */
    int income_message(std::shared_ptr<message> msg, ip::udp::endpoint &ep);
    
    /**
     * @brief Get signaling send function
     * @return Send function for signaling
     */
    signaling_server::s_send_func get_signaling_send_func();
    
    /**
     * @brief Update global self endpoint
     * @param ep New global endpoint
     */
    void update_global_self_endpoint(ip::udp::endpoint &ep);
    
    /**
     * @brief Get STUN server reference
     * @return STUN server reference
     */
    stun_server& get_stun_server();
    
    /**
     * @brief Get ICE sender reference
     * @return ICE sender reference
     */
    ice_sender& get_ice_sender();

private:
    #if SS_DEBUG
public:
    /**
     * @brief Get observer storage (debug only)
     * @return Observer storage reference
     */
    ice_observer_storage& get_observer_storage();
    #endif

private:
    class impl;
    std::unique_ptr<impl> pImpl;
    
    // Legacy components for compatibility
    udp_socket_manager& _sock_manager;
    ip::udp::endpoint _glob_self_ep;
    message::app_id _app_id;
    sender& _sender;
    ice_sender _ice_sender;
    ice_observer_storage _obs_storage;
    signaling_server _sgnl_server;
    stun_server _stun_server;
    ss_logger* _logger;
    
    // Thread safety
    mutable std::shared_mutex _mtx;
    
    // Metrics
    std::atomic<uint64_t> _messages_processed{0};
    std::atomic<uint64_t> _signaling_messages{0};
    std::atomic<uint64_t> _stun_messages{0};
};

} // namespace ice
} // namespace ss

#endif