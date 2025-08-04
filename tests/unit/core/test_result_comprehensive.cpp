#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <ss_p2p/core/result.hpp>
#include <ss_p2p/core/types.hpp>

#include <string>
#include <memory>
#include <optional>

using namespace ss::core;

namespace ss::core::test {

// Test error types for result testing
enum class test_error {
    none = 0,
    generic_error,
    network_error,
    validation_error,
    timeout_error
};

std::string to_string(test_error error) {
    switch (error) {
        case test_error::none: return "no_error";
        case test_error::generic_error: return "generic_error";
        case test_error::network_error: return "network_error";
        case test_error::validation_error: return "validation_error";
        case test_error::timeout_error: return "timeout_error";
        default: return "unknown_error";
    }
}

class ResultTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
    
    // Helper functions for testing
    result<int, test_error> successful_operation(int value) {
        return result<int, test_error>::ok(value);
    }
    
    result<int, test_error> failing_operation(test_error error) {
        return result<int, test_error>::err(error);
    }
    
    result<std::string, test_error> string_operation(const std::string& str) {
        if (str.empty()) {
            return result<std::string, test_error>::err(test_error::validation_error);
        }
        return result<std::string, test_error>::ok(str);
    }
};

// Basic result construction and status tests
TEST_F(ResultTest, SuccessConstruction) {
    auto res = result<int, test_error>::ok(42);
    
    EXPECT_TRUE(res.is_ok());
    EXPECT_FALSE(res.is_err());
    EXPECT_EQ(res.value(), 42);
    EXPECT_EQ(res.value_or(0), 42);
}

TEST_F(ResultTest, ErrorConstruction) {
    auto res = result<int, test_error>::err(test_error::network_error);
    
    EXPECT_FALSE(res.is_ok());
    EXPECT_TRUE(res.is_err());
    EXPECT_EQ(res.error(), test_error::network_error);
    EXPECT_EQ(res.value_or(100), 100);
}

TEST_F(ResultTest, MoveConstruction) {
    auto original = result<std::string, test_error>::ok(std::string("test"));
    auto moved = std::move(original);
    
    EXPECT_TRUE(moved.is_ok());
    EXPECT_EQ(moved.value(), "test");
}

TEST_F(ResultTest, CopyConstruction) {
    auto original = result<int, test_error>::ok(123);
    auto copied = original;
    
    EXPECT_TRUE(copied.is_ok());
    EXPECT_EQ(copied.value(), 123);
    EXPECT_TRUE(original.is_ok());
    EXPECT_EQ(original.value(), 123);
}

// Value access and error handling tests
TEST_F(ResultTest, ValueAccess) {
    auto res = successful_operation(42);
    
    EXPECT_EQ(res.value(), 42);
    EXPECT_EQ(*res, 42);
    EXPECT_EQ(res.operator->(), &res.value());
}

TEST_F(ResultTest, ErrorAccess) {
    auto res = failing_operation(test_error::timeout_error);
    
    EXPECT_EQ(res.error(), test_error::timeout_error);
    EXPECT_THROW(res.value(), std::runtime_error);
}

TEST_F(ResultTest, ValueOrDefault) {
    auto success = successful_operation(42);
    auto failure = failing_operation(test_error::generic_error);
    
    EXPECT_EQ(success.value_or(100), 42);
    EXPECT_EQ(failure.value_or(100), 100);
}

// Monadic operations tests
TEST_F(ResultTest, MapOperation) {
    auto res = successful_operation(5);
    
    auto mapped = res.map([](int x) { return x * 2; });
    EXPECT_TRUE(mapped.is_ok());
    EXPECT_EQ(mapped.value(), 10);
    
    auto error_res = failing_operation(test_error::network_error);
    auto mapped_error = error_res.map([](int x) { return x * 2; });
    EXPECT_TRUE(mapped_error.is_err());
    EXPECT_EQ(mapped_error.error(), test_error::network_error);
}

