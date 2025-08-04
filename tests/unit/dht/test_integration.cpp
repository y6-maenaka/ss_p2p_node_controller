#include <gtest/gtest.h>
#include "../../../include/ss_p2p/dht/i_routing.hpp"
#include "../../../include/ss_p2p/dht/i_storage.hpp"
#include "../../../include/ss_p2p/dht/node_id.hpp"
#include "../../../include/ss_p2p/dht/kademlia_config.hpp"

#include <memory>
#include <vector>
#include <chrono>
#include <thread>
#include <boost/asio.hpp>

namespace ss::dht::test {

/**
 * @brief Mock storage implementation for testing
 */
class mock_storage : public i_storage {
private:
    std::unordered_map<dht_key, dht_value> storage_;
    storage_config config_;
    storage_stats stats_;
    std::atomic<bool> running_{false};

public:
    mock_storage() {
        config_ = storage_config{};
        stats_ = storage_stats{};
    }

    // i_component interface
    std::string name() const noexcept override { return "MockStorage"; }
    bool is_running() const noexcept override { return running_.load(); }
    
    ss::core::async_void start() override {
        running_.store(true);
        co_return;
    }
    
    ss::core::async_void stop() override {
        running_.store(false);
        co_return;
    }

    // i_storage interface
    ss::core::async_result<ss::core::result<void, std::string>>
    store(const dht_key& key, dht_value value) override {
        if (!running_.load()) {
            co_return ss::core::result<void, std::string>::err("Storage not running");
        }
        
        storage_[key] = std::move(value);
        ++stats_.store_operations;
        ++stats_.total_entries;
        co_return ss::core::result<void, std::string>::ok();
    }

    ss::core::async_result<ss::core::result<dht_value, std::string>>
    retrieve(const dht_key& key) override {
        if (!running_.load()) {
            co_return ss::core::result<dht_value, std::string>::err("Storage not running");
        }
        
        ++stats_.retrieve_operations;
        auto it = storage_.find(key);
        if (it != storage_.end()) {
            co_return ss::core::result<dht_value, std::string>::ok(it->second);
        }
        
        co_return ss::core::result<dht_value, std::string>::err("Key not found");
    }

    bool contains(const dht_key& key) const noexcept override {
        return storage_.find(key) != storage_.end();
    }

    ss::core::async_result<ss::core::result<bool, std::string>>
    remove(const dht_key& key) override {
        auto erased = storage_.erase(key) > 0;
        if (erased) {
            --stats_.total_entries;
        }
        co_return ss::core::result<bool, std::string>::ok(erased);
    }

    std::vector<dht_key> get_all_keys() const override {
        std::vector<dht_key> keys;
        for (const auto& [key, value] : storage_) {
            keys.push_back(key);
        }
        return keys;
    }

    std::unordered_map<dht_key, dht_value> get_all_entries() const override {
        return storage_;
    }

    ss::core::async_result<std::vector<dht_key>>
    find_republish_candidates(std::chrono::seconds max_age) override {
        std::vector<dht_key> candidates;
        auto now = std::chrono::steady_clock::now();
        
        for (const auto& [key, value] : storage_) {
            auto age = std::chrono::duration_cast<std::chrono::seconds>(now - value.stored_at);
            if (age >= max_age) {
                candidates.push_back(key);
            }
        }
        
        co_return candidates;
    }

