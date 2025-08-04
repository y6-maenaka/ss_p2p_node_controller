#ifndef BD51C051_D379_4330_8C9D_0B9E15DFEFC9
#define BD51C051_D379_4330_8C9D_0B9E15DFEFC9

#include <functional>
#include <memory>
#include <string>
#include <array>
#include <span>
#include <mutex>
#include <atomic>
#include <shared_mutex>

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
        class i_nat_traversal;
        struct ice_candidate;
        struct nat_detection_result;
    }
}

// Legacy includes
#include <ss_p2p/message.hpp>
#include <ss_p2p/observer.hpp>
#include <ss_p2p/sender.hpp>
#include <ss_p2p/socket_manager.hpp>
#include <ss_p2p/kademlia/k_routing_table.hpp>
#include <ss_p2p/kademlia/direct_routing_table_controller.hpp>
#include <ss_p2p/ss_logger.hpp>
#include <ss_p2p/ice_agent/ice_observer_storage.hpp>
#include <json.hpp>

#include "./ice_message.hpp"

#include "boost/asio.hpp"


using namespace boost::asio;
using namespace ss::kademlia;
using json = nlohmann::json;


namespace ss
{
namespace ice
{


class ice_sender;


/**
 * @brief Legacy signaling server adapter
 * 
 * This class wraps the new ice::i_nat_traversal interface to provide
 * backward compatibility with the legacy signaling API. Internally uses
 * the new NAT traversal and hole punching mechanisms.
 * 
 * @deprecated Use the new ice::i_nat_traversal interface directly
 */
class signaling_server
{
public:
    using s_send_func = std::function<void(ip::udp::endpoint &dest_ep, std::string, const json payload)>;
    
    /**
     * @brief Construct signaling server
     * @param io_ctx IO context
     * @param sender Legacy sender
     * @param ice_sender ICE sender
     * @param glob_self_ep Global self endpoint
     * @param d_routing_table_controller DHT controller
     * @param obs_strage Observer storage
     * @param logger Logger instance
     */
    signaling_server(
        io_context &io_ctx,
        sender &sender,
        ice_sender& ice_sender,
        ip::udp::endpoint &glob_self_ep,
        direct_routing_table_controller &d_routing_table_controller,
        ice_observer_strage &obs_strage,
        ss_logger *logger
    );
    
    ~signaling_server();
    
    // Disable copy
    signaling_server(const signaling_server&) = delete;
    signaling_server& operator=(const signaling_server&) = delete;
    
    // Enable move
    signaling_server(signaling_server&&) noexcept;
    signaling_server& operator=(signaling_server&&) noexcept;
    
    /**
     * @brief Process incoming signaling message
     * @param msg Message
     * @param ep Source endpoint
     * @return 0 on success
     */
    int income_message(std::shared_ptr<ss::message> msg, ip::udp::endpoint &ep);
    
    /**
     * @brief Get signaling send function
     * @return Send function for signaling
     */
    s_send_func get_signaling_send_func();

private:
    void signaling_send(ip::udp::endpoint &dest_ep, std::string root_param, json payload);
    void on_send_done(const boost::system::error_code &ec);
    ice_message format_relay_msg(ice_message &base_msg);
    
    // Implementation class
    class impl;
    std::unique_ptr<impl> pImpl;
    
    // Legacy members
    io_context &_io_ctx;
    direct_routing_table_controller &_d_routing_table_controller;
    sender &_sender;
    ice_sender &_ice_sender;
    ice_observer_strage &_obs_strage;
    ip::udp::endpoint &_glob_self_ep;
    ss_logger *_logger;
    
    // Thread safety
    mutable std::shared_mutex _mtx;
    
    // Metrics
    std::atomic<uint64_t> _messages_relayed{0};
    std::atomic<uint64_t> _messages_delivered{0};
    std::atomic<uint64_t> _relay_failures{0};
};


}; // namespace ice
}; // namespace ss


#endif 


