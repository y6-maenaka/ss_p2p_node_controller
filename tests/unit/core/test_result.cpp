#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <ss_p2p/core/result.hpp>
#include <string>
#include <stdexcept>

using namespace ss::core;

class ResultTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Basic result functionality tests
TEST_F(ResultTest, SuccessConstruction) {
    result<int, std::string> r(42);
    
    EXPECT_TRUE(r.is_ok());
    EXPECT_FALSE(r.is_err());
    EXPECT_TRUE(static_cast<bool>(r));
    EXPECT_EQ(r.value(), 42);
}

TEST_F(ResultTest, ErrorConstruction) {
    result<int, std::string> r("error message");
    
    EXPECT_FALSE(r.is_ok());
    EXPECT_TRUE(r.is_err());
    EXPECT_FALSE(static_cast<bool>(r));
    EXPECT_EQ(r.error(), "error message");
}

TEST_F(ResultTest, CopyAndMove) {
    result<int, std::string> original(42);
    
    // Copy construction
    result<int, std::string> copied(original);
    EXPECT_TRUE(copied.is_ok());
    EXPECT_EQ(copied.value(), 42);
    
    // Move construction
    result<int, std::string> moved(std::move(copied));
    EXPECT_TRUE(moved.is_ok());
    EXPECT_EQ(moved.value(), 42);
    
    // Copy assignment
    result<int, std::string> copy_assigned("temp");
    copy_assigned = original;
    EXPECT_TRUE(copy_assigned.is_ok());
    EXPECT_EQ(copy_assigned.value(), 42);
    
    // Move assignment
    result<int, std::string> move_assigned("temp");
    move_assigned = std::move(copy_assigned);
    EXPECT_TRUE(move_assigned.is_ok());
    EXPECT_EQ(move_assigned.value(), 42);
}

TEST_F(ResultTest, ValueAccess) {
    result<int, std::string> success(42);
    result<int, std::string> error("failed");
    
    // Success case
    EXPECT_EQ(success.value(), 42);
    EXPECT_EQ(std::move(success).value(), 42);
    
    // Error case should throw
    EXPECT_THROW(error.value(), std::bad_variant_access);
    EXPECT_THROW(std::move(error).value(), std::bad_variant_access);
}

TEST_F(ResultTest, ErrorAccess) {
    result<int, std::string> success(42);
    result<int, std::string> error("failed");
    
    // Error case
    EXPECT_EQ(error.error(), "failed");
    EXPECT_EQ(std::move(error).error(), "failed");
    
    // Success case should throw
    EXPECT_THROW(success.error(), std::bad_variant_access);
    EXPECT_THROW(std::move(success).error(), std::bad_variant_access);
}

TEST_F(ResultTest, ValueOr) {
    result<int, std::string> success(42);
    result<int, std::string> error("failed");
    
    EXPECT_EQ(success.value_or(100), 42);
    EXPECT_EQ(error.value_or(100), 100);
    
    // Test with move
    EXPECT_EQ(std::move(success).value_or(100), 42);
    EXPECT_EQ(std::move(error).value_or(100), 100);
}

TEST_F(ResultTest, Equality) {
    result<int, std::string> success1(42);
    result<int, std::string> success2(42);
    result<int, std::string> success3(43);
    result<int, std::string> error1("failed");
    result<int, std::string> error2("failed");
    result<int, std::string> error3("different");
    
    EXPECT_EQ(success1, success2);
    EXPECT_NE(success1, success3);
    EXPECT_NE(success1, error1);
    EXPECT_EQ(error1, error2);
    EXPECT_NE(error1, error3);
}

// Map operation tests
TEST_F(ResultTest, MapSuccess) {
    result<int, std::string> r(42);
    
    auto mapped = r.map([](int x) { return x * 2; });
    static_assert(std::is_same_v<decltype(mapped), result<int, std::string>>);
    
    EXPECT_TRUE(mapped.is_ok());
    EXPECT_EQ(mapped.value(), 84);
}

TEST_F(ResultTest, MapError) {
    result<int, std::string> r("error");
    
    auto mapped = r.map([](int x) { return x * 2; });
    
    EXPECT_TRUE(mapped.is_err());
    EXPECT_EQ(mapped.error(), "error");
}

TEST_F(ResultTest, MapTypeChange) {
    result<int, std::string> r(42);
    
    auto mapped = r.map([](int x) { return std::to_string(x); });
    static_assert(std::is_same_v<decltype(mapped), result<std::string, std::string>>);
    
    EXPECT_TRUE(mapped.is_ok());
    EXPECT_EQ(mapped.value(), "42");
}

