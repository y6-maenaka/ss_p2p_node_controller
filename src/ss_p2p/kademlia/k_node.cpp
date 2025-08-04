#include <ss_p2p/kademlia/k_node.hpp>
#include <utils.hpp>

#include <stdexcept>
#include <sstream>

namespace ss::kademlia {

// Default constructor
k_node::k_node() 
    : peer_info_(std::make_unique<ss::dht::peer_info>(
          ss::core::node_id{}, 
          ss::core::endpoint{}))
    , legacy_id_valid_(false) {
}

// Copy constructor
k_node::k_node(const k_node& kn) 
    : peer_info_(std::make_unique<ss::dht::peer_info>(*kn.peer_info_))
    , legacy_id_valid_(false) {
}

// Move constructor
k_node::k_node(k_node&& kn) noexcept 
    : peer_info_(std::move(kn.peer_info_))
    , legacy_id_(std::move(kn.legacy_id_))
    , legacy_id_valid_(kn.legacy_id_valid_) {
    
    kn.legacy_id_valid_ = false;
}

// Constructor from endpoint
k_node::k_node(boost::asio::ip::udp::endpoint ep) 
    : legacy_id_valid_(false) {
    
    // Calculate node_id from endpoint (legacy behavior)
    auto legacy_id = calc_node_id(ep);
    
    // Convert boost endpoint to core endpoint
    ss::core::endpoint core_ep{ep.address().to_string(), ep.port()};
    
    peer_info_ = std::make_unique<ss::dht::peer_info>(
        legacy_id.core(), std::move(core_ep));
}

// Constructor from node_id and endpoint
k_node::k_node(const node_id& id, boost::asio::ip::udp::endpoint ep) 
    : legacy_id_valid_(false) {
    
    // Convert boost endpoint to core endpoint
    ss::core::endpoint core_ep{ep.address().to_string(), ep.port()};
    
    peer_info_ = std::make_unique<ss::dht::peer_info>(
        id.core(), std::move(core_ep));
}

// Constructor from peer_info
k_node::k_node(const ss::dht::peer_info& peer_info) 
    : peer_info_(std::make_unique<ss::dht::peer_info>(peer_info))
    , legacy_id_valid_(false) {
}

// Constructor from peer_info (move)
k_node::k_node(ss::dht::peer_info&& peer_info) 
    : peer_info_(std::make_unique<ss::dht::peer_info>(std::move(peer_info)))
    , legacy_id_valid_(false) {
}

// Copy assignment
k_node& k_node::operator=(const k_node& kn) {
    if (this != &kn) {
        *peer_info_ = *kn.peer_info_;
        legacy_id_valid_ = false;
    }
    return *this;
}

// Move assignment
k_node& k_node::operator=(k_node&& kn) noexcept {
    if (this != &kn) {
        peer_info_ = std::move(kn.peer_info_);
        legacy_id_ = std::move(kn.legacy_id_);
        legacy_id_valid_ = kn.legacy_id_valid_;
        kn.legacy_id_valid_ = false;
    }
    return *this;
}

// Equality comparison
bool k_node::operator==(const k_node& kn) const {
    return *peer_info_ == *kn.peer_info_;
}

// Inequality comparison
bool k_node::operator!=(const k_node& kn) const {
    return !(*this == kn);
}

// Get endpoint
boost::asio::ip::udp::endpoint k_node::get_endpoint() const {
    const auto& core_ep = peer_info_->endpoint;
    
    try {
        boost::asio::ip::address addr = boost::asio::ip::make_address(core_ep.address());
        return boost::asio::ip::udp::endpoint{addr, core_ep.port()};
    } catch (const std::exception&) {
        // Return default endpoint on conversion failure
        return boost::asio::ip::udp::endpoint{};
    }
}

// Get node ID reference
node_id& k_node::get_id() {
    update_legacy_id();
    return *legacy_id_;
}

// Get const node ID reference
const node_id& k_node::get_id() const {
    update_legacy_id();
    return *legacy_id_;
}

// Convert to string representation
std::string k_node::to_str() {
    std::ostringstream oss;
    oss << "k_node{id=" << peer_info_->id.to_hex().substr(0, 8) << "..., "
        << "ep=" << peer_info_->endpoint.to_string() << ", "
        << "failures=" << peer_info_->failure_count << "}";
    return oss.str();
}

// Get shared_ptr to this
k_node::ref k_node::to_ref() {
    return shared_from_this();
}

// Print node information
void k_node::print() const {
    printf("k_node{id=");
    auto hex = peer_info_->id.to_hex();
    printf("%s", hex.c_str());
    printf(", ep=%s}", peer_info_->endpoint.to_string().c_str());
}

// Create blank k_node
k_node k_node::blank() {
    return k_node{};
}

// Get underlying peer_info
const ss::dht::peer_info& k_node::peer_info() const noexcept {
    return *peer_info_;
}

// Get mutable underlying peer_info
ss::dht::peer_info& k_node::peer_info() noexcept {
    legacy_id_valid_ = false; // Invalidate cache when peer_info is modified
    return *peer_info_;
}

// Check if node is alive
bool k_node::is_alive(std::chrono::milliseconds max_age) const {
    return peer_info_->is_alive(max_age);
}

// Update liveness information
void k_node::update_liveness(std::chrono::microseconds rtt) {
    peer_info_->update_liveness(rtt);
}

// Record communication failure
void k_node::record_failure() {
    peer_info_->record_failure();
}

// Get failure count
std::uint32_t k_node::failure_count() const noexcept {
    return peer_info_->failure_count;
}

// Get round-trip time
std::chrono::microseconds k_node::rtt() const noexcept {
    return peer_info_->rtt;
}

// Update legacy node_id from peer_info
void k_node::update_legacy_id() const {
    if (!legacy_id_valid_) {
        legacy_id_ = std::make_unique<node_id>(peer_info_->id);
        legacy_id_valid_ = true;
    }
}

// Convert string to k_node (legacy compatibility)
k_node str_to_k_node(const std::string& from) {
    // Parse format: "node_id@ip:port" or just "ip:port"
    auto at_pos = from.find('@');
    
    if (at_pos != std::string::npos) {
        // Format: "node_id@ip:port"
        auto id_str = from.substr(0, at_pos);
        auto endpoint_str = from.substr(at_pos + 1);
        
        auto legacy_id = str_to_node_id(id_str);
        
        // Parse endpoint
        auto colon_pos = endpoint_str.find(':');
        if (colon_pos != std::string::npos) {
            auto ip = endpoint_str.substr(0, colon_pos);
            auto port_str = endpoint_str.substr(colon_pos + 1);
            
            try {
                auto port = static_cast<std::uint16_t>(std::stoul(port_str));
                boost::asio::ip::address addr = boost::asio::ip::make_address(ip);
                boost::asio::ip::udp::endpoint ep{addr, port};
                
                return k_node{legacy_id, ep};
            } catch (const std::exception&) {
                // Fall through to blank node
            }
        }
    } else {
        // Format: "ip:port" - generate node_id from endpoint
        auto colon_pos = from.find(':');
        if (colon_pos != std::string::npos) {
            auto ip = from.substr(0, colon_pos);
            auto port_str = from.substr(colon_pos + 1);
            
            try {
                auto port = static_cast<std::uint16_t>(std::stoul(port_str));
                boost::asio::ip::address addr = boost::asio::ip::make_address(ip);
                boost::asio::ip::udp::endpoint ep{addr, port};
                
                return k_node{ep};
            } catch (const std::exception&) {
                // Fall through to blank node
            }
        }
    }
    
    // Return blank node on parse failure
    return k_node::blank();
}

} // namespace ss::kademlia