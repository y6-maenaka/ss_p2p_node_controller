#include "../../../../include/ss_p2p/dht/i_routing.hpp"
#include "../../../../include/ss_p2p/dht/node_id.hpp"
#include "../../../../include/ss_p2p/dht/kademlia_config.hpp"
#include "../../../../include/ss_p2p/core/result.hpp"

#include <vector>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <algorithm>
#include <chrono>
#include <random>
#include <unordered_map>
#include <atomic>
#include <boost/asio.hpp>

namespace ss::dht::impl {

/**
 * @brief K-bucket implementation for Kademlia routing table
 * 
 * Manages a single bucket of peer contacts with LRU replacement policy.
 * Thread-safe for concurrent access and supports efficient closest-node queries.
 */
class k_bucket {
private:
    /// Maximum number of peers in this bucket
    std::uint32_t max_size_;
    /// Peers in this bucket (ordered by last-seen, most recent at end)
    std::vector<peer_info> peers_;
    /// Mutex for thread-safe access
    mutable std::shared_mutex mutex_;
    /// Configuration reference
    const kademlia_config& config_;
    /// Bucket creation timestamp
    std::chrono::steady_clock::time_point created_at_;

public:
    /**
     * @brief Constructor
     * @param max_size Maximum bucket size
     * @param config Kademlia configuration
     */
    explicit k_bucket(std::uint32_t max_size, const kademlia_config& config) noexcept
        : max_size_(max_size)
        , config_(config)
        , created_at_(std::chrono::steady_clock::now()) {
        peers_.reserve(max_size_);
    }

    /**
     * @brief Add or update peer in bucket
     * @param peer Peer information to add/update
     * @return true if peer was added/updated, false if bucket is full and peer is not better
     */
    bool add_or_update_peer(peer_info peer) {
        std::unique_lock lock(mutex_);

        // Check if peer already exists
        auto it = std::find_if(peers_.begin(), peers_.end(),
            [&peer](const auto& p) { return p.id == peer.id; });

        if (it != peers_.end()) {
            // Update existing peer and move to end (most recently seen)
            *it = std::move(peer);
            it->update_liveness();
            
            // Move to end for LRU ordering
            std::rotate(it, it + 1, peers_.end());
            return true;
        }

        // Add new peer if there's space
        if (peers_.size() < max_size_) {
            peer.update_liveness();
            peers_.push_back(std::move(peer));
            return true;
        }

        // Bucket is full, check if we should replace least recently used peer
        // First try to find a failed/stale peer to replace
        auto stale_it = std::find_if(peers_.begin(), peers_.end(),
            [this](const auto& p) {
                return p.failure_count >= config_.max_failures ||
                       !p.is_alive(config_.node_timeout);
            });

        if (stale_it != peers_.end()) {
            // Replace stale peer
            *stale_it = std::move(peer);
            stale_it->update_liveness();
            
            // Move to end
            std::rotate(stale_it, stale_it + 1, peers_.end());
            return true;
        }

        // No stale peers, don't add if LRU eviction is disabled
        if (!config_.enable_lru_eviction) {
            return false;
        }

        // Replace least recently used peer (first in vector)
        peers_[0] = std::move(peer);
        peers_[0].update_liveness();
        
        // Move to end
        std::rotate(peers_.begin(), peers_.begin() + 1, peers_.end());
        return true;
    }

    /**
     * @brief Remove peer from bucket
     * @param node_id Node ID of peer to remove
     * @return true if peer was removed, false if not found
     */
    bool remove_peer(const node_id& node_id) {
        std::unique_lock lock(mutex_);

        auto it = std::find_if(peers_.begin(), peers_.end(),
            [&node_id](const auto& p) { return p.id == node_id; });

        if (it != peers_.end()) {
            peers_.erase(it);
            return true;
        }

        return false;
    }

    /**
     * @brief Get peer by node ID
     * @param node_id Node ID to search for
     * @return Peer info if found, nullptr otherwise
     */
    std::optional<peer_info> get_peer(const node_id& node_id) const {
        std::shared_lock lock(mutex_);

        auto it = std::find_if(peers_.begin(), peers_.end(),
            [&node_id](const auto& p) { return p.id == node_id; });

        return it != peers_.end() ? std::make_optional(*it) : std::nullopt;
    }

