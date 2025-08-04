#ifndef D879B588_6D8D_42D0_8E22_4078BD445DA5
#define D879B588_6D8D_42D0_8E22_4078BD445DA5

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <future>
#include <functional>

#include "boost/asio.hpp"

#include <ss_p2p/peer.hpp>
#include <ss_p2p/message.hpp>
#include <ss_p2p/message_pool.hpp>
#include <ss_p2p/socket_manager.hpp>
#include <ss_p2p/multicast_manager.hpp>

using namespace boost::asio;

namespace ss {

// Forward declarations for backward compatibility
class udp_server;

namespace kademlia {
    class dht_manager;
    class direct_routing_table_controller;
    class k_routing_table;
}

namespace ice {
    class ice_agent;
}

namespace application {
    class i_p2p_node;
}

namespace core {
    class endpoint;
}

constexpr std::time_t DEFAULT_NODE_CONTROLLER_TICK_TIME_S = 30; // Periodic processing interval in seconds
constexpr std::time_t DEFAULT_GLOBAL_ADDRESS_REFRESH_TICK_TIME_s = 60; // Global address refresh interval in seconds

/**
 * @brief Legacy P2P Node Controller
 * 
 * This class provides backward compatibility with the legacy node_controller API
 * while internally delegating to the new modular architecture (i_p2p_node).
 * 
 * This wrapper ensures existing code continues to work while leveraging the
 * improved modular design under the hood.
 */
class node_controller {
public:
    // Legacy application ID for backward compatibility
    const message::app_id _id = {'a','b','c','d','e','f','g','h'};
    
    /**
     * @brief Constructor with endpoint and IO context
     * @param self_ep Local endpoint for the node
     * @param io_ctx Boost.Asio IO context (shared pointer)
     */
    explicit node_controller(
        ip::udp::endpoint& self_ep,
        std::shared_ptr<io_context> io_ctx = std::make_shared<boost::asio::io_context>()
    );
    
    /**
     * @brief Destructor - ensures proper cleanup
     */
    ~node_controller();
    
    // Disable copy construction and assignment
    node_controller(const node_controller&) = delete;
    node_controller& operator=(const node_controller&) = delete;
    
    // Enable move construction and assignment
    node_controller(node_controller&&) noexcept;
    node_controller& operator=(node_controller&&) noexcept;
    
    // =======================================
    // Legacy API Methods (Public Interface)
    // =======================================
    
    /**
     * @brief Get reference to the underlying socket
     * @return Socket reference
     */
    const ip::udp::socket& self_sock();
    
    /**
     * @brief Initialize node with bootstrap endpoints
     * @param boot_eps Bootstrap node endpoints
     */
    void init(std::vector<ip::udp::endpoint> boot_eps);
    
    /**
     * @brief Start the P2P node
     * @param boot_eps Bootstrap node endpoints (optional)
     */
    void start(std::vector<ip::udp::endpoint> boot_eps = std::vector<ip::udp::endpoint>());
    
    /**
     * @brief Stop the P2P node gracefully
     */
    void stop();
    
    /**
     * @brief Create a peer object for communication
     * @param ep Peer endpoint
     * @return Peer object
     */
    peer get_peer(const ip::udp::endpoint& ep);
    
    /**
     * @brief Create a shared peer reference
     * @param ep Peer endpoint
     * @return Shared pointer to peer
     */
    peer::ref get_peer_ref(const ip::udp::endpoint ep);
    
    /**
     * @brief Get socket manager reference
     * @return Socket manager reference
     */
    udp_socket_manager& get_socket_manager();
    
    /**
     * @brief Get direct routing table controller
     * @return Routing table controller reference
     */
    kademlia::direct_routing_table_controller& get_direct_routing_table_controller();
    
    /**
     * @brief Get message hub for handling incoming messages
     * @return Message hub reference
     */
    message_pool::message_hub& get_message_hub();
    
    /**
     * @brief Get multicast manager for broadcasting
     * @return Multicast manager instance
     */
    multicast_manager get_multicast_manager();
    
