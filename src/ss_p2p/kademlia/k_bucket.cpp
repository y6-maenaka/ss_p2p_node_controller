#include <ss_p2p/kademlia/k_bucket.hpp>

#include <iostream>
#include <algorithm>
#include <chrono>

namespace ss::kademlia {

// Default constructor
k_bucket::k_bucket() 
    : bucket_index_(0)
    , k_value_(DEFAULT_K)
    , cache_valid_(false)
    , last_cache_update_{} {
}

// Constructor with routing table reference
k_bucket::k_bucket(std::shared_ptr<ss::dht::i_routing> routing, std::uint32_t bucket_index)
    : routing_(std::move(routing))
    , bucket_index_(bucket_index)
    , k_value_(DEFAULT_K)
    , cache_valid_(false)
    , last_cache_update_{} {
}

// Copy constructor
k_bucket::k_bucket(const k_bucket& other) {
    std::shared_lock lock(other.mutex_);
    nodes_ = other.nodes_;
    routing_ = other.routing_;
    bucket_index_ = other.bucket_index_;
    k_value_ = other.k_value_;
    cache_valid_ = other.cache_valid_;
    last_cache_update_ = other.last_cache_update_;
}

// Move constructor
k_bucket::k_bucket(k_bucket&& other) noexcept {
    std::unique_lock lock(other.mutex_);
    nodes_ = std::move(other.nodes_);
    routing_ = std::move(other.routing_);
    bucket_index_ = other.bucket_index_;
    k_value_ = other.k_value_;
    cache_valid_ = other.cache_valid_;
    last_cache_update_ = other.last_cache_update_;
    
    other.cache_valid_ = false;
}

// Copy assignment operator
k_bucket& k_bucket::operator=(const k_bucket& other) {
    if (this != &other) {
        std::unique_lock lock1(mutex_, std::defer_lock);
        std::shared_lock lock2(other.mutex_, std::defer_lock);
        std::lock(lock1, lock2);
        
        nodes_ = other.nodes_;
        routing_ = other.routing_;
        bucket_index_ = other.bucket_index_;
        k_value_ = other.k_value_;
        cache_valid_ = other.cache_valid_;
        last_cache_update_ = other.last_cache_update_;
    }
    return *this;
}

// Move assignment operator
k_bucket& k_bucket::operator=(k_bucket&& other) noexcept {
    if (this != &other) {
        std::unique_lock lock1(mutex_, std::defer_lock);
        std::unique_lock lock2(other.mutex_, std::defer_lock);
        std::lock(lock1, lock2);
        
        nodes_ = std::move(other.nodes_);
        routing_ = std::move(other.routing_);
        bucket_index_ = other.bucket_index_;
        k_value_ = other.k_value_;
        cache_valid_ = other.cache_valid_;
        last_cache_update_ = other.last_cache_update_;
        
        other.cache_valid_ = false;
    }
    return *this;
}

// Auto-update bucket with node
k_bucket::update_state k_bucket::auto_update(k_node kn) {
    std::unique_lock lock(mutex_);
    
    try {
        // Update cache if needed
        if (!is_cache_valid()) {
            update_cache();
        }
        
        // Check if node already exists
        auto it = find_node(kn);
        if (it != nodes_.end()) {
            // Move existing node to back
            k_node existing_node = std::move(*it);
            nodes_.erase(it);
            
            // Update liveness
            existing_node.update_liveness();
            nodes_.push_back(std::move(existing_node));
            
            // Update routing table if available
            if (routing_) {
                // Note: This is async, so we don't wait for result
                routing_->update_peer_liveness(kn.peer_info().id);
            }
            
            cache_valid_ = false; // Invalidate cache
            return moved_back;
        }
        
        // Check if bucket is full
        if (nodes_.size() >= k_value_) {
            return overflow;
        }
        
        // Add new node to back
        nodes_.push_back(std::move(kn));
        
        // Update routing table if available
        if (routing_) {
            // Note: This is async, so we don't wait for result  
            routing_->add_peer(nodes_.back().peer_info());
        }
        
        cache_valid_ = false; // Invalidate cache
        return added_back;
        
    } catch (const std::exception&) {
        return error;
    }
}

// Get nodes from front of bucket
k_bucket::k_nodes k_bucket::get_node_front(std::size_t count, const k_nodes& ignore_nodes) {
    std::shared_lock lock(mutex_);
    
    if (!is_cache_valid()) {
        lock.unlock();
        std::unique_lock write_lock(mutex_);
        update_cache();
        write_lock.unlock();
        lock.lock();
    }
    
    k_nodes result;
    result.reserve(std::min(count, nodes_.size()));
    
    for (const auto& node : nodes_) {
        if (result.size() >= count) break;
        
        // Check if node should be ignored
        bool should_ignore = std::any_of(ignore_nodes.begin(), ignore_nodes.end(),
            [&node](const k_node& ignore) { return node == ignore; });
        
        if (!should_ignore) {
            result.push_back(node);
        }
    }
    
    return result;
}

// Get nodes from back of bucket
k_bucket::k_nodes k_bucket::get_node_back(std::size_t count, const k_nodes& ignore_nodes) {
    std::shared_lock lock(mutex_);
    
    if (!is_cache_valid()) {
        lock.unlock();
        std::unique_lock write_lock(mutex_);
        update_cache();
        write_lock.unlock();
        lock.lock();
    }
    
    k_nodes result;
    result.reserve(std::min(count, nodes_.size()));
    
    // Iterate from back
    for (auto it = nodes_.rbegin(); it != nodes_.rend() && result.size() < count; ++it) {
        // Check if node should be ignored
        bool should_ignore = std::any_of(ignore_nodes.begin(), ignore_nodes.end(),
            [it](const k_node& ignore) { return *it == ignore; });
        
        if (!should_ignore) {
            result.push_back(*it);
        }
    }
    
    return result;
}

// Get all nodes in bucket
k_bucket::k_nodes k_bucket::get_nodes() {
    std::shared_lock lock(mutex_);
    
    if (!is_cache_valid()) {
        lock.unlock();
        std::unique_lock write_lock(mutex_);
        update_cache();
        write_lock.unlock();
        lock.lock();
    }
    
    return nodes_;
}

// Add node to back of bucket
void k_bucket::add_back(k_node kn) {
    std::unique_lock lock(mutex_);
    
    // Check if already exists
    auto it = find_node(kn);
    if (it != nodes_.end()) {
        return; // Already exists, don't add duplicate
    }
    
    // Add to back if not full
    if (nodes_.size() < k_value_) {
        nodes_.push_back(std::move(kn));
        
        // Update routing table if available
        if (routing_) {
            routing_->add_peer(nodes_.back().peer_info());
        }
        
        cache_valid_ = false;
    }
}

// Move existing node to back of bucket
void k_bucket::move_back(k_node& kn) {
    std::unique_lock lock(mutex_);
    
    auto it = find_node(kn);
    if (it != nodes_.end()) {
        // Update liveness
        it->update_liveness();
        
        // Move to back
        k_node node_to_move = std::move(*it);
        nodes_.erase(it);
        nodes_.push_back(std::move(node_to_move));
        
        // Update routing table if available
        if (routing_) {
            routing_->update_peer_liveness(kn.peer_info().id);
        }
        
        cache_valid_ = false;
    }
}

// Delete node from bucket
void k_bucket::delete_node(k_node& kn) {
    std::unique_lock lock(mutex_);
    
    auto it = find_node(kn);
    if (it != nodes_.end()) {
        // Remove from routing table if available
        if (routing_) {
            routing_->remove_peer(it->peer_info().id);
        }
        
        nodes_.erase(it);
        cache_valid_ = false;
    }
}

// Swap node in bucket
void k_bucket::swap_node(k_node& node_src, k_node node_dest) {
    std::unique_lock lock(mutex_);
    
    auto it = find_node(node_src);
    if (it != nodes_.end()) {
        // Remove old node from routing table
        if (routing_) {
            routing_->remove_peer(it->peer_info().id);
        }
        
        // Replace with new node
        *it = std::move(node_dest);
        
        // Add new node to routing table
        if (routing_) {
            routing_->add_peer(it->peer_info());
        }
        
        cache_valid_ = false;
    }
}

// Check if node exists in bucket
bool k_bucket::is_exist(k_node& kn) const {
    std::shared_lock lock(mutex_);
    
    if (!is_cache_valid()) {
        lock.unlock();
        std::unique_lock write_lock(mutex_);
        update_cache();
        write_lock.unlock();
        lock.lock();
    }
    
    return find_node(kn) != nodes_.cend();
}

// Check if bucket is full
bool k_bucket::is_full() const {
    std::shared_lock lock(mutex_);
    
    if (!is_cache_valid()) {
        lock.unlock();
        std::unique_lock write_lock(mutex_);
        update_cache();
        write_lock.unlock();
        lock.lock();
    }
    
    return nodes_.size() >= k_value_;
}

// Get current node count in bucket
std::size_t k_bucket::get_node_count() const {
    std::shared_lock lock(mutex_);
    
    if (!is_cache_valid()) {
        lock.unlock();
        std::unique_lock write_lock(mutex_);
        update_cache();
        write_lock.unlock();
        lock.lock();
    }
    
    return nodes_.size();
}

// Print bucket contents vertically
void k_bucket::print_vertical() const {
    std::shared_lock lock(mutex_);
    
    if (!is_cache_valid()) {
        lock.unlock();
        std::unique_lock write_lock(mutex_);
        update_cache();
        write_lock.unlock();
        lock.lock();
    }
    
    std::cout << "k_bucket[" << bucket_index_ << "] (" << nodes_.size() << "/" << k_value_ << "):\n";
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        std::cout << "  [" << i << "] ";
        nodes_[i].print();
        std::cout << "\n";
    }
}