    /**
     * @brief Check if bucket contains peer
     * @param node_id Node ID to check
     * @return true if peer exists in bucket
     */
    bool has_peer(const node_id& node_id) const {
        std::shared_lock lock(mutex_);
        return std::any_of(peers_.begin(), peers_.end(),
            [&node_id](const auto& p) { return p.id == node_id; });
    }

    /**
     * @brief Get all peers in bucket
     * @return Vector of all peer information
     */
    std::vector<peer_info> get_all_peers() const {
        std::shared_lock lock(mutex_);
        return peers_;
    }

    /**
     * @brief Get closest peers to target
     * @param target Target node ID
     * @param count Maximum number of peers to return
     * @return Vector of closest peers sorted by distance
     */
    std::vector<peer_info> get_closest_peers(const node_id& target, std::uint32_t count) const {
        std::shared_lock lock(mutex_);

        auto result = peers_;
        
        // Sort by distance to target
        std::sort(result.begin(), result.end(),
            [&target](const auto& a, const auto& b) {
                return node_id_utils::is_closer(target, a.id, b.id);
            });

        // Limit result size
        if (result.size() > count) {
            result.resize(count);
        }

        return result;
    }

    /**
     * @brief Update peer liveness information
     * @param node_id Node ID of responding peer
     * @param rtt Round-trip time
     * @return true if peer was found and updated
     */
    bool update_peer_liveness(const node_id& node_id, std::chrono::microseconds rtt) {
        std::unique_lock lock(mutex_);

        auto it = std::find_if(peers_.begin(), peers_.end(),
            [&node_id](const auto& p) { return p.id == node_id; });

        if (it != peers_.end()) {
            it->update_liveness(rtt);
            
            // Move to end (most recently seen)
            std::rotate(it, it + 1, peers_.end());
            return true;
        }

        return false;
    }

    /**
     * @brief Record communication failure for peer
     * @param node_id Node ID of failed peer
     * @return true if peer should be removed due to excessive failures
     */
    bool record_peer_failure(const node_id& node_id) {
        std::unique_lock lock(mutex_);

        auto it = std::find_if(peers_.begin(), peers_.end(),
            [&node_id](const auto& p) { return p.id == node_id; });

        if (it != peers_.end()) {
            it->record_failure();
            return it->failure_count >= config_.max_failures;
        }

        return false;
    }

    /**
     * @brief Get peers that need liveness checking
     * @param max_age Maximum age before considering peer stale
     * @return Vector of stale peers
     */
    std::vector<peer_info> get_stale_peers(std::chrono::milliseconds max_age) const {
        std::shared_lock lock(mutex_);
        std::vector<peer_info> stale_peers;

        for (const auto& peer : peers_) {
            if (!peer.is_alive(max_age) && !peer.ping_pending) {
                stale_peers.push_back(peer);
            }
        }

        return stale_peers;
    }

    /**
     * @brief Mark peer as having pending ping
     * @param node_id Node ID to mark
     */
    void mark_ping_pending(const node_id& node_id) {
        std::unique_lock lock(mutex_);

        auto it = std::find_if(peers_.begin(), peers_.end(),
            [&node_id](const auto& p) { return p.id == node_id; });

        if (it != peers_.end()) {
            it->ping_pending = true;
        }
    }

    /**
     * @brief Get current bucket size
     * @return Number of peers in bucket
     */
    std::uint32_t size() const {
        std::shared_lock lock(mutex_);
        return static_cast<std::uint32_t>(peers_.size());
    }

    /**
     * @brief Check if bucket is full
     * @return true if bucket has reached maximum capacity
     */
    bool is_full() const {
        std::shared_lock lock(mutex_);
        return peers_.size() >= max_size_;
    }

    /**
     * @brief Check if bucket is empty
     * @return true if bucket has no peers
     */
    bool is_empty() const {
        std::shared_lock lock(mutex_);
        return peers_.empty();
    }