    ss::core::async_result<std::uint64_t> cleanup_expired() override {
        std::uint64_t removed = 0;
        auto now = std::chrono::steady_clock::now();
        
        for (auto it = storage_.begin(); it != storage_.end();) {
            if (it->second.is_expired()) {
                it = storage_.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        
        stats_.expired_removals += removed;
        stats_.total_entries -= removed;
        co_return removed;
    }

    std::vector<dht_key> get_responsible_keys(const ss::core::node_id& local_node_id,
                                             std::uint32_t replication_factor) const override {
        // Simple implementation: return all keys for testing
        return get_all_keys();
    }

    ss::core::async_result<ss::core::result<void, std::string>>
    update_ttl(const dht_key& key, std::chrono::seconds new_ttl) override {
        auto it = storage_.find(key);
        if (it != storage_.end()) {
            it->second.ttl = new_ttl;
            co_return ss::core::result<void, std::string>::ok();
        }
        co_return ss::core::result<void, std::string>::err("Key not found");
    }

    ss::core::async_result<ss::core::result<void, std::string>>
    refresh_entry(const dht_key& key) override {
        auto it = storage_.find(key);
        if (it != storage_.end()) {
            it->second.refresh();
            co_return ss::core::result<void, std::string>::ok();
        }
        co_return ss::core::result<void, std::string>::err("Key not found");
    }

    storage_stats get_stats() const noexcept override { return stats_; }
    void reset_stats() noexcept override { stats_ = storage_stats{}; }
    storage_config get_config() const noexcept override { return config_; }
    
    ss::core::result<void, std::string> set_config(const storage_config& config) override {
        config_ = config;
        return ss::core::result<void, std::string>::ok();
    }

    std::uint64_t entry_count() const noexcept override {
        return storage_.size();
    }

    std::uint64_t storage_size() const noexcept override {
        std::uint64_t size = 0;
        for (const auto& [key, value] : storage_) {
            size += value.data.size();
        }
        return size;
    }

    bool has_capacity(std::uint64_t additional_entries = 1, 
                     std::uint64_t additional_bytes = 0) const noexcept override {
        return storage_.size() + additional_entries <= config_.max_entries &&
               storage_size() + additional_bytes <= config_.max_storage_bytes;
    }

    std::uint64_t add_change_handler(value_change_handler handler) override {
        // Not implemented for mock
        return 0;
    }

    bool remove_change_handler(std::uint64_t handler_id) override {
        // Not implemented for mock
        return false;
    }

    std::vector<std::uint8_t> export_state() const override {
        // Not implemented for mock
        return {};
    }

    ss::core::async_result<ss::core::result<void, std::string>>
    import_state(const std::vector<std::uint8_t>& data) override {
        // Not implemented for mock
        co_return ss::core::result<void, std::string>::ok();
    }

    ss::core::async_void perform_maintenance() override {
        co_await cleanup_expired();
        co_return;
    }
};

/**
 * @brief Integration test fixture
 */
class DHT_IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        io_context_ = std::make_unique<boost::asio::io_context>();
        
        // Create test configuration
        config_ = kademlia_config::testing_config();
        
        // Create local node ID
        local_node_id_ = node_id::random();
        
        // Create mock storage
        storage_ = std::make_shared<mock_storage>();
    }

    void TearDown() override {
        if (io_context_) {
            io_context_->stop();
        }
    }

