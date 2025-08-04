#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <ss_p2p/core/interfaces.hpp>
#include <ss_p2p/core/types.hpp>
#include <string>
#include <memory>

using namespace ss::core;

class InterfacesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Mock implementations for testing

class MockLifecycle : public i_lifecycle {
public:
    MOCK_METHOD(async_void, start, (), (override));
    MOCK_METHOD(async_void, stop, (), (override));
    MOCK_METHOD(bool, is_running, (), (const, noexcept, override));
};

class MockComponent : public i_component {
public:
    MOCK_METHOD(async_void, start, (), (override));
    MOCK_METHOD(async_void, stop, (), (override));
    MOCK_METHOD(bool, is_running, (), (const, noexcept, override));
    MOCK_METHOD(std::string, name, (), (const, noexcept, override));
    MOCK_METHOD(std::string, version, (), (const, noexcept, override));
    MOCK_METHOD(bool, is_healthy, (), (const, noexcept, override));
    MOCK_METHOD(std::string, status, (), (const, override));
    MOCK_METHOD(async_void, initialize, (), (override));
    MOCK_METHOD(async_void, cleanup, (), (override));
};

// Test message type for handler tests
struct TestMessage {
    std::string type_name;
    std::string content;
    
    TestMessage(const std::string& type = "", const std::string& data = "") 
        : type_name(type), content(data) {}
};

class MockMessageHandler : public i_message_handler<TestMessage> {
public:
    MOCK_METHOD(async_void, handle_message, 
               (const TestMessage&, const endpoint&), (override));
    MOCK_METHOD(bool, can_handle, (const TestMessage&), (const, noexcept, override));
    MOCK_METHOD(std::uint32_t, priority, (), (const, noexcept, override));
};

// Test event type for observable tests
struct TestEvent {
    std::string name;
    int value;
    
    TestEvent(const std::string& n = "", int v = 0) : name(n), value(v) {}
};

class MockObservable : public i_observable<TestEvent> {
public:
    MOCK_METHOD(observer_id, add_observer, (observer_func), (override));
    MOCK_METHOD(bool, remove_observer, (observer_id), (override));
    MOCK_METHOD(void, clear_observers, (), (override));
    MOCK_METHOD(std::size_t, observer_count, (), (const, noexcept, override));
    MOCK_METHOD(void, notify_observers, (const TestEvent&), (override));
};

// Test config type for configurable tests
struct TestConfig {
    std::string name;
    int value;
    bool enabled;
    
    TestConfig(const std::string& n = "", int v = 0, bool e = true) 
        : name(n), value(v), enabled(e) {}
    
    bool operator==(const TestConfig& other) const {
        return name == other.name && value == other.value && enabled == other.enabled;
    }
};

class MockConfigurable : public i_configurable<TestConfig> {
public:
    MOCK_METHOD(result<void, std::string>, configure, (const TestConfig&), (override));
    MOCK_METHOD(TestConfig, get_config, (), (const, override));
    MOCK_METHOD(result<void, std::string>, validate_config, (const TestConfig&), (const, override));
};

class MockTimeoutManaged : public i_timeout_managed {
public:
    MOCK_METHOD(void, set_timeout, (std::chrono::milliseconds), (noexcept, override));
    MOCK_METHOD(std::chrono::milliseconds, get_timeout, (), (const, noexcept, override));
    MOCK_METHOD(bool, is_timed_out, (), (const, noexcept, override));
    MOCK_METHOD(async_void, cancel, (), (override));
};

class MockLogger : public i_logger {
public:
    MOCK_METHOD(void, log, (level, const std::string&), (override));
    MOCK_METHOD(bool, is_enabled, (level), (const, noexcept, override));
    MOCK_METHOD(void, set_level, (level), (noexcept, override));
};

// Concrete implementations for functionality testing

class ConcreteSerializable : public i_serializable<ConcreteSerializable> {
private:
    int value_;
    
public:
    explicit ConcreteSerializable(int val = 0) : value_(val) {}
    
    int value() const { return value_; }
    
    std::vector<std::uint8_t> serialize() const override {
        std::vector<std::uint8_t> data(sizeof(int));
        std::memcpy(data.data(), &value_, sizeof(int));
        return data;
    }
    
    static result<ConcreteSerializable, std::string> deserialize(const std::vector<std::uint8_t>& data) {
        if (data.size() != sizeof(int)) {
            return result<ConcreteSerializable, std::string>::err("Invalid data size");
        }
        
        int value;
        std::memcpy(&value, data.data(), sizeof(int));
        return result<ConcreteSerializable, std::string>::ok(ConcreteSerializable(value));
    }
};

