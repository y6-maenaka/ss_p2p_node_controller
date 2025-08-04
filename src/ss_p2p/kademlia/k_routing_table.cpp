#include <ss_p2p/kademlia/k_routing_table.hpp>

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace ss::kademlia {

// Constructor
k_routing_table::k_routing_table(node_id self_id, ss_logger* logger)
    : _logger(logger)
    , self_id_(std::move(self_id)) {
    
    initialize_buckets();
}

// Constructor with routing interface
k_routing_table::k_routing_table(node_id self_id, 
                               std::shared_ptr<ss::dht::i_routing> routing,
                               ss_logger* logger)
    : _logger(logger)
    , self_id_(std::move(self_id))
    , routing_(std::move(routing)) {
    
    initialize_buckets();
}

// Copy constructor
k_routing_table::k_routing_table(const k_routing_table& other) {
    std::shared_lock lock(other.mutex_);
    _logger = other._logger;
    table_ = other.table_;
    self_id_ = other.self_id_;
    routing_ = other.routing_;
    
    // Re-initialize buckets with routing interface
    initialize_buckets();
}

// Move constructor
k_routing_table::k_routing_table(k_routing_table&& other) noexcept {
    std::unique_lock lock(other.mutex_);
    _logger = other._logger;
    table_ = std::move(other.table_);
    self_id_ = std::move(other.self_id_);
    routing_ = std::move(other.routing_);
    
    other._logger = nullptr;
}

// Copy assignment
k_routing_table& k_routing_table::operator=(const k_routing_table& other) {
    if (this != &other) {
        std::unique_lock lock1(mutex_, std::defer_lock);
        std::shared_lock lock2(other.mutex_, std::defer_lock);
        std::lock(lock1, lock2);
        
        _logger = other._logger;
        table_ = other.table_;
        self_id_ = other.self_id_;
        routing_ = other.routing_;
        
        initialize_buckets();
    }
    return *this;
}

// Move assignment
k_routing_table& k_routing_table::operator=(k_routing_table&& other) noexcept {
    if (this != &other) {
        std::unique_lock lock1(mutex_, std::defer_lock);
        std::unique_lock lock2(other.mutex_, std::defer_lock);
        std::lock(lock1, lock2);
        
        _logger = other._logger;
        table_ = std::move(other.table_);
        self_id_ = std::move(other.self_id_);
        routing_ = std::move(other.routing_);
        
        other._logger = nullptr;
    }
    return *this;
}

// Swap node in routing table
void k_routing_table::swap_node(k_node& node_src, k_node node_dest) {
    std::unique_lock lock(mutex_);
    
    auto branch_idx = calc_branch(node_src);
    if (branch_idx < K_BUCKET_COUNT) {
        table_[branch_idx].swap_node(node_src, std::move(node_dest));
    }
}

// Auto-update routing table with node
k_routing_table::update_state k_routing_table::auto_update(k_node kn) {
    std::unique_lock lock(mutex_);
    
    auto branch_idx = calc_branch(kn);
    if (branch_idx >= K_BUCKET_COUNT) {
        return update_state::error;
    }
    
    return table_[branch_idx].auto_update(std::move(kn));
}

// Check if node exists in routing table
bool k_routing_table::is_exist(k_node& kn) {
    std::shared_lock lock(mutex_);
    
    auto branch_idx = calc_branch(kn);
    if (branch_idx >= K_BUCKET_COUNT) {
        return false;
    }
    
    return table_[branch_idx].is_exist(kn);
}

// Collect nodes around root node
std::vector<k_node> k_routing_table::collect_node(k_node& root_node, 
                                                 std::size_t max_count, 
                                                 const std::vector<k_node>& ignore_nodes) {
    std::shared_lock lock(mutex_);
    
    std::vector<k_node> result;
    result.reserve(max_count);
    
    // Start from root node's bucket
    auto start_branch = calc_branch(root_node);
    
    // Collect from multiple buckets, starting from closest
    for (int offset = 0; offset < static_cast<int>(K_BUCKET_COUNT) && result.size() < max_count; ++offset) {
        // Try both directions from start branch
        std::vector<int> branches_to_try;
        
        if (offset == 0) {
            branches_to_try.push_back(start_branch);
        } else {
            if (start_branch + offset < K_BUCKET_COUNT) {
                branches_to_try.push_back(start_branch + offset);
            }
            if (start_branch >= offset) {
                branches_to_try.push_back(start_branch - offset);
            }
        }
        
        for (int branch : branches_to_try) {
            if (result.size() >= max_count) break;
            
            auto bucket_nodes = table_[branch].get_nodes();
            for (const auto& node : bucket_nodes) {
                if (result.size() >= max_count) break;
                
                // Check if node should be ignored
                bool should_ignore = std::any_of(ignore_nodes.begin(), ignore_nodes.end(),
                    [&node](const k_node& ignore) { return node == ignore; });
                
                if (!should_ignore) {
                    result.push_back(node);
                }
            }
        }
    }
    
    // Sort by distance to root node if we have routing interface
    if (routing_ && !result.empty()) {
        const auto& root_core_id = root_node.peer_info().id;
        
        std::sort(result.begin(), result.end(),
            [&root_core_id](const k_node& a, const k_node& b) {
                auto dist_a = root_core_id.distance(a.peer_info().id);
                auto dist_b = root_core_id.distance(b.peer_info().id);
                return dist_a < dist_b;
            });
    }
    
    return result;
}

// Get nodes from front of bucket containing root node
std::vector<k_node> k_routing_table::get_node_front(k_node& root_node, 
                                                   std::size_t count, 
                                                   const std::vector<k_node>& ignore_nodes) {
    std::shared_lock lock(mutex_);
    
    auto branch_idx = calc_branch(root_node);
    if (branch_idx >= K_BUCKET_COUNT) {
        return {};
    }
    
    return table_[branch_idx].get_node_front(count, ignore_nodes);
}

// Get nodes from back of bucket containing root node
std::vector<k_node> k_routing_table::get_node_back(k_node& root_node, 
                                                  std::size_t count, 
                                                  const std::vector<k_node>& ignore_nodes) {
    std::shared_lock lock(mutex_);
    
    auto branch_idx = calc_branch(root_node);
    if (branch_idx >= K_BUCKET_COUNT) {
        return {};
    }
    
    return table_[branch_idx].get_node_back(count, ignore_nodes);
}

// Get bucket containing node
k_bucket& k_routing_table::get_bucket(k_node& kn) {
    std::shared_lock lock(mutex_);
    
    auto branch_idx = calc_branch(kn);
    if (branch_idx >= K_BUCKET_COUNT) {
        throw std::out_of_range("Invalid branch index for node");
    }
    
    return table_[branch_idx];
}

// Get bucket by branch index
k_bucket& k_routing_table::get_bucket(unsigned short branch) {
    std::shared_lock lock(mutex_);
    
    if (branch >= K_BUCKET_COUNT) {
        throw std::out_of_range("Invalid branch index");
    }
    
    return table_[branch];
}

// Calculate branch index for node
unsigned short k_routing_table::calc_branch_index(k_node& kn) {
    return calc_branch(kn);
}

// Get total node count in routing table
std::size_t k_routing_table::get_node_count() {
    std::shared_lock lock(mutex_);
    
    std::size_t total = 0;
    for (const auto& bucket : table_) {
        total += bucket.get_node_count();
    }
    
    return total;
}

// Get bucket iterator pointing to first bucket
k_bucket_iterator k_routing_table::get_begin_bucket_iterator() {
    std::shared_lock lock(mutex_);
    return k_bucket_iterator(this, table_.begin(), 0);
}

// Print routing table contents
void k_routing_table::print(int start_branch) {
    std::shared_lock lock(mutex_);
    
    std::cout << "k_routing_table (self_id: " << self_id_.to_str().substr(0, 8) << "...):\n";
    
    for (int i = start_branch; i < K_BUCKET_COUNT; ++i) {
        if (table_[i].get_node_count() > 0) {
            table_[i].print_horizontal();
        }
    }
    
    std::cout << "Total nodes: " << get_node_count() << "\n";
}

// Set modern routing interface
void k_routing_table::set_routing(std::shared_ptr<ss::dht::i_routing> routing) {
    std::unique_lock lock(mutex_);
    routing_ = std::move(routing);
    initialize_buckets();
}

// Get underlying routing interface
std::shared_ptr<ss::dht::i_routing> k_routing_table::routing() const noexcept {
    std::shared_lock lock(mutex_);
    return routing_;
}

// Get local node ID
const node_id& k_routing_table::self_id() const noexcept {
    std::shared_lock lock(mutex_);
    return self_id_;
}

// Refresh all buckets from routing table
void k_routing_table::refresh_all_buckets() {
    std::shared_lock lock(mutex_);
    
    for (auto& bucket : table_) {
        bucket.refresh_from_routing();
    }
}

// Set k-value for all buckets
void k_routing_table::set_k_value(std::size_t k) {
    std::shared_lock lock(mutex_);
    
    for (auto& bucket : table_) {
        bucket.set_k_value(k);
    }
}

// Calculate branch index (internal)
unsigned short k_routing_table::calc_branch(k_node& kn) {
    // Calculate XOR distance between self and node
    auto distance = calc_node_xor_distance(self_id_, kn.get_id());
    
    // Branch index is the position of the first differing bit
    // For 160-bit space, this gives us 0-159
    return std::min(static_cast<unsigned short>(distance), 
                   static_cast<unsigned short>(K_BUCKET_COUNT - 1));
}

// Initialize buckets with routing interface
void k_routing_table::initialize_buckets() {
    for (std::size_t i = 0; i < K_BUCKET_COUNT; ++i) {
        table_[i].set_routing(routing_, static_cast<std::uint32_t>(i));
    }
}

// k_bucket_iterator implementation

// Default constructor
k_bucket_iterator::k_bucket_iterator()
    : routing_table_(nullptr)
    , branch_(-1) {
}

// Constructor with routing table and position
k_bucket_iterator::k_bucket_iterator(k_routing_table* routing_table,
                                   k_routing_table::routing_table::iterator bucket_itr,
                                   unsigned short branch)
    : routing_table_(routing_table)
    , bucket_itr_(bucket_itr)
    , branch_(branch) {
}

// Pre-increment operator
k_bucket_iterator& k_bucket_iterator::operator++() {
    if (routing_table_ && branch_ < k_routing_table::K_BUCKET_COUNT - 1) {
        ++branch_;
        ++bucket_itr_;
    } else {
        branch_ = -1; // Mark as invalid
    }
    return *this;
}

// Post-increment operator
k_bucket_iterator k_bucket_iterator::operator++(int) {
    k_bucket_iterator temp = *this;
    ++(*this);
    return temp;
}

// Pre-decrement operator
k_bucket_iterator& k_bucket_iterator::operator--() {
    if (routing_table_ && branch_ > 0) {
        --branch_;
        --bucket_itr_;
    } else {
        branch_ = -1; // Mark as invalid
    }
    return *this;
}

// Post-decrement operator
k_bucket_iterator k_bucket_iterator::operator--(int) {
    k_bucket_iterator temp = *this;
    --(*this);
    return temp;
}

// Equality comparison
bool k_bucket_iterator::operator==(const k_bucket_iterator& itr) {
    return routing_table_ == itr.routing_table_ && branch_ == itr.branch_;
}

// Dereference operator
k_bucket& k_bucket_iterator::operator*() {
    if (!routing_table_ || branch_ < 0 || branch_ >= k_routing_table::K_BUCKET_COUNT) {
        throw std::runtime_error("Invalid bucket iterator");
    }
    return *bucket_itr_;
}

// Get raw bucket reference
k_bucket& k_bucket_iterator::get_raw() {
    return **this;
}

// Get current branch index
unsigned short k_bucket_iterator::get_branch() {
    return static_cast<unsigned short>(std::max(0, branch_));
}

// Check if iterator is invalid
bool k_bucket_iterator::is_invalid() const {
    return !routing_table_ || branch_ < 0 || branch_ >= k_routing_table::K_BUCKET_COUNT;
}

// Reset iterator to beginning
k_bucket_iterator& k_bucket_iterator::to_begin() {
    if (routing_table_) {
        branch_ = 0;
        bucket_itr_ = routing_table_->table_.begin();
    }
    return *this;
}

// Get all nodes from current bucket
std::vector<k_node> k_bucket_iterator::get_nodes() {
    if (is_invalid()) {
        return {};
    }
    return bucket_itr_->get_nodes();
}

// Print iterator state (debug)
void k_bucket_iterator::_print_() const {
    std::cout << "k_bucket_iterator{branch=" << branch_ 
              << ", valid=" << !is_invalid() << "}\n";
}

// Create invalid iterator
k_bucket_iterator k_bucket_iterator::invalid() {
    return k_bucket_iterator{};
}

// Convert endpoints to k_nodes (legacy compatibility)
std::vector<k_node> eps_to_k_nodes(std::vector<boost::asio::ip::udp::endpoint> eps) {
    std::vector<k_node> result;
    result.reserve(eps.size());
    
    for (auto& ep : eps) {
        try {
            result.emplace_back(ep);
        } catch (const std::exception&) {
            // Skip invalid endpoints
        }
    }
    
    return result;
}

// Generate random k_node (legacy compatibility)
k_node generate_random_k_node() {
    auto random_id = node_id::random();
    
    // Generate random endpoint
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<std::uint16_t> port_dist(1024, 65535);
    std::uniform_int_distribution<int> ip_dist(1, 254);
    
    try {
        auto ip_str = std::to_string(ip_dist(gen)) + "." + 
                     std::to_string(ip_dist(gen)) + "." +
                     std::to_string(ip_dist(gen)) + "." +
                     std::to_string(ip_dist(gen));
        
        boost::asio::ip::address addr = boost::asio::ip::make_address(ip_str);
        boost::asio::ip::udp::endpoint ep{addr, port_dist(gen)};
        
        return k_node{random_id, ep};
    } catch (const std::exception&) {
        // Return blank node on failure
        return k_node::blank();
    }
}

} // namespace ss::kademlia