    /**
     * @brief Get bucket age
     * @return Duration since bucket was created
     */
    std::chrono::seconds age() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - created_at_);
    }

    /**
     * @brief Clean up expired/failed peers
     * @return Number of peers removed
     */
    std::uint32_t cleanup() {
        std::unique_lock lock(mutex_);
        
        auto initial_size = peers_.size();
        
        peers_.erase(
            std::remove_if(peers_.begin(), peers_.end(),
                [this](const auto& peer) {
                    return peer.failure_count >= config_.max_failures ||
                           !peer.is_alive(config_.node_timeout);
                }),
            peers_.end());

        return static_cast<std::uint32_t>(initial_size - peers_.size());
    }

    /**
     * @brief Get random peer from bucket
     * @return Random peer if bucket is not empty
     */
    std::optional<peer_info> get_random_peer() const {
        std::shared_lock lock(mutex_);
        
        if (peers_.empty()) {
            return std::nullopt;
        }

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<std::size_t> dist(0, peers_.size() - 1);
        
        return peers_[dist(gen)];
    }
};

/**
 * @brief Kademlia routing table implementation
 * 
 * Manages the complete routing table with 160 k-buckets for a 160-bit key space.
 * Provides thread-safe operations for peer management and closest-node queries.
 */
class kademlia_routing_table : public i_routing {
private:
    /// Local node identifier
    node_id local_node_id_;
    /// Configuration parameters
    kademlia_config config_;
    /// Array of k-buckets (160 buckets for 160-bit key space)
    std::array<std::unique_ptr<k_bucket>, 160> buckets_;
    /// Statistics
    mutable routing_stats stats_;
    /// Mutex for statistics updates
    mutable std::mutex stats_mutex_;
    /// IO context for async operations
    boost::asio::io_context& io_context_;
    /// Component running state
    std::atomic<bool> running_{false};
    /// Maintenance timer
    std::unique_ptr<boost::asio::steady_timer> maintenance_timer_;

public:
    /**
     * @brief Constructor
     * @param local_id Local node identifier
     * @param config Configuration parameters
     * @param io_ctx IO context for async operations
     */
    kademlia_routing_table(node_id local_id, 
                          kademlia_config config,
                          boost::asio::io_context& io_ctx)
        : local_node_id_(std::move(local_id))
        , config_(std::move(config))
        , io_context_(io_ctx)
        , maintenance_timer_(std::make_unique<boost::asio::steady_timer>(io_ctx)) {
        
        // Initialize k-buckets
        for (std::size_t i = 0; i < buckets_.size(); ++i) {
            buckets_[i] = std::make_unique<k_bucket>(config_.k_bucket_size, config_);
        }

        stats_.last_update = std::chrono::steady_clock::now();
    }

    /**
     * @brief Destructor
     */
    ~kademlia_routing_table() override {
        if (running_.load()) {
            // Stop synchronously in destructor
            boost::asio::co_spawn(io_context_, stop(), boost::asio::detached);
        }
    }

    // i_component interface implementation

    std::string name() const noexcept override {
        return "KademliaRoutingTable";
    }

    std::string version() const noexcept override {
        return "1.0.0";
    }

    bool is_running() const noexcept override {
        return running_.load();
    }

    ss::core::async_void start() override {
        if (running_.exchange(true)) {
            co_return; // Already running
        }

        // Start maintenance timer
        schedule_maintenance();
        co_return;
    }

    ss::core::async_void stop() override {
        if (!running_.exchange(false)) {
            co_return; // Already stopped
        }

        // Cancel maintenance timer
        if (maintenance_timer_) {
            maintenance_timer_->cancel();
        }

        co_return;
    }

    // i_routing interface implementation

    ss::core::async_result<ss::core::result<void, std::string>>
    add_peer(peer_info peer) override {
        auto bucket_idx = node_id_utils::bucket_index(local_node_id_, peer.id);
        
        if (bucket_idx >= buckets_.size()) {
            co_return ss::core::result<void, std::string>::err("Invalid bucket index");
        }

        bool added = buckets_[bucket_idx]->add_or_update_peer(std::move(peer));
        
        if (added) {
            update_stats([](auto& stats) {
                ++stats.peers_added;
                stats.last_update = std::chrono::steady_clock::now();
            });
        }

        co_return ss::core::result<void, std::string>::ok();
    }