    /**
     * @brief Update global self endpoint
     * @param ep New global endpoint
     */
    void update_global_self_endpoint(ip::udp::endpoint ep);
    
    /**
     * @brief Handle command input (legacy interface)
     * @param inputs Command input strings
     */
    void on_command_input(std::vector<std::string> inputs);
    
    /**
     * @brief Synchronously get global address using STUN
     * @param boot_eps Bootstrap endpoints for STUN servers
     * @return Global endpoint if successful
     */
    std::optional<ip::udp::endpoint> sync_get_global_address(
        std::vector<ip::udp::endpoint> boot_eps = std::vector<ip::udp::endpoint>()
    );
    
    // =======================================
    // Debug Interface (SS_DEBUG only)
    // =======================================
    
#ifdef SS_DEBUG
    /**
     * @brief Get routing table for debugging
     * @return Routing table reference
     */
    kademlia::k_routing_table& get_routing_table();
    
    /**
     * @brief Get ICE agent for debugging
     * @return ICE agent reference
     */
    ice::ice_agent& get_ice_agent();
    
    /**
     * @brief Get DHT manager for debugging
     * @return DHT manager reference
     */
    kademlia::dht_manager& get_dht_manager();
    
    /**
     * @brief Get message pool for debugging
     * @return Message pool reference
     */
    message_pool& get_message_pool();
#endif
    
    // =======================================
    // Additional API for compatibility
    // =======================================
    
    /**
     * @brief Check if node is running
     * @return true if running, false otherwise
     */
    bool is_running() const noexcept;
    
    /**
     * @brief Get local endpoint
     * @return Local endpoint
     */
    ip::udp::endpoint get_self_endpoint() const noexcept;
    
protected:
    /**
     * @brief Periodic maintenance tick
     */
    void tick();
    
    /**
     * @brief Schedule next tick
     * @param tick_time_s Tick interval in seconds
     */
    void call_tick(std::time_t tick_time_s = DEFAULT_GLOBAL_ADDRESS_REFRESH_TICK_TIME_s);
    
    /**
     * @brief Handle incoming packet (legacy callback)
     * @param raw_msg Raw message bytes
     * @param ep Sender endpoint
     */
    void on_receive_packet(std::vector<std::uint8_t> raw_msg, ip::udp::endpoint& ep);
    
    /**
     * @brief Enable/disable routing requirements
     * @param b Routing enabled flag
     */
    void requires_routing(bool b);
    
private:
    // =======================================
    // Implementation Details
    // =======================================
    
    // New modular architecture interface
    std::unique_ptr<application::i_p2p_node> p2p_node_;
    
    // Legacy compatibility layer
    mutable std::shared_mutex state_mutex_;
    std::shared_ptr<io_context> io_context_;
    ip::udp::endpoint local_endpoint_;
    ip::udp::endpoint global_endpoint_;
    
    // Threading and lifecycle
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    std::thread daemon_thread_;
    
    // Legacy timer for compatibility
    std::unique_ptr<boost::asio::deadline_timer> tick_timer_;
    
    // Legacy compatibility objects (lazy-initialized)
    mutable std::unique_ptr<udp_socket_manager> socket_manager_;
    mutable std::unique_ptr<message_pool> message_pool_;
    mutable std::unique_ptr<message_pool::message_hub> message_hub_;
    
    // Error state
    mutable std::string last_error_;
    
    // =======================================
    // Helper Methods
    // =======================================
    
    /**
     * @brief Initialize P2P node with configuration
     */
    void initialize_p2p_node();
    
    /**
     * @brief Convert legacy endpoint vector to new format
     */
    std::vector<core::endpoint> convert_endpoints(
        const std::vector<ip::udp::endpoint>& legacy_endpoints
    ) const;
    
    /**
     * @brief Setup legacy compatibility objects
     */
    void setup_legacy_objects();
    
    /**
     * @brief Cleanup resources
     */
    void cleanup();
    
    /**
     * @brief Set error state
     */
    void set_error(const std::string& error) const;
};

} // namespace ss

#endif