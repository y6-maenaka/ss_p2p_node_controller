#pragma once

#include <ss_p2p/core/types.hpp>
#include <ss_p2p/peer.hpp>

#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <boost/asio.hpp>

namespace ss {

/**
 * @brief Legacy multicast manager (deprecated - minimal implementation)
 * 
 * This class provides minimal backward compatibility for legacy code.
 * New code should use the application layer's message bus for broadcasting.
 */
class multicast_manager {
public:
    using endpoint = boost::asio::ip::udp::endpoint;
    using peers = std::vector<std::shared_ptr<peer>>;
    using endpoint_to_peer_func = std::function<std::shared_ptr<peer>(const endpoint&)>;

    /**
     * @brief Cast type enumeration (legacy compatibility)
     */
    enum cast_type {
        breath_first,  // Breadth-first peer selection
        depth_first    // Depth-first peer selection (not implemented)
    };

    /**
     * @brief Constructor (minimal implementation)
     * @param routing_table_ref Unused legacy parameter (kept for compatibility)
     * @param ep_to_peer_func Function to convert endpoint to peer
     */
    template<typename RoutingTableRef>
    multicast_manager(RoutingTableRef& routing_table_ref, const endpoint_to_peer_func& ep_to_peer_func)
        : _ep_to_peer_func(ep_to_peer_func) {
        // Legacy constructor - routing table parameter ignored in minimal implementation
        (void)routing_table_ref; // Suppress unused parameter warning
    }

    /**
     * @brief Get multicast targets (minimal implementation)
     * @param n Number of peers to select
     * @param type Cast type (ignored in minimal implementation)
     * @return Empty vector (multicast not implemented)
     */
    peers get_multicast_target(std::size_t n, cast_type type = cast_type::breath_first);

    /**
     * @brief Get random peer (minimal implementation)
     * @return nullptr (not implemented)
     */
    std::shared_ptr<peer> get_random();

    /**
     * @brief Update context with picked peer (no-op)
     * @param picked_peer Selected peer
     */
    void update_context(std::shared_ptr<peer> picked_peer);

    /**
     * @brief Update context with picked peers (no-op)
     * @param picked_peers Selected peers
     */
    void update_context(const peers& picked_peers);

    /**
     * @brief Clear context (no-op)
     */
    void clear_context();

    /**
     * @brief Check if multicast manager is enabled
     * @return false (always disabled in minimal implementation)
     */
    bool is_enabled() const noexcept { return false; }

private:
    endpoint_to_peer_func _ep_to_peer_func;
    mutable std::mutex _mutex;
    
    // Legacy context (kept for compatibility but unused)
    struct context {
        peers _picked_peers;
    } _context;
};

} // namespace ss 


