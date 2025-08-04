#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <ss_p2p/observer.hpp>
#include <ss_p2p/core/types.hpp>
#include <ss_p2p/message.hpp>

#include <chrono>
#include <thread>
#include <atomic>
#include <boost/asio.hpp>

namespace ss::test {

// Mock observer body for testing
class MockObserverBody : public base_observer {
public:
    MockObserverBody(ss::core::io_context& io_context)
        : base_observer(io_context, "mock_observer") {}
    
    MOCK_METHOD(int, income_message, (message& msg, boost::asio::ip::udp::endpoint& endpoint), (override));
    MOCK_METHOD(void, on_send_done, (const boost::system::error_code& ec), (override));
    MOCK_METHOD(void, print, (), (const, override));
};

class ObserverAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        io_context_ = std::make_unique<ss::core::io_context>();
    }

    void TearDown() override {
        if (io_context_) {
            io_context_->stop();
        }
    }

    std::unique_ptr<ss::core::io_context> io_context_;
};

TEST_F(ObserverAdapterTest, UtilityFunctions) {
    // Test UUID generation from string
    auto uuid1 = generate_uuid_from_str("test_string");
    auto uuid2 = generate_uuid_from_str("test_string");
    EXPECT_EQ(uuid1, uuid2); // Same input should produce same UUID
    
    auto uuid3 = generate_uuid_from_str("different_string");
    EXPECT_NE(uuid1, uuid3); // Different input should produce different UUID
    
    // Test string conversion
    std::string uuid_str = observer_id_to_str(uuid1);
    EXPECT_FALSE(uuid_str.empty());
    
    auto uuid_from_str = str_to_observer_id(uuid_str);
    EXPECT_EQ(uuid1, uuid_from_str);
    
    // Test invalid string conversion
    auto invalid_uuid = str_to_observer_id("invalid_uuid_string");
    EXPECT_TRUE(invalid_uuid.is_nil());
}

TEST_F(ObserverAdapterTest, BaseObserverConstruction) {
    EXPECT_NO_THROW({
        base_observer observer(*io_context_, "test_observer");
    });
    
    // Test with specific ID
    auto specific_id = generate_uuid_from_str("specific_id");
    base_observer observer(*io_context_, "test_observer", specific_id);
    EXPECT_EQ(observer.get_id(), specific_id);
}

TEST_F(ObserverAdapterTest, BaseObserverComponentInterface) {
    base_observer observer(*io_context_, "test_observer");
    
    // Test component interface
    EXPECT_EQ(observer.name(), "base_observer");
    EXPECT_EQ(observer.version(), "2.0.0");
    EXPECT_FALSE(observer.is_running());
    EXPECT_FALSE(observer.is_healthy()); // Not running, so not healthy
    
    // Status should be valid JSON-like string
    std::string status = observer.status();
    EXPECT_FALSE(status.empty());
    EXPECT_NE(status.find("id"), std::string::npos);
    EXPECT_NE(status.find("type"), std::string::npos);
}

TEST_F(ObserverAdapterTest, BaseObserverTimeout) {
    base_observer observer(*io_context_, "test_observer");
    
    // Test timeout management
    auto initial_timeout = observer.get_timeout();
    EXPECT_GT(initial_timeout.count(), 0);
    
    observer.set_timeout(std::chrono::milliseconds(5000));
    EXPECT_EQ(observer.get_timeout(), std::chrono::milliseconds(5000));
    
    // Initially should not be timed out
    EXPECT_FALSE(observer.is_timed_out());
    
    // Test force expire
    observer.force_expire();
    EXPECT_TRUE(observer.is_expired());
    EXPECT_TRUE(observer.is_timed_out());
}

TEST_F(ObserverAdapterTest, BaseObserverExpiration) {
    base_observer observer(*io_context_, "test_observer");
    
    // Initially should not be expired
    EXPECT_FALSE(observer.is_expired());
    
    // Should have time left
    auto time_left = observer.get_expire_time_left();
    EXPECT_GT(time_left.count(), 0);
    
    // Test extend expire time
    observer.extend_expire_time(std::chrono::seconds(30));
    auto extended_time_left = observer.get_expire_time_left();
    EXPECT_GE(extended_time_left.count(), 25); // Should be close to 30
    
    // Test force expire
    observer.force_expire();
    EXPECT_TRUE(observer.is_expired());
    EXPECT_EQ(observer.get_expire_time_left().count(), 0);
}