    ss::core::async_result<ss::core::result<bool, std::string>>
    remove_peer(const node_id& node_id) override {
        auto bucket_idx = node_id_utils::bucket_index(local_node_id_, node_id);
        
        if (bucket_idx >= buckets_.size()) {
            co_return ss::core::result<bool, std::string>::err("Invalid bucket index");
        }

        bool removed = buckets_[bucket_idx]->remove_peer(node_id);
        
        if (removed) {
            update_stats([](auto& stats) {
                ++stats.peers_removed;
                stats.last_update = std::chrono::steady_clock::now();
            });
        }

        co_return ss::core::result<bool, std::string>::ok(removed);
    }

    ss::core::async_result<ss::core::result<std::vector<peer_info>, std::string>>
    find_closest_peers(const node_id& target, std::uint32_t count) override {
        if (count == 0) {
            count = config_.k_bucket_size;
        }

        std::vector<peer_info> all_peers;
        
        // Collect peers from all buckets
        for (const auto& bucket : buckets_) {
            auto bucket_peers = bucket->get_all_peers();
            all_peers.insert(all_peers.end(), bucket_peers.begin(), bucket_peers.end());
        }

        // Sort by distance to target
        std::sort(all_peers.begin(), all_peers.end(),
            [&target](const auto& a, const auto& b) {
                return node_id_utils::is_closer(target, a.id, b.id);
            });

        // Limit result size
        if (all_peers.size() > count) {
            all_peers.resize(count);
        }

        co_return ss::core::result<std::vector<peer_info>, std::string>::ok(std::move(all_peers));
    }

    ss::core::async_result<ss::core::result<std::vector<peer_info>, std::string>>
    find_node(const node_id& target, std::uint32_t max_results) override {
        auto start_time = std::chrono::steady_clock::now();
        
        update_stats([](auto& stats) { ++stats.find_operations; });

        // Get initial closest peers
        auto closest_result = co_await find_closest_peers(target, max_results);
        if (!closest_result) {
            update_stats([](auto& stats) { ++stats.failed_pings; });
            co_return closest_result;
        }

        auto peers = closest_result.value();
        
        // For now, return the closest peers we have locally
        // In a full implementation, this would perform iterative lookups
        // using the transport layer to query remote nodes
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time);
        
        update_stats([duration](auto& stats) {
            ++stats.successful_finds;
            stats.avg_find_time_us = (stats.avg_find_time_us + duration.count()) / 2;
        });