class ConcreteObservable : public i_observable<TestEvent> {
private:
    std::unordered_map<observer_id, observer_func> observers_;
    observer_id next_id_;
    
public:
    ConcreteObservable() : next_id_(1) {}
    
    observer_id add_observer(observer_func observer) override {
        observer_id id = next_id_++;
        observers_[id] = std::move(observer);
        return id;
    }
    
    bool remove_observer(observer_id id) override {
        return observers_.erase(id) > 0;
    }
    
    void clear_observers() override {
        observers_.clear();
    }
    
    std::size_t observer_count() const noexcept override {
        return observers_.size();
    }
    
protected:
    void notify_observers(const TestEvent& event) override {
        for (const auto& [id, observer] : observers_) {
            observer(event);
        }
    }
    
public:
    // Public method to trigger notifications for testing
    void trigger_event(const TestEvent& event) {
        notify_observers(event);
    }
};

// Interface tests

TEST_F(InterfacesTest, LifecycleInterface) {
    auto mock = std::make_unique<MockLifecycle>();
    
    // Test interface methods exist and can be called
    EXPECT_CALL(*mock, is_running())
        .WillOnce(::testing::Return(false));
    
    EXPECT_FALSE(mock->is_running());
}

TEST_F(InterfacesTest, ComponentInterface) {
    auto mock = std::make_unique<MockComponent>();
    
    EXPECT_CALL(*mock, name())
        .WillOnce(::testing::Return("test_component"));
    EXPECT_CALL(*mock, is_running())
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mock, is_healthy())
        .WillOnce(::testing::Return(true));
    
    EXPECT_EQ(mock->name(), "test_component");
    EXPECT_TRUE(mock->is_running());
    EXPECT_TRUE(mock->is_healthy());
}

TEST_F(InterfacesTest, SerializableInterface) {
    ConcreteSerializable obj(42);
    
    // Test serialization
    auto data = obj.serialize();
    EXPECT_EQ(data.size(), sizeof(int));
    
    // Test deserialization
    auto result = ConcreteSerializable::deserialize(data);
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().value(), 42);
    
    // Test serialized size
    EXPECT_EQ(obj.serialized_size(), sizeof(int));
    
    // Test error case
    std::vector<std::uint8_t> invalid_data{1, 2};  // Wrong size
    auto error_result = ConcreteSerializable::deserialize(invalid_data);
    EXPECT_TRUE(error_result.is_err());
    EXPECT_EQ(error_result.error(), "Invalid data size");
}

TEST_F(InterfacesTest, MessageHandlerInterface) {
    auto mock = std::make_unique<MockMessageHandler>();
    TestMessage msg("test_type", "test_data");
    endpoint ep("127.0.0.1", 8080);
    
    EXPECT_CALL(*mock, can_handle(::testing::_))
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mock, priority())
        .WillOnce(::testing::Return(10));
    
    EXPECT_TRUE(mock->can_handle(msg));
    EXPECT_EQ(mock->priority(), 10);
}

TEST_F(InterfacesTest, ObservableInterface) {
    ConcreteObservable observable;
    
    // Test adding observers
    bool observer1_called = false;
    bool observer2_called = false;
    TestEvent received_event1("", 0);
    TestEvent received_event2("", 0);
    
    auto id1 = observable.add_observer([&](const TestEvent& event) {
        observer1_called = true;
        received_event1 = event;
    });
    
    auto id2 = observable.add_observer([&](const TestEvent& event) {
        observer2_called = true;
        received_event2 = event;
    });
    
    EXPECT_EQ(observable.observer_count(), 2);
    EXPECT_NE(id1, id2);
    
    // Test notification
    TestEvent test_event("test_event", 42);
    observable.trigger_event(test_event);
    
    EXPECT_TRUE(observer1_called);
    EXPECT_TRUE(observer2_called);
    EXPECT_EQ(received_event1.name, "test_event");
    EXPECT_EQ(received_event1.value, 42);
    EXPECT_EQ(received_event2.name, "test_event");
    EXPECT_EQ(received_event2.value, 42);
    
    // Test removing observer
    EXPECT_TRUE(observable.remove_observer(id1));
    EXPECT_EQ(observable.observer_count(), 1);
    EXPECT_FALSE(observable.remove_observer(id1));  // Already removed
    
    // Test clearing observers
    observable.clear_observers();
    EXPECT_EQ(observable.observer_count(), 0);
}