TEST_F(ResultTest, MapVoidReturn) {
    result<int, std::string> r(42);
    bool called = false;
    
    auto mapped = r.map([&called](int x) { called = true; });
    static_assert(std::is_same_v<decltype(mapped), result<void, std::string>>);
    
    EXPECT_TRUE(mapped.is_ok());
    EXPECT_TRUE(called);
}

TEST_F(ResultTest, MapMove) {
    result<std::string, std::string> r("hello");
    
    auto mapped = std::move(r).map([](std::string&& s) { 
        return s + " world"; 
    });
    
    EXPECT_TRUE(mapped.is_ok());
    EXPECT_EQ(mapped.value(), "hello world");
}

// Map error tests
TEST_F(ResultTest, MapErrSuccess) {
    result<int, std::string> r(42);
    
    auto mapped = r.map_err([](const std::string& err) { 
        return "transformed: " + err; 
    });
    static_assert(std::is_same_v<decltype(mapped), result<int, std::string>>);
    
    EXPECT_TRUE(mapped.is_ok());
    EXPECT_EQ(mapped.value(), 42);
}

TEST_F(ResultTest, MapErrError) {
    result<int, std::string> r("error");
    
    auto mapped = r.map_err([](const std::string& err) { 
        return "transformed: " + err; 
    });
    
    EXPECT_TRUE(mapped.is_err());
    EXPECT_EQ(mapped.error(), "transformed: error");
}

TEST_F(ResultTest, MapErrTypeChange) {
    result<int, std::string> r("error");
    
    auto mapped = r.map_err([](const std::string& err) { 
        return static_cast<int>(err.length()); 
    });
    static_assert(std::is_same_v<decltype(mapped), result<int, int>>);
    
    EXPECT_TRUE(mapped.is_err());
    EXPECT_EQ(mapped.error(), 5);  // "error".length()
}

// and_then tests
TEST_F(ResultTest, AndThenSuccess) {
    result<int, std::string> r(42);
    
    auto chained = r.and_then([](int x) -> result<std::string, std::string> {
        if (x > 0) {
            return result<std::string, std::string>::ok(std::to_string(x));
        }
        return result<std::string, std::string>::err("negative");
    });
    
    EXPECT_TRUE(chained.is_ok());
    EXPECT_EQ(chained.value(), "42");
}

TEST_F(ResultTest, AndThenSuccessToError) {
    result<int, std::string> r(-1);
    
    auto chained = r.and_then([](int x) -> result<std::string, std::string> {
        if (x > 0) {
            return result<std::string, std::string>::ok(std::to_string(x));
        }
        return result<std::string, std::string>::err("negative");
    });
    
    EXPECT_TRUE(chained.is_err());
    EXPECT_EQ(chained.error(), "negative");
}

TEST_F(ResultTest, AndThenError) {
    result<int, std::string> r("initial error");
    
    auto chained = r.and_then([](int x) -> result<std::string, std::string> {
        return result<std::string, std::string>::ok(std::to_string(x));
    });
    
    EXPECT_TRUE(chained.is_err());
    EXPECT_EQ(chained.error(), "initial error");
}

TEST_F(ResultTest, AndThenChaining) {
    result<int, std::string> r(10);
    
    auto result_chain = r
        .and_then([](int x) -> result<int, std::string> {
            return result<int, std::string>::ok(x * 2);
        })
        .and_then([](int x) -> result<std::string, std::string> {
            return result<std::string, std::string>::ok(std::to_string(x));
        });
    
    EXPECT_TRUE(result_chain.is_ok());
    EXPECT_EQ(result_chain.value(), "20");
}

// or_else tests
TEST_F(ResultTest, OrElseSuccess) {
    result<int, std::string> r(42);
    
    auto alternative = r.or_else([](const std::string& err) -> result<int, std::string> {
        return result<int, std::string>::ok(0);
    });
    
    EXPECT_TRUE(alternative.is_ok());
    EXPECT_EQ(alternative.value(), 42);
}

TEST_F(ResultTest, OrElseError) {
    result<int, std::string> r("error");
    
    auto alternative = r.or_else([](const std::string& err) -> result<int, std::string> {
        if (err == "error") {
            return result<int, std::string>::ok(100);
        }
        return result<int, std::string>::err("unhandled");
    });
    
    EXPECT_TRUE(alternative.is_ok());
    EXPECT_EQ(alternative.value(), 100);
}

