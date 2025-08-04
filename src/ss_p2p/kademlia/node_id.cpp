#include <ss_p2p/kademlia/node_id.hpp>
#include <utils.hpp>
#include <crypto_utils/crypto_utils.hpp>

#include <stdexcept>
#include <cstring>
#include <random>

namespace ss::kademlia {

// Default constructor
node_id::node_id() noexcept 
    : core_id_(std::make_unique<ss::core::node_id>())
    , cache_valid_(false) {
}

// Constructor from raw data
node_id::node_id(const void* from) 
    : core_id_(std::make_unique<ss::core::node_id>())
    , cache_valid_(false) {
    
    if (!from) {
        throw std::invalid_argument("node_id: null pointer provided");
    }
    
    // Copy data into core node_id
    auto& core_data = core_id_->data();
    std::memcpy(core_data.data(), from, ss::core::node_id::SIZE);
}

// Copy constructor
node_id::node_id(const node_id& nid) noexcept 
    : core_id_(std::make_unique<ss::core::node_id>(*nid.core_id_))
    , cache_valid_(false) {
}

// Move constructor
node_id::node_id(node_id&& nid) noexcept 
    : core_id_(std::move(nid.core_id_))
    , cached_data_(std::move(nid.cached_data_))
    , cache_valid_(nid.cache_valid_) {
    
    nid.cache_valid_ = false;
}

// Constructor from core::node_id
node_id::node_id(const ss::core::node_id& core_id) noexcept 
    : core_id_(std::make_unique<ss::core::node_id>(core_id))
    , cache_valid_(false) {
}

// Constructor from core::node_id (move)
node_id::node_id(ss::core::node_id&& core_id) noexcept 
    : core_id_(std::make_unique<ss::core::node_id>(std::move(core_id)))
    , cache_valid_(false) {
}

// Copy assignment
node_id& node_id::operator=(const node_id& nid) noexcept {
    if (this != &nid) {
        *core_id_ = *nid.core_id_;
        cache_valid_ = false;
    }
    return *this;
}

// Move assignment
node_id& node_id::operator=(node_id&& nid) noexcept {
    if (this != &nid) {
        core_id_ = std::move(nid.core_id_);
        cached_data_ = std::move(nid.cached_data_);
        cache_valid_ = nid.cache_valid_;
        nid.cache_valid_ = false;
    }
    return *this;
}

// Convert to string representation
std::string node_id::to_str() const {
    return core_id_->to_hex();
}

// Equality comparison
bool node_id::operator==(const node_id& nid) const noexcept {
    return *core_id_ == *nid.core_id_;
}

// Inequality comparison
bool node_id::operator!=(const node_id& nid) const noexcept {
    return !(*this == nid);
}

// Less-than comparison
bool node_id::operator<(const node_id& nid) const noexcept {
    return *core_id_ < *nid.core_id_;
}

// Get raw ID data (legacy compatibility)
node_id::id node_id::operator()() const noexcept {
    update_cache();
    return cached_data_;
}

// Array subscript operator
unsigned char node_id::operator[](unsigned short idx) const {
    if (idx >= K_NODE_ID_LENGTH) {
        throw std::out_of_range("node_id: index out of range");
    }
    update_cache();
    return cached_data_[idx];
}

// Create zero node_id
node_id node_id::none() noexcept {
    return node_id{};
}

// Generate random node_id
node_id node_id::random() {
    auto core_random = ss::core::node_id::random();
    return node_id{std::move(core_random)};
}

// Print node_id to stdout
void node_id::print() const {
    update_cache();
    for (std::size_t i = 0; i < cached_data_.size(); ++i) {
        printf("%02X", cached_data_[i]);
    }
}

// Get underlying core::node_id
const ss::core::node_id& node_id::core() const noexcept {
    return *core_id_;
}

// Get mutable underlying core::node_id
ss::core::node_id& node_id::core() noexcept {
    cache_valid_ = false; // Invalidate cache when core is modified
    return *core_id_;
}

// Direct access to raw data
const node_id::id& node_id::data() const noexcept {
    update_cache();
    return cached_data_;
}

// Update cached data from core_id
void node_id::update_cache() const {
    if (!cache_valid_) {
        const auto& core_data = core_id_->data();
        std::copy(core_data.begin(), core_data.end(), cached_data_.begin());
        cache_valid_ = true;
    }
}

// Calculate node_id from endpoint (legacy compatibility)
node_id calc_node_id(boost::asio::ip::udp::endpoint& ep) {
    auto ep_bin = endpoint_to_binary(ep);
    
    // Use SHA1 hash of endpoint binary data
    auto ep_md = cu::sha1::hash(ep_bin.first.get(), ep_bin.second);
    auto hash_array = ep_md.template to_array<std::uint8_t, node_id::K_NODE_ID_LENGTH>();
    
    return node_id{hash_array.data()};
}

// Calculate XOR distance between two node_ids (legacy compatibility)
unsigned short calc_node_xor_distance(const node_id& nid_1, const node_id& nid_2) {
    auto distance = nid_1.core().distance(nid_2.core());
    
    // Count leading zero bits in distance
    const auto& distance_data = distance.data();
    unsigned short prefix_zero_count = 0;
    
    for (std::size_t byte_idx = 0; byte_idx < distance_data.size(); ++byte_idx) {
        std::uint8_t byte_val = distance_data[byte_idx];
        
        for (int bit_idx = 7; bit_idx >= 0; --bit_idx) {
            if ((byte_val >> bit_idx) & 1) {
                return prefix_zero_count;
            }
            ++prefix_zero_count;
        }
    }
    
    return prefix_zero_count;
}

// Convert string to node_id (legacy compatibility)  
node_id str_to_node_id(const std::string& from) {
    try {
        auto core_id = ss::core::node_id::from_hex(from);
        return node_id{std::move(core_id)};
    } catch (const std::exception&) {
        // Return zero node_id on parse failure (legacy behavior)
        return node_id::none();
    }
}

} // namespace ss::kademlia