TEST_F(ObserverAdapterTest, BaseObserverObservableInterface) {
    base_observer observer(*io_context_, "test_observer");
    
    std::atomic<int> event_count{0};
    observer_event last_event;
    
    // Add observer
    auto observer_id = observer.add_observer([&event_count, &last_event](const observer_event& event) {
        event_count.fetch_add(1);
        last_event = event;
    });
    
    EXPECT_EQ(observer.observer_count(), 1);
    
    // Trigger an event
    observer.force_expire();
    
    // Give time for event processing
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    EXPECT_GT(event_count.load(), 0);
    EXPECT_EQ(last_event.event_type, observer_event::type::expired);
    
    // Remove observer
    observer.remove_observer(observer_id);
    EXPECT_EQ(observer.observer_count(), 0);
    
    // Clear observers
    observer.add_observer([](const observer_event& event) {});
    EXPECT_EQ(observer.observer_count(), 1);
    observer.clear_observers();
    EXPECT_EQ(observer.observer_count(), 0);
}

TEST_F(ObserverAdapterTest, BaseObserverLifecycle) {
    base_observer observer(*io_context_, "test_observer");
    
    // Test lifecycle operations
    EXPECT_FALSE(observer.is_running());
    
    // Note: These are async operations, so we can't easily test completion
    // In a real implementation, we would properly await these
    EXPECT_NO_THROW({
        auto init_coro = observer.initialize();
        auto start_coro = observer.start();
        auto stop_coro = observer.stop();
        auto cleanup_coro = observer.cleanup();
    });
}

TEST_F(ObserverAdapterTest, BaseObserverMessageHandling) {
    base_observer observer(*io_context_, "test_observer");
    
    std::atomic<int> event_count{0};
    observer_event last_event;
    
    // Add observer to monitor events
    observer.add_observer([&event_count, &last_event](const observer_event& event) {
        event_count.fetch_add(1);
        last_event = event;
    });
    
    // Test message handling
    message msg("test_app");
    boost::asio::ip::udp::endpoint endpoint(
        boost::asio::ip::address::from_string("127.0.0.1"), 12345);
    
    int result = observer.income_message(msg, endpoint);
    EXPECT_EQ(result, 0); // Success
    
    // Should have triggered an event
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_GT(event_count.load(), 0);
    EXPECT_EQ(last_event.event_type, observer_event::type::message_received);
    EXPECT_EQ(last_event.endpoint, endpoint);
    
    // Test send completion
    event_count.store(0);
    boost::system::error_code ec;
    observer.on_send_done(ec);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_GT(event_count.load(), 0);
    EXPECT_EQ(last_event.event_type, observer_event::type::send_completed);
}

TEST_F(ObserverAdapterTest, TemplateObserverConstruction) {
    auto mock_body = std::make_shared<MockObserverBody>(*io_context_);
    
    EXPECT_NO_THROW({
        observer<MockObserverBody> obs(mock_body);
    });
    
    EXPECT_NO_THROW({
        MockObserverBody body(*io_context_);
        observer<MockObserverBody> obs(std::move(body));
    });
    
    EXPECT_NO_THROW({
        observer<MockObserverBody> obs(*io_context_);
    });
}

TEST_F(ObserverAdapterTest, TemplateObserverBasicOperations) {
    auto mock_body = std::make_shared<MockObserverBody>(*io_context_);
    observer<MockObserverBody> obs(mock_body);
    
    // Test basic operations
    EXPECT_EQ(obs.get(), mock_body);
    EXPECT_FALSE(obs.get_id().is_nil());
    EXPECT_EQ(obs.get_type_name(), "mock_observer");
    
    // Test ref creation
    auto ref = obs.get_ref();
    EXPECT_TRUE(ref != nullptr);
    EXPECT_EQ(ref->get_id(), obs.get_id());
}