TEST_F(ResultTest, OrElseErrorToError) {
    result<int, std::string> r("error");
    
    auto alternative = r.or_else([](const std::string& err) -> result<int, std::string> {
        return result<int, std::string>::err("transformed: " + err);
    });
    
    EXPECT_TRUE(alternative.is_err());
    EXPECT_EQ(alternative.error(), "transformed: error");
}

// Factory function tests
TEST_F(ResultTest, FactoryFunctions) {
    auto success = result<int, std::string>::ok(42);
    auto error = result<int, std::string>::err("failed");
    
    EXPECT_TRUE(success.is_ok());
    EXPECT_EQ(success.value(), 42);
    
    EXPECT_TRUE(error.is_err());
    EXPECT_EQ(error.error(), "failed");
}

// Void specialization tests
TEST_F(ResultTest, VoidSpecializationSuccess) {
    result<void, std::string> r;
    
    EXPECT_TRUE(r.is_ok());
    EXPECT_FALSE(r.is_err());
    EXPECT_TRUE(static_cast<bool>(r));
}

TEST_F(ResultTest, VoidSpecializationError) {
    result<void, std::string> r("error");
    
    EXPECT_FALSE(r.is_ok());
    EXPECT_TRUE(r.is_err());
    EXPECT_FALSE(static_cast<bool>(r));
    EXPECT_EQ(r.error(), "error");
}

TEST_F(ResultTest, VoidSpecializationMap) {
    result<void, std::string> r;
    bool called = false;
    
    auto mapped = r.map([&called]() { 
        called = true;
        return 42;
    });
    static_assert(std::is_same_v<decltype(mapped), result<int, std::string>>);
    
    EXPECT_TRUE(mapped.is_ok());
    EXPECT_EQ(mapped.value(), 42);
    EXPECT_TRUE(called);
}

TEST_F(ResultTest, VoidSpecializationMapVoid) {
    result<void, std::string> r;
    bool called = false;
    
    auto mapped = r.map([&called]() { 
        called = true;
    });
    static_assert(std::is_same_v<decltype(mapped), result<void, std::string>>);
    
    EXPECT_TRUE(mapped.is_ok());
    EXPECT_TRUE(called);
}

TEST_F(ResultTest, VoidSpecializationAndThen) {
    result<void, std::string> r;
    
    auto chained = r.and_then([]() -> result<int, std::string> {
        return result<int, std::string>::ok(42);
    });
    
    EXPECT_TRUE(chained.is_ok());
    EXPECT_EQ(chained.value(), 42);
}

TEST_F(ResultTest, VoidSpecializationEquality) {
    result<void, std::string> success1;
    result<void, std::string> success2;
    result<void, std::string> error1("error");
    result<void, std::string> error2("error");
    result<void, std::string> error3("different");
    
    EXPECT_EQ(success1, success2);
    EXPECT_NE(success1, error1);
    EXPECT_EQ(error1, error2);
    EXPECT_NE(error1, error3);
}

TEST_F(ResultTest, VoidSpecializationFactoryFunctions) {
    auto success = result<void, std::string>::ok();
    auto error = result<void, std::string>::err("failed");
    
    EXPECT_TRUE(success.is_ok());
    EXPECT_TRUE(error.is_err());
    EXPECT_EQ(error.error(), "failed");
}

// Complex chaining test
TEST_F(ResultTest, ComplexChaining) {
    auto process = [](int x) -> result<std::string, std::string> {
        return result<int, std::string>::ok(x)
            .map([](int val) { return val * 2; })
            .and_then([](int val) -> result<int, std::string> {
                if (val > 10) {
                    return result<int, std::string>::ok(val);
                }
                return result<int, std::string>::err("too small");
            })
            .map([](int val) { return std::to_string(val); })
            .or_else([](const std::string& err) -> result<std::string, std::string> {
                return result<std::string, std::string>::ok("default");
            });
    };
    
    auto result1 = process(10);  // 10 * 2 = 20 > 10, should succeed
    EXPECT_TRUE(result1.is_ok());
    EXPECT_EQ(result1.value(), "20");
    
    auto result2 = process(3);   // 3 * 2 = 6 <= 10, should use default
    EXPECT_TRUE(result2.is_ok());
    EXPECT_EQ(result2.value(), "default");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}