// Print bucket contents horizontally
void k_bucket::print_horizontal() const {
    std::shared_lock lock(mutex_);
    
    if (!is_cache_valid()) {
        lock.unlock();
        std::unique_lock write_lock(mutex_);
        update_cache();
        write_lock.unlock();
        lock.lock();
    }
    
    std::cout << "k_bucket[" << bucket_index_ << "](" << nodes_.size() << "): ";
    for (const auto& node : nodes_) {
        auto hex = node.peer_info().id.to_hex();
        std::cout << hex.substr(0, 8) << "... ";
    }
    std::cout << "\n";
}

// Set routing table reference
void k_bucket::set_routing(std::shared_ptr<ss::dht::i_routing> routing, std::uint32_t bucket_index) {
    std::unique_lock lock(mutex_);
    routing_ = std::move(routing);
    bucket_index_ = bucket_index;
    cache_valid_ = false; // Force refresh
}

// Refresh bucket contents from routing table
void k_bucket::refresh_from_routing() {
    std::unique_lock lock(mutex_);
    update_cache();
}

// Set maximum bucket size
void k_bucket::set_k_value(std::size_t k) {
    std::unique_lock lock(mutex_);
    k_value_ = k;
    
    // Trim nodes if necessary
    if (nodes_.size() > k_value_) {
        nodes_.resize(k_value_);
        cache_valid_ = false;
    }
}