        co_return ss::core::result<std::vector<peer_info>, std::string>::ok(std::move(peers));
    }

    ss::core::async_result<ss::core::result<peer_info, std::string>>
    get_peer(const node_id& node_id) override {
        auto bucket_idx = node_id_utils::bucket_index(local_node_id_, node_id);
        
        if (bucket_idx >= buckets_.size()) {
            co_return ss::core::result<peer_info, std::string>::err("Invalid bucket index");
        }

        auto peer = buckets_[bucket_idx]->get_peer(node_id);
        if (!peer) {
            co_return ss::core::result<peer_info, std::string>::err("Peer not found");
        }

        co_return ss::core::result<peer_info, std::string>::ok(std::move(*peer));
    }

    bool has_peer(const node_id& node_id) const noexcept override {
        auto bucket_idx = node_id_utils::bucket_index(local_node_id_, node_id);
        
        if (bucket_idx >= buckets_.size()) {
            return false;
        }

        return buckets_[bucket_idx]->has_peer(node_id);
    }

    std::vector<peer_info> get_all_peers() const override {
        std::vector<peer_info> all_peers;
        
        for (const auto& bucket : buckets_) {
            auto bucket_peers = bucket->get_all_peers();
            all_peers.insert(all_peers.end(), bucket_peers.begin(), bucket_peers.end());
        }

        return all_peers;
    }

    std::vector<peer_info> get_bucket_peers(std::uint32_t bucket_index) const override {
        if (bucket_index >= buckets_.size()) {
            return {};
        }

        return buckets_[bucket_index]->get_all_peers();
    }

    ss::core::async_result<ss::core::result<void, std::string>>
    update_peer_liveness(const node_id& node_id, std::chrono::microseconds rtt) override {
        auto bucket_idx = node_id_utils::bucket_index(local_node_id_, node_id);
        
        if (bucket_idx >= buckets_.size()) {
            co_return ss::core::result<void, std::string>::err("Invalid bucket index");
        }

        bool updated = buckets_[bucket_idx]->update_peer_liveness(node_id, rtt);
        
        if (updated) {
            update_stats([](auto& stats) {
                stats.last_update = std::chrono::steady_clock::now();
            });
        }

        co_return ss::core::result<void, std::string>::ok();
    }

    ss::core::async_result<ss::core::result<bool, std::string>>
    record_peer_failure(const node_id& node_id) override {
        auto bucket_idx = node_id_utils::bucket_index(local_node_id_, node_id);
        
        if (bucket_idx >= buckets_.size()) {
            co_return ss::core::result<bool, std::string>::err("Invalid bucket index");
        }

        bool should_remove = buckets_[bucket_idx]->record_peer_failure(node_id);
        
        update_stats([](auto& stats) { ++stats.failed_pings; });

        if (should_remove) {
            bool removed = buckets_[bucket_idx]->remove_peer(node_id);
            if (removed) {
                update_stats([](auto& stats) { ++stats.peers_removed; });
            }
            co_return ss::core::result<bool, std::string>::ok(removed);
        }

        co_return ss::core::result<bool, std::string>::ok(false);
    }

    ss::core::async_void perform_maintenance() override {
        // Clean up expired/failed peers
        std::uint32_t total_removed = 0;
        for (auto& bucket : buckets_) {
            total_removed += bucket->cleanup();
        }

        if (total_removed > 0) {
            update_stats([total_removed](auto& stats) {
                stats.peers_removed += total_removed;
            });
        }

        // Update statistics
        update_stats([this](auto& stats) {
            stats.total_peers = peer_count();
            stats.active_buckets = active_bucket_count();
            stats.last_update = std::chrono::steady_clock::now();
        });

        co_return;
    }

    routing_stats get_stats() const noexcept override {
        std::lock_guard lock(stats_mutex_);
        return stats_;
    }

    void reset_stats() noexcept override {
        std::lock_guard lock(stats_mutex_);
        stats_ = routing_stats{};
        stats_.last_update = std::chrono::steady_clock::now();
    }

    const node_id& local_node_id() const noexcept override {
        return local_node_id_;
    }

    std::uint32_t bucket_index(const node_id& node_id) const noexcept override {
        return node_id_utils::bucket_index(local_node_id_, node_id);
    }

    std::uint32_t peer_count() const noexcept override {
        std::uint32_t count = 0;
        for (const auto& bucket : buckets_) {
            count += bucket->size();
        }
        return count;
    }

    std::uint32_t active_bucket_count() const noexcept override {
        std::uint32_t count = 0;
        for (const auto& bucket : buckets_) {
            if (!bucket->is_empty()) {
                ++count;
            }
        }
        return count;
    }

    std::vector<std::uint8_t> export_state() const override {
        // TODO: Implement state serialization
        return {};
    }

    ss::core::async_result<ss::core::result<void, std::string>>
    import_state(const std::vector<std::uint8_t>& data) override {
        // TODO: Implement state deserialization
        co_return ss::core::result<void, std::string>::ok();
    }

private:
    /**
     * @brief Schedule periodic maintenance
     */
    void schedule_maintenance() {
        if (!running_.load()) {
            return;
        }

        maintenance_timer_->expires_after(config_.maintenance_interval);
        maintenance_timer_->async_wait([this](boost::system::error_code ec) {
            if (!ec && running_.load()) {
                boost::asio::co_spawn(io_context_, perform_maintenance(), boost::asio::detached);
                schedule_maintenance();
            }
        });
    }

    /**
     * @brief Update statistics with thread safety
     * @param updater Function to update statistics
     */
    template<typename F>
    void update_stats(F&& updater) const {
        std::lock_guard lock(stats_mutex_);
        updater(stats_);
    }
};

} // namespace ss::dht::impl