TEST_F(InterfacesTest, ConfigurableInterface) {
    auto mock = std::make_unique<MockConfigurable>();
    TestConfig config("test", 42, true);
    
    EXPECT_CALL(*mock, validate_config(::testing::_))
        .WillOnce(::testing::Return(result<void, std::string>::ok()));
    EXPECT_CALL(*mock, configure(::testing::_))
        .WillOnce(::testing::Return(result<void, std::string>::ok()));
    EXPECT_CALL(*mock, get_config())
        .WillOnce(::testing::Return(config));
    
    auto validation_result = mock->validate_config(config);
    EXPECT_TRUE(validation_result.is_ok());
    
    auto config_result = mock->configure(config);
    EXPECT_TRUE(config_result.is_ok());
    
    auto retrieved_config = mock->get_config();
    EXPECT_EQ(retrieved_config.name, "test");
    EXPECT_EQ(retrieved_config.value, 42);
    EXPECT_TRUE(retrieved_config.enabled);
}

TEST_F(InterfacesTest, TimeoutManagedInterface) {
    auto mock = std::make_unique<MockTimeoutManaged>();
    
    auto timeout = std::chrono::milliseconds(5000);
    
    EXPECT_CALL(*mock, set_timeout(timeout));
    EXPECT_CALL(*mock, get_timeout())
        .WillOnce(::testing::Return(timeout));
    EXPECT_CALL(*mock, is_timed_out())
        .WillOnce(::testing::Return(false));
    
    mock->set_timeout(timeout);
    EXPECT_EQ(mock->get_timeout(), timeout);
    EXPECT_FALSE(mock->is_timed_out());
}

TEST_F(InterfacesTest, LoggerInterface) {
    auto mock = std::make_unique<MockLogger>();
    
    EXPECT_CALL(*mock, is_enabled(i_logger::level::info))
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mock, log(i_logger::level::info, "test message"));
    EXPECT_CALL(*mock, set_level(i_logger::level::warn));
    
    EXPECT_TRUE(mock->is_enabled(i_logger::level::info));
    mock->log(i_logger::level::info, "test message");
    mock->set_level(i_logger::level::warn);
    
    // Test convenience methods
    EXPECT_CALL(*mock, log(i_logger::level::debug, "debug"));
    EXPECT_CALL(*mock, log(i_logger::level::error, "error"));
    EXPECT_CALL(*mock, log(i_logger::level::warn, "warning"));
    
    mock->debug("debug");
    mock->error("error");
    mock->warn("warning");
}

// Test inheritance hierarchy
TEST_F(InterfacesTest, InheritanceHierarchy) {
    // Component should inherit from lifecycle
    static_assert(std::is_base_of_v<i_lifecycle, i_component>);
    
    auto mock_component = std::make_unique<MockComponent>();
    i_lifecycle* lifecycle_ptr = mock_component.get();
    
    // Should be able to call lifecycle methods through base pointer
    EXPECT_CALL(*mock_component, is_running())
        .WillOnce(::testing::Return(true));
    
    EXPECT_TRUE(lifecycle_ptr->is_running());
}

// Test virtual destructor behavior
TEST_F(InterfacesTest, VirtualDestructors) {
    // Test that derived objects can be properly destroyed through base pointers
    {
        std::unique_ptr<i_lifecycle> ptr = std::make_unique<MockLifecycle>();
        // Destruction should work properly
    }
    
    {
        std::unique_ptr<i_component> ptr = std::make_unique<MockComponent>();
        // Destruction should work properly
    }
    
    // This test mainly ensures the code compiles and doesn't cause undefined behavior
    SUCCEED();
}

// Test template interfaces
TEST_F(InterfacesTest, TemplateInterfaces) {
    // Test that template interfaces can be instantiated with different types
    std::unique_ptr<i_serializable<ConcreteSerializable>> serializable_ptr;
    std::unique_ptr<i_message_handler<TestMessage>> handler_ptr;
    std::unique_ptr<i_observable<TestEvent>> observable_ptr;
    std::unique_ptr<i_configurable<TestConfig>> configurable_ptr;
    
    // These should compile without issues
    static_assert(std::is_abstract_v<i_serializable<ConcreteSerializable>>);
    static_assert(std::is_abstract_v<i_message_handler<TestMessage>>);
    static_assert(std::is_abstract_v<i_observable<TestEvent>>);
    static_assert(std::is_abstract_v<i_configurable<TestConfig>>);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}