// Get maximum bucket size
std::size_t k_bucket::get_k_value() const noexcept {
    std::shared_lock lock(mutex_);
    return k_value_;
}

// Update cache from routing table
void k_bucket::update_cache() const {
    if (!routing_) {
        cache_valid_ = true;
        last_cache_update_ = std::chrono::steady_clock::now();
        return;
    }
    
    try {
        // Get peers from routing table for this bucket
        auto bucket_peers = routing_->get_bucket_peers(bucket_index_);
        
        // Convert to k_nodes
        nodes_.clear();
        nodes_.reserve(bucket_peers.size());
        
        for (const auto& peer : bucket_peers) {
            nodes_.emplace_back(peer);
        }
        
        // Limit to k_value
        if (nodes_.size() > k_value_) {
            nodes_.resize(k_value_);
        }
        
        cache_valid_ = true;
        last_cache_update_ = std::chrono::steady_clock::now();
        
    } catch (const std::exception&) {
        // On error, keep existing cache but mark it valid to avoid infinite loops
        cache_valid_ = true;
        last_cache_update_ = std::chrono::steady_clock::now();
    }
}

// Check if cache is valid
bool k_bucket::is_cache_valid() const {
    if (!cache_valid_) {
        return false;
    }
    
    // Cache expires after 30 seconds
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::seconds>(now - last_cache_update_);
    return age < std::chrono::seconds{30};
}

// Find node in local cache
std::vector<k_node>::iterator k_bucket::find_node(const k_node& kn) {
    return std::find_if(nodes_.begin(), nodes_.end(),
        [&kn](const k_node& node) { return node == kn; });
}

// Find node in local cache (const version)
std::vector<k_node>::const_iterator k_bucket::find_node(const k_node& kn) const {
    return std::find_if(nodes_.cbegin(), nodes_.cend(),
        [&kn](const k_node& node) { return node == kn; });
}

// Filter out ignored nodes
k_bucket::k_nodes k_bucket::filter_ignore_nodes(const k_nodes& nodes, const k_nodes& ignore_nodes) const {
    k_nodes result;
    result.reserve(nodes.size());
    
    for (const auto& node : nodes) {
        bool should_ignore = std::any_of(ignore_nodes.begin(), ignore_nodes.end(),
            [&node](const k_node& ignore) { return node == ignore; });
        
        if (!should_ignore) {
            result.push_back(node);
        }
    }
    
    return result;
}

} // namespace ss::kademlia