    std::unique_ptr<boost::asio::io_context> io_context_;
    kademlia_config config_;
    node_id local_node_id_;
    std::shared_ptr<mock_storage> storage_;
};

/**
 * @brief Test basic DHT value storage and retrieval
 */
TEST_F(DHT_IntegrationTest, BasicStorageAndRetrieval) {
    boost::asio::co_spawn(*io_context_, [this]() -> boost::asio::awaitable<void> {
        // Start storage
        co_await storage_->start();
        
        // Create test data
        auto key = node_id::random();
        auto value_data = std::vector<std::uint8_t>{'h', 'e', 'l', 'l', 'o'};
        auto ttl = std::chrono::seconds{300};
        dht_value value{value_data, ttl, local_node_id_};
        
        // Store value
        auto store_result = co_await storage_->store(key, value);
        EXPECT_TRUE(store_result.is_ok());
        
        // Retrieve value
        auto retrieve_result = co_await storage_->retrieve(key);
        EXPECT_TRUE(retrieve_result.is_ok());
        
        if (retrieve_result.is_ok()) {
            auto retrieved_value = retrieve_result.value();
            EXPECT_EQ(retrieved_value.data, value_data);
            EXPECT_EQ(retrieved_value.publisher, local_node_id_);
        }
        
        // Check contains
        EXPECT_TRUE(storage_->contains(key));
        
        // Remove value
        auto remove_result = co_await storage_->remove(key);
        EXPECT_TRUE(remove_result.is_ok());
        EXPECT_TRUE(remove_result.value());
        
        // Verify removal
        EXPECT_FALSE(storage_->contains(key));
        auto retrieve_after_remove = co_await storage_->retrieve(key);
        EXPECT_FALSE(retrieve_after_remove.is_ok());
        
        co_await storage_->stop();
    }, boost::asio::detached);
    
    io_context_->run();
}

/**
 * @brief Test TTL expiration handling
 */
TEST_F(DHT_IntegrationTest, TTL_ExpirationHandling) {
    boost::asio::co_spawn(*io_context_, [this]() -> boost::asio::awaitable<void> {
        co_await storage_->start();
        
        // Create short-lived value
        auto key = node_id::random();
        auto value_data = std::vector<std::uint8_t>{'t', 'e', 's', 't'};
        auto short_ttl = std::chrono::seconds{1};
        dht_value value{value_data, short_ttl, local_node_id_};
        
        // Store value
        auto store_result = co_await storage_->store(key, value);
        EXPECT_TRUE(store_result.is_ok());
        
        // Value should be retrievable immediately
        auto retrieve_result = co_await storage_->retrieve(key);
        EXPECT_TRUE(retrieve_result.is_ok());
        
        // Wait for expiration
        auto timer = boost::asio::steady_timer{*io_context_, std::chrono::seconds{2}};
        co_await timer.async_wait(boost::asio::use_awaitable);
        
        // Clean up expired entries
        auto cleanup_result = co_await storage_->cleanup_expired();
        EXPECT_GT(cleanup_result, 0u);
        
        // Value should no longer exist
        EXPECT_FALSE(storage_->contains(key));
        
        co_await storage_->stop();
    }, boost::asio::detached);
    
    io_context_->run();
}

/**
 * @brief Test multiple concurrent operations
 */
TEST_F(DHT_IntegrationTest, ConcurrentOperations) {
    boost::asio::co_spawn(*io_context_, [this]() -> boost::asio::awaitable<void> {
        co_await storage_->start();
        
        const int num_operations = 100;
        std::vector<boost::asio::awaitable<void>> operations;
        
        // Create concurrent store operations
        for (int i = 0; i < num_operations; ++i) {
            operations.push_back([this, i]() -> boost::asio::awaitable<void> {
                auto key = node_id::random();
                auto value_data = std::vector<std::uint8_t>{'v', static_cast<std::uint8_t>(i)};
                auto ttl = std::chrono::seconds{300};
                dht_value value{value_data, ttl, local_node_id_};
                
                auto result = co_await storage_->store(key, value);
                EXPECT_TRUE(result.is_ok());
            }());
        }
        
        // Wait for all operations to complete
        for (auto& op : operations) {
            co_await std::move(op);
        }
        
        // Verify statistics
        auto stats = storage_->get_stats();
        EXPECT_EQ(stats.store_operations, num_operations);
        EXPECT_EQ(stats.total_entries, num_operations);
        
        co_await storage_->stop();
    }, boost::asio::detached);
    
    io_context_->run();
}

/**
 * @brief Test storage capacity limits
 */
TEST_F(DHT_IntegrationTest, StorageCapacityLimits) {
    boost::asio::co_spawn(*io_context_, [this]() -> boost::asio::awaitable<void> {
        co_await storage_->start();
        
        // Set small capacity limits
        storage_config config;
        config.max_entries = 5;
        config.max_storage_bytes = 100;
        auto set_config_result = storage_->set_config(config);
        EXPECT_TRUE(set_config_result.is_ok());
        
        // Check initial capacity
        EXPECT_TRUE(storage_->has_capacity());
        EXPECT_TRUE(storage_->has_capacity(5, 100));
        EXPECT_FALSE(storage_->has_capacity(6, 0));
        EXPECT_FALSE(storage_->has_capacity(0, 101));
        
        // Fill storage to capacity
        std::vector<node_id> keys;
        for (int i = 0; i < 5; ++i) {
            auto key = node_id::random();
            keys.push_back(key);
            
            auto value_data = std::vector<std::uint8_t>(10, static_cast<std::uint8_t>(i));
            auto ttl = std::chrono::seconds{300};
            dht_value value{value_data, ttl, local_node_id_};
            
            auto result = co_await storage_->store(key, value);
            EXPECT_TRUE(result.is_ok());
        }
        
        // Verify capacity is reached
        EXPECT_EQ(storage_->entry_count(), 5u);
        EXPECT_EQ(storage_->storage_size(), 50u);
        EXPECT_FALSE(storage_->has_capacity());
        
        co_await storage_->stop();
    }, boost::asio::detached);
    
    io_context_->run();
}

/**
 * @brief Test storage maintenance operations
 */
TEST_F(DHT_IntegrationTest, StorageMaintenanceOperations) {
    boost::asio::co_spawn(*io_context_, [this]() -> boost::asio::awaitable<void> {
        co_await storage_->start();
        
        // Store values with different TTLs
        std::vector<node_id> short_lived_keys;
        std::vector<node_id> long_lived_keys;
        
        // Short-lived values (1 second TTL)
        for (int i = 0; i < 3; ++i) {
            auto key = node_id::random();
            short_lived_keys.push_back(key);
            
            auto value_data = std::vector<std::uint8_t>{'s', static_cast<std::uint8_t>(i)};
            auto ttl = std::chrono::seconds{1};
            dht_value value{value_data, ttl, local_node_id_};
            
            auto result = co_await storage_->store(key, value);
            EXPECT_TRUE(result.is_ok());
        }
        
        // Long-lived values (300 second TTL)
        for (int i = 0; i < 3; ++i) {
            auto key = node_id::random();
            long_lived_keys.push_back(key);
            
            auto value_data = std::vector<std::uint8_t>{'l', static_cast<std::uint8_t>(i)};
            auto ttl = std::chrono::seconds{300};
            dht_value value{value_data, ttl, local_node_id_};
            
            auto result = co_await storage_->store(key, value);
            EXPECT_TRUE(result.is_ok());
        }
        
        // Initial state
        EXPECT_EQ(storage_->entry_count(), 6u);
        
        // Wait for short-lived values to expire
        auto timer = boost::asio::steady_timer{*io_context_, std::chrono::seconds{2}};
        co_await timer.async_wait(boost::asio::use_awaitable);
        
        // Perform maintenance
        co_await storage_->perform_maintenance();
        
        // Check that short-lived values are gone but long-lived remain
        EXPECT_EQ(storage_->entry_count(), 3u);
        
        for (const auto& key : short_lived_keys) {
            EXPECT_FALSE(storage_->contains(key));
        }
        
        for (const auto& key : long_lived_keys) {
            EXPECT_TRUE(storage_->contains(key));
        }
        
        // Check statistics
        auto stats = storage_->get_stats();
        EXPECT_EQ(stats.expired_removals, 3u);
        
        co_await storage_->stop();
    }, boost::asio::detached);
    
    io_context_->run();
}

/**
 * @brief Test republish candidate identification
 */
TEST_F(DHT_IntegrationTest, RepublishCandidateIdentification) {
    boost::asio::co_spawn(*io_context_, [this]() -> boost::asio::awaitable<void> {
        co_await storage_->start();
        
        // Store some values
        std::vector<node_id> old_keys;
        std::vector<node_id> new_keys;
        
        // Old values (will need republishing)
        for (int i = 0; i < 3; ++i) {
            auto key = node_id::random();
            old_keys.push_back(key);
            
            auto value_data = std::vector<std::uint8_t>{'o', static_cast<std::uint8_t>(i)};
            auto ttl = std::chrono::seconds{300};
            dht_value value{value_data, ttl, local_node_id_};
            
            // Manually set old timestamp
            value.stored_at = std::chrono::steady_clock::now() - std::chrono::seconds{120};
            
            auto result = co_await storage_->store(key, value);
            EXPECT_TRUE(result.is_ok());
        }
        
        // Wait a bit to ensure different timestamps
        auto timer = boost::asio::steady_timer{*io_context_, std::chrono::milliseconds{10}};
        co_await timer.async_wait(boost::asio::use_awaitable);
        
        // New values (don't need republishing yet)
        for (int i = 0; i < 3; ++i) {
            auto key = node_id::random();
            new_keys.push_back(key);
            
            auto value_data = std::vector<std::uint8_t>{'n', static_cast<std::uint8_t>(i)};
            auto ttl = std::chrono::seconds{300};
            dht_value value{value_data, ttl, local_node_id_};
            
            auto result = co_await storage_->store(key, value);
            EXPECT_TRUE(result.is_ok());
        }
        
        // Find republish candidates (older than 60 seconds)
        auto candidates = co_await storage_->find_republish_candidates(std::chrono::seconds{60});
        
        // Should find the old values but not the new ones
        EXPECT_EQ(candidates.size(), 3u);
        
        for (const auto& key : old_keys) {
            EXPECT_TRUE(std::find(candidates.begin(), candidates.end(), key) != candidates.end());
        }
        
        for (const auto& key : new_keys) {
            EXPECT_TRUE(std::find(candidates.begin(), candidates.end(), key) == candidates.end());
        }
        
        co_await storage_->stop();
    }, boost::asio::detached);
    
    io_context_->run();
}

/**
 * @brief Test value update and refresh operations
 */
TEST_F(DHT_IntegrationTest, ValueUpdateAndRefresh) {
    boost::asio::co_spawn(*io_context_, [this]() -> boost::asio::awaitable<void> {
        co_await storage_->start();
        
        // Store initial value
        auto key = node_id::random();
        auto value_data = std::vector<std::uint8_t>{'i', 'n', 'i', 't', 'i', 'a', 'l'};
        auto ttl = std::chrono::seconds{300};
        dht_value value{value_data, ttl, local_node_id_};
        
        auto store_result = co_await storage_->store(key, value);
        EXPECT_TRUE(store_result.is_ok());
        
        // Get initial stored timestamp
        auto retrieve_result = co_await storage_->retrieve(key);
        EXPECT_TRUE(retrieve_result.is_ok());
        auto initial_timestamp = retrieve_result.value().stored_at;
        
        // Wait a bit
        auto timer = boost::asio::steady_timer{*io_context_, std::chrono::milliseconds{10}};
        co_await timer.async_wait(boost::asio::use_awaitable);
        
        // Refresh the entry
        auto refresh_result = co_await storage_->refresh_entry(key);
        EXPECT_TRUE(refresh_result.is_ok());
        
        // Check that timestamp was updated
        retrieve_result = co_await storage_->retrieve(key);
        EXPECT_TRUE(retrieve_result.is_ok());
        auto refreshed_timestamp = retrieve_result.value().stored_at;
        EXPECT_GT(refreshed_timestamp, initial_timestamp);
        
        // Update TTL
        auto new_ttl = std::chrono::seconds{600};
        auto update_ttl_result = co_await storage_->update_ttl(key, new_ttl);
        EXPECT_TRUE(update_ttl_result.is_ok());
        
        // Verify TTL was updated
        retrieve_result = co_await storage_->retrieve(key);
        EXPECT_TRUE(retrieve_result.is_ok());
        EXPECT_EQ(retrieve_result.value().ttl, new_ttl);
        
        co_await storage_->stop();
    }, boost::asio::detached);
    
    io_context_->run();
}

/**
 * @brief Test error handling and edge cases
 */
TEST_F(DHT_IntegrationTest, ErrorHandlingAndEdgeCases) {
    boost::asio::co_spawn(*io_context_, [this]() -> boost::asio::awaitable<void> {
        // Test operations on stopped storage
        auto key = node_id::random();
        auto value_data = std::vector<std::uint8_t>{'t', 'e', 's', 't'};
        auto ttl = std::chrono::seconds{300};
        dht_value value{value_data, ttl, local_node_id_};
        
        // Should fail when storage is not running
        auto store_result = co_await storage_->store(key, value);
        EXPECT_FALSE(store_result.is_ok());
        
        auto retrieve_result = co_await storage_->retrieve(key);
        EXPECT_FALSE(retrieve_result.is_ok());
        
        // Start storage
        co_await storage_->start();
        
        // Test operations on non-existent keys
        auto non_existent_key = node_id::random();
        
        auto retrieve_non_existent = co_await storage_->retrieve(non_existent_key);
        EXPECT_FALSE(retrieve_non_existent.is_ok());
        
        auto remove_non_existent = co_await storage_->remove(non_existent_key);
        EXPECT_TRUE(remove_non_existent.is_ok());
        EXPECT_FALSE(remove_non_existent.value()); // Should return false (not found)
        
        auto refresh_non_existent = co_await storage_->refresh_entry(non_existent_key);
        EXPECT_FALSE(refresh_non_existent.is_ok());
        
        auto update_ttl_non_existent = co_await storage_->update_ttl(non_existent_key, ttl);
        EXPECT_FALSE(update_ttl_non_existent.is_ok());
        
        // Test duplicate store operations
        store_result = co_await storage_->store(key, value);
        EXPECT_TRUE(store_result.is_ok());
        
        // Store same key again (should overwrite)
        auto updated_value_data = std::vector<std::uint8_t>{'u', 'p', 'd', 'a', 't', 'e', 'd'};
        dht_value updated_value{updated_value_data, ttl, local_node_id_};
        
        auto store_updated_result = co_await storage_->store(key, updated_value);
        EXPECT_TRUE(store_updated_result.is_ok());
        
        // Verify the value was updated
        retrieve_result = co_await storage_->retrieve(key);
        EXPECT_TRUE(retrieve_result.is_ok());
        EXPECT_EQ(retrieve_result.value().data, updated_value_data);
        
        co_await storage_->stop();
    }, boost::asio::detached);
    
    io_context_->run();
}

} // namespace ss::dht::test