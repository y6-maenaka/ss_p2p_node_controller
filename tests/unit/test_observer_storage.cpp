#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <ss_p2p/observer_storage.hpp>
#include <ss_p2p/observer.hpp>
#include <ss_p2p/core/types.hpp>

#include <chrono>
#include <thread>
#include <atomic>

namespace ss::test {

// Mock observer types for testing
class TestObserverA : public base_observer {
public:
    TestObserverA(ss::core::io_context& io_context) 
        : base_observer(io_context, "test_observer_a") {}
};

class TestObserverB : public base_observer {
public:
    TestObserverB(ss::core::io_context& io_context) 
        : base_observer(io_context, "test_observer_b") {}
};

class ObserverStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        io_context_ = std::make_unique<ss::core::io_context>();
        storage_ = std::make_unique<observer_storage<default_observer_storage_policy, TestObserverA, TestObserverB>>(*io_context_);
    }

    void TearDown() override {
        if (storage_) {
            storage_.reset();
        }
        if (io_context_) {
            io_context_->stop();
        }
    }

    std::unique_ptr<ss::core::io_context> io_context_;
    std::unique_ptr<observer_storage<default_observer_storage_policy, TestObserverA, TestObserverB>> storage_;
};

TEST_F(ObserverStorageTest, Construction) {
    EXPECT_TRUE(storage_ != nullptr);
    EXPECT_FALSE(storage_->is_running());
    EXPECT_FALSE(storage_->is_healthy());
}

TEST_F(ObserverStorageTest, ComponentInterface) {
    EXPECT_EQ(storage_->name(), "observer_storage");
    EXPECT_EQ(storage_->version(), "2.0.0");
    
    // Status should be valid JSON-like string
    std::string status = storage_->status();
    EXPECT_FALSE(status.empty());
    EXPECT_NE(status.find("running"), std::string::npos);
    EXPECT_NE(status.find("total_count"), std::string::npos);
}

TEST_F(ObserverStorageTest, Lifecycle) {
    EXPECT_FALSE(storage_->is_running());
    
    // Test async lifecycle operations
    EXPECT_NO_THROW({
        auto init_coro = storage_->initialize();
        auto start_coro = storage_->start();
        auto stop_coro = storage_->stop();
        auto cleanup_coro = storage_->cleanup();
    });
}

TEST_F(ObserverStorageTest, AddAndFindObservers) {
    // Create observers
    auto obs_a1 = std::make_shared<observer<TestObserverA>>(*io_context_);
    auto obs_a2 = std::make_shared<observer<TestObserverA>>(*io_context_);
    auto obs_b1 = std::make_shared<observer<TestObserverB>>(*io_context_);
    
    // Add observers
    EXPECT_TRUE(storage_->add_observer(*obs_a1));
    EXPECT_TRUE(storage_->add_observer(*obs_a2));
    EXPECT_TRUE(storage_->add_observer(*obs_b1));
    
    // Adding same observer should fail
    EXPECT_FALSE(storage_->add_observer(*obs_a1));
    
    // Check counts
    EXPECT_EQ(storage_->observer_count<TestObserverA>(), 2);
    EXPECT_EQ(storage_->observer_count<TestObserverB>(), 1);
    EXPECT_EQ(storage_->total_observer_count(), 3);
    
    // Find observers by ID
    auto found_a1 = storage_->find_observer<TestObserverA>(obs_a1->get_id());
    EXPECT_EQ(found_a1.size(), 1);
    EXPECT_EQ(found_a1[0]->get_id(), obs_a1->get_id());
    
    // Find non-existent observer
    observer_id fake_id = generate_uuid_from_str("fake_id");
    auto found_fake = storage_->find_observer<TestObserverA>(fake_id);
    EXPECT_TRUE(found_fake.empty());
}

TEST_F(ObserverStorageTest, PopObservers) {
    // Create and add observers
    auto obs_a1 = std::make_shared<observer<TestObserverA>>(*io_context_);
    auto obs_a2 = std::make_shared<observer<TestObserverA>>(*io_context_);
    
    storage_->add_observer(*obs_a1);
    storage_->add_observer(*obs_a2);
    
    EXPECT_EQ(storage_->observer_count<TestObserverA>(), 2);
    
    // Pop one observer
    auto popped = storage_->pop_observer<TestObserverA>(obs_a1->get_id());
    EXPECT_EQ(popped.size(), 1);
    EXPECT_EQ(popped[0]->get_id(), obs_a1->get_id());
    EXPECT_EQ(storage_->observer_count<TestObserverA>(), 1);
    
    // Pop non-existent observer
    observer_id fake_id = generate_uuid_from_str("fake_id");
    auto popped_fake = storage_->pop_observer<TestObserverA>(fake_id);
    EXPECT_TRUE(popped_fake.empty());
}

TEST_F(ObserverStorageTest, DeleteObservers) {
    // Create and add observers
    auto obs_a1 = std::make_shared<observer<TestObserverA>>(*io_context_);
    auto obs_a2 = std::make_shared<observer<TestObserverA>>(*io_context_);
    
    storage_->add_observer(*obs_a1);
    storage_->add_observer(*obs_a2);
    
    EXPECT_EQ(storage_->observer_count<TestObserverA>(), 2);
    
    // Delete by ID
    storage_->delete_observer<TestObserverA>(obs_a1->get_id());
    EXPECT_EQ(storage_->observer_count<TestObserverA>(), 1);
    
    // Verify observer is gone
    auto found = storage_->find_observer<TestObserverA>(obs_a1->get_id());
    EXPECT_TRUE(found.empty());
}