TEST_F(ObserverAdapterTest, TemplateObserverDelegation) {
    auto mock_body = std::make_shared<MockObserverBody>(*io_context_);
    observer<MockObserverBody> obs(mock_body);
    
    // Set up expectations
    EXPECT_CALL(*mock_body, income_message(testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(42));
    
    EXPECT_CALL(*mock_body, on_send_done(testing::_))
        .Times(1);
    
    EXPECT_CALL(*mock_body, print())
        .Times(1);
    
    // Test delegation
    message msg("test_app");
    boost::asio::ip::udp::endpoint endpoint;
    
    int result = obs.income_message(msg, endpoint);
    EXPECT_EQ(result, 42);
    
    boost::system::error_code ec;
    obs.on_send_done(ec);
    
    obs.print();
}

TEST_F(ObserverAdapterTest, TemplateObserverEquality) {
    auto mock_body1 = std::make_shared<MockObserverBody>(*io_context_);
    auto mock_body2 = std::make_shared<MockObserverBody>(*io_context_);
    
    observer<MockObserverBody> obs1(mock_body1);
    observer<MockObserverBody> obs2(mock_body1); // Same body
    observer<MockObserverBody> obs3(mock_body2); // Different body
    
    // Observers with same body should be equal (same ID)
    EXPECT_EQ(obs1, obs2);
    EXPECT_FALSE(obs1 != obs2);
    
    // Observers with different bodies should not be equal (different IDs)
    EXPECT_NE(obs1, obs3);
    EXPECT_TRUE(obs1 != obs3);
}

TEST_F(ObserverAdapterTest, TemplateObserverHash) {
    auto mock_body = std::make_shared<MockObserverBody>(*io_context_);
    observer<MockObserverBody> obs(mock_body);
    auto ref = obs.get_ref();
    
    typename observer<MockObserverBody>::Hash hasher;
    
    // Test different hash operations
    auto hash1 = hasher(obs);
    auto hash2 = hasher(ref);
    auto hash3 = hasher(obs.get_id());
    
    EXPECT_EQ(hash1, hash2);
    EXPECT_EQ(hash1, hash3);
}

TEST_F(ObserverAdapterTest, TemplateObserverEqual) {
    auto mock_body1 = std::make_shared<MockObserverBody>(*io_context_);
    auto mock_body2 = std::make_shared<MockObserverBody>(*io_context_);
    
    observer<MockObserverBody> obs1(mock_body1);
    observer<MockObserverBody> obs2(mock_body1);
    observer<MockObserverBody> obs3(mock_body2);
    
    auto ref1 = obs1.get_ref();
    auto ref2 = obs2.get_ref();
    auto ref3 = obs3.get_ref();
    
    typename observer<MockObserverBody>::Equal equal_func;
    
    // Test observer equality
    EXPECT_TRUE(equal_func(obs1, obs2));
    EXPECT_FALSE(equal_func(obs1, obs3));
    
    // Test reference equality
    EXPECT_TRUE(equal_func(ref1, ref2));
    EXPECT_FALSE(equal_func(ref1, ref3));
    
    // Test ID equality
    EXPECT_TRUE(equal_func(obs1.get_id(), obs2.get_id()));
    EXPECT_FALSE(equal_func(obs1.get_id(), obs3.get_id()));
}

TEST_F(ObserverAdapterTest, ObserverWithNullBody) {
    observer<MockObserverBody> obs(std::shared_ptr<MockObserverBody>{});
    
    // Operations on null body should return default values or handle gracefully
    EXPECT_EQ(obs.income_message(message("test"), boost::asio::ip::udp::endpoint{}), -1);
    EXPECT_TRUE(obs.is_expired());
    EXPECT_TRUE(obs.get_id().is_nil());
    EXPECT_EQ(obs.get_type_name(), "unknown");
    EXPECT_EQ(obs.get_expire_time_left().count(), 0);
    
    // These should not crash
    EXPECT_NO_THROW({
        obs.on_send_done(boost::system::error_code{});
        obs.print();
    });
}

} // namespace ss::test