TEST_F(ResultTest, MapErrorOperation) {
    auto res = failing_operation(test_error::network_error);
    
    auto mapped = res.map_error([](test_error e) {
        return e == test_error::network_error ? test_error::timeout_error : e;
    });
    
    EXPECT_TRUE(mapped.is_err());
    EXPECT_EQ(mapped.error(), test_error::timeout_error);
    
    auto success_res = successful_operation(42);
    auto mapped_success = success_res.map_error([](test_error) { return test_error::generic_error; });
    EXPECT_TRUE(mapped_success.is_ok());
    EXPECT_EQ(mapped_success.value(), 42);
}

TEST_F(ResultTest, AndThenOperation) {
    auto res = successful_operation(5);
    
    auto chained = res.and_then([this](int x) {
        return x > 0 ? successful_operation(x * 2) : failing_operation(test_error::validation_error);
    });
    
    EXPECT_TRUE(chained.is_ok());
    EXPECT_EQ(chained.value(), 10);
    
    auto error_res = failing_operation(test_error::network_error);
    auto chained_error = error_res.and_then([this](int x) {
        return successful_operation(x * 2);
    });
    
    EXPECT_TRUE(chained_error.is_err());
    EXPECT_EQ(chained_error.error(), test_error::network_error);
}

TEST_F(ResultTest, OrElseOperation) {
    auto error_res = failing_operation(test_error::network_error);
    
    auto recovered = error_res.or_else([this](test_error e) {
        return e == test_error::network_error ? successful_operation(999) : failing_operation(e);
    });
    
    EXPECT_TRUE(recovered.is_ok());
    EXPECT_EQ(recovered.value(), 999);
    
    auto success_res = successful_operation(42);
    auto no_recovery = success_res.or_else([this](test_error) {
        return successful_operation(0);
    });
    
    EXPECT_TRUE(no_recovery.is_ok());
    EXPECT_EQ(no_recovery.value(), 42);
}

// Complex chaining tests
TEST_F(ResultTest, ComplexChaining) {
    auto res = successful_operation(10)
        .and_then([this](int x) { 
            return x > 5 ? successful_operation(x * 2) : failing_operation(test_error::validation_error);
        })
        .map([](int x) { return x + 100; })
        .and_then([this](int x) {
            return x > 100 ? successful_operation(x) : failing_operation(test_error::generic_error);
        });
    
    EXPECT_TRUE(res.is_ok());
    EXPECT_EQ(res.value(), 120);
}

TEST_F(ResultTest, ChainWithEarlyFailure) {
    auto res = failing_operation(test_error::network_error)
        .map([](int x) { return x * 2; })
        .and_then([this](int x) { return successful_operation(x + 100); })
        .or_else([this](test_error) { return successful_operation(999); });
    
    EXPECT_TRUE(res.is_ok());
    EXPECT_EQ(res.value(), 999);
}

// Type conversion and forwarding tests
TEST_F(ResultTest, TypeConversions) {
    auto int_result = successful_operation(42);
    
    // Test conversion to different types
    auto string_result = int_result.map([](int x) { return std::to_string(x); });
    EXPECT_TRUE(string_result.is_ok());
    EXPECT_EQ(string_result.value(), "42");
    
    auto float_result = int_result.map([](int x) { return static_cast<float>(x) / 2.0f; });
    EXPECT_TRUE(float_result.is_ok());
    EXPECT_FLOAT_EQ(float_result.value(), 21.0f);
}

TEST_F(ResultTest, MoveSemantics) {
    auto movable_resource = std::make_unique<int>(42);
    auto ptr_value = movable_resource.get();
    auto res = result<std::unique_ptr<int>, test_error>::ok(std::move(movable_resource));
    
    EXPECT_TRUE(res.is_ok());
    EXPECT_EQ(res.value().get(), ptr_value);
    EXPECT_EQ(*res.value(), 42);
    EXPECT_EQ(movable_resource.get(), nullptr); // Should be moved
}

// Optional integration tests
TEST_F(ResultTest, OptionalIntegration) {
    auto res = successful_operation(42);
    
    std::optional<int> opt = res.is_ok() ? std::optional<int>(res.value()) : std::nullopt;
    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), 42);
    
    auto error_res = failing_operation(test_error::network_error);
    std::optional<int> error_opt = error_res.is_ok() ? std::optional<int>(error_res.value()) : std::nullopt;
    EXPECT_FALSE(error_opt.has_value());
}