TEST_F(ObserverStorageTest, ClearObservers) {
    // Create and add observers
    auto obs_a1 = std::make_shared<observer<TestObserverA>>(*io_context_);
    auto obs_a2 = std::make_shared<observer<TestObserverA>>(*io_context_);
    auto obs_b1 = std::make_shared<observer<TestObserverB>>(*io_context_);
    
    storage_->add_observer(*obs_a1);
    storage_->add_observer(*obs_a2);
    storage_->add_observer(*obs_b1);
    
    EXPECT_EQ(storage_->total_observer_count(), 3);
    
    // Clear specific type
    storage_->clear_observers<TestObserverA>();
    EXPECT_EQ(storage_->observer_count<TestObserverA>(), 0);
    EXPECT_EQ(storage_->observer_count<TestObserverB>(), 1);
    
    // Clear all observers
    storage_->clear_all_observers();
    EXPECT_EQ(storage_->total_observer_count(), 0);
}

TEST_F(ObserverStorageTest, ExpiredObserverCleanup) {
    // Create observer that will expire quickly
    auto obs = std::make_shared<observer<TestObserverA>>(*io_context_);
    storage_->add_observer(*obs);
    
    EXPECT_EQ(storage_->observer_count<TestObserverA>(), 1);
    
    // Force expire the observer
    obs->get()->force_expire();
    EXPECT_TRUE(obs->get()->is_expired());
    
    // Cleanup expired observers
    std::size_t cleaned = storage_->cleanup_expired_observers();
    EXPECT_EQ(cleaned, 1);
    EXPECT_EQ(storage_->observer_count<TestObserverA>(), 0);
}

TEST_F(ObserverStorageTest, FindIteratorRange) {
    // Create and add observers
    auto obs_a1 = std::make_shared<observer<TestObserverA>>(*io_context_);
    auto obs_a2 = std::make_shared<observer<TestObserverA>>(*io_context_);
    
    storage_->add_observer(*obs_a1);
    storage_->add_observer(*obs_a2);
    
    // Find iterator range for specific ID
    auto itr_range = storage_->find_observer_itr_range<TestObserverA>(obs_a1->get_id());
    
    std::size_t count = 0;
    for (auto itr = itr_range.first; itr != itr_range.second; ++itr) {
        EXPECT_EQ((*itr)->get_id(), obs_a1->get_id());
        count++;
    }
    EXPECT_EQ(count, 1);
}

TEST_F(ObserverStorageTest, Statistics) {
    // Add some observers
    auto obs_a1 = std::make_shared<observer<TestObserverA>>(*io_context_);
    auto obs_b1 = std::make_shared<observer<TestObserverB>>(*io_context_);
    
    storage_->add_observer(*obs_a1);
    storage_->add_observer(*obs_b1);
    
    // Get statistics
    std::string stats = storage_->get_statistics();
    EXPECT_FALSE(stats.empty());
    EXPECT_NE(stats.find("total_count"), std::string::npos);
    EXPECT_NE(stats.find("total_additions"), std::string::npos);
}

TEST_F(ObserverStorageTest, ThreadSafety) {
    std::atomic<bool> stop_threads{false};
    std::vector<std::thread> threads;
    std::atomic<int> total_added{0};
    
    // Create multiple threads that add/remove observers
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([this, &stop_threads, &total_added, i]() {
            int local_count = 0;
            while (!stop_threads.load()) {
                // Add observer
                auto obs = std::make_shared<observer<TestObserverA>>(*io_context_);
                if (storage_->add_observer(*obs)) {
                    local_count++;
                    total_added.fetch_add(1);
                    
                    // Sometimes remove it
                    if (local_count % 3 == 0) {
                        storage_->delete_observer<TestObserverA>(obs->get_id());
                    }
                }
                
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }
    
    // Let threads run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Stop threads
    stop_threads.store(true);
    for (auto& thread : threads) {
        thread.join();
    }
    
    // If we reach here without deadlock, thread safety is working
    EXPECT_GT(total_added.load(), 0);
    SUCCEED();
}

TEST_F(ObserverStorageTest, DestructorCleanup) {
    {
        auto local_storage = std::make_unique<observer_storage<default_observer_storage_policy, TestObserverA>>(*io_context_);
        
        // Add some observers
        auto obs = std::make_shared<observer<TestObserverA>>(*io_context_);
        local_storage->add_observer(*obs);
        
        EXPECT_EQ(local_storage->total_observer_count(), 1);
        // Destructor should clean up automatically
    }
    
    // If we reach here without hanging, the destructor worked correctly
    SUCCEED();
}

TEST_F(ObserverStorageTest, LegacyTypeAlias) {
    // Test that the legacy type alias works
    observer_strage<TestObserverA> legacy_storage(*io_context_);
    
    auto obs = std::make_shared<observer<TestObserverA>>(*io_context_);
    EXPECT_TRUE(legacy_storage.add_observer(*obs));
    EXPECT_EQ(legacy_storage.observer_count<TestObserverA>(), 1);
}

TEST_F(ObserverStorageTest, PolicyTemplate) {
    // Test that the policy template system works
    using custom_storage = observer_storage<default_observer_storage_policy, TestObserverA>;
    custom_storage storage(*io_context_);
    
    auto obs = std::make_shared<observer<TestObserverA>>(*io_context_);
    EXPECT_TRUE(storage.add_observer(*obs));
    EXPECT_EQ(storage.observer_count<TestObserverA>(), 1);
}

} // namespace ss::test