// Edge cases and corner cases
TEST_F(ResultTest, VoidResultType) {
    auto void_success = result<void, test_error>::ok();
    auto void_error = result<void, test_error>::err(test_error::generic_error);
    
    EXPECT_TRUE(void_success.is_ok());
    EXPECT_FALSE(void_success.is_err());
    
    EXPECT_FALSE(void_error.is_ok());
    EXPECT_TRUE(void_error.is_err());
    EXPECT_EQ(void_error.error(), test_error::generic_error);
}

TEST_F(ResultTest, ResultOfResult) {
    using nested_result = result<result<int, test_error>, test_error>;
    
    auto inner_success = successful_operation(42);
    auto outer_success = nested_result::ok(inner_success);
    
    EXPECT_TRUE(outer_success.is_ok());
    EXPECT_TRUE(outer_success.value().is_ok());
    EXPECT_EQ(outer_success.value().value(), 42);
    
    auto outer_error = nested_result::err(test_error::network_error);
    EXPECT_TRUE(outer_error.is_err());
    EXPECT_EQ(outer_error.error(), test_error::network_error);
}

TEST_F(ResultTest, LargeValueTypes) {
    struct LargeStruct {
        std::array<int, 1000> data;
        LargeStruct() { data.fill(42); }
        bool operator==(const LargeStruct& other) const {
            return data == other.data;
        }
    };
    
    LargeStruct large_value;
    auto res = result<LargeStruct, test_error>::ok(std::move(large_value));
    
    EXPECT_TRUE(res.is_ok());
    EXPECT_EQ(res.value().data[0], 42);
    EXPECT_EQ(res.value().data[999], 42);
}

// Error handling and debugging tests
TEST_F(ResultTest, ErrorMessageHandling) {
    auto error_res = failing_operation(test_error::validation_error);
    
    // Test error conversion to string (if implemented)
    std::string error_str = to_string(error_res.error());
    EXPECT_EQ(error_str, "validation_error");
}

TEST_F(ResultTest, ExceptionSafety) {
    struct ThrowingType {
        ThrowingType(bool should_throw = false) {
            if (should_throw) throw std::runtime_error("Construction failed");
        }
        ThrowingType(const ThrowingType&) = default;
        ThrowingType& operator=(const ThrowingType&) = default;
    };
    
    // Should not throw during result construction
    EXPECT_NO_THROW({
        auto res = result<ThrowingType, test_error>::ok(ThrowingType(false));
        EXPECT_TRUE(res.is_ok());
    });
    
    // Error construction should never throw
    EXPECT_NO_THROW({
        auto res = result<ThrowingType, test_error>::err(test_error::generic_error);
        EXPECT_TRUE(res.is_err());
    });
}

// Performance and efficiency tests
TEST_F(ResultTest, NoUnnecessaryCopies) {
    struct CopyTracker {
        static int copy_count;
        static int move_count;
        
        CopyTracker() = default;
        CopyTracker(const CopyTracker&) { ++copy_count; }
        CopyTracker(CopyTracker&&) noexcept { ++move_count; }
        CopyTracker& operator=(const CopyTracker&) { ++copy_count; return *this; }
        CopyTracker& operator=(CopyTracker&&) noexcept { ++move_count; return *this; }
    };
    
    CopyTracker::copy_count = 0;
    CopyTracker::move_count = 0;
    
    auto res = result<CopyTracker, test_error>::ok(CopyTracker{});
    
    // Should prefer moves over copies
    EXPECT_GE(CopyTracker::move_count, CopyTracker::copy_count);
}

int ResultTest::CopyTracker::copy_count = 0;
int ResultTest::CopyTracker::move_count = 0;

// Comparison operators
TEST_F(ResultTest, ComparisonOperators) {
    auto success1 = successful_operation(42);
    auto success2 = successful_operation(42);
    auto success3 = successful_operation(43);
    auto error1 = failing_operation(test_error::network_error);
    auto error2 = failing_operation(test_error::network_error);
    auto error3 = failing_operation(test_error::validation_error);
    
    // Test equality
    EXPECT_EQ(success1, success2);
    EXPECT_NE(success1, success3);
    EXPECT_EQ(error1, error2);
    EXPECT_NE(error1, error3);
    EXPECT_NE(success1, error1);
}

} // namespace ss::core::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}