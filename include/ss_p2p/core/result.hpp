#pragma once

#include <type_traits>
#include <utility>
#include <functional>
#include <optional>
#include <variant>
#include <stdexcept>

namespace ss::core {

/**
 * @brief A Result type similar to Rust's Result<T, E> or std::expected
 * 
 * Represents either a successful value of type T or an error of type E.
 * Provides monadic operations like map, and_then, and or_else.
 * 
 * @tparam T Success value type
 * @tparam E Error type
 */
template<typename T, typename E>
class result {
private:
    std::variant<T, E> storage_;

public:
    using value_type = T;
    using error_type = E;

    // SFINAE helpers
    template<typename U>
    using enable_if_not_result = std::enable_if_t<
        !std::is_same_v<std::decay_t<U>, result>>;

    /**
     * @brief Construct successful result from value
     * @param value Success value
     */
    template<typename U = T, typename = enable_if_not_result<U>>
    constexpr result(U&& value) noexcept(std::is_nothrow_constructible_v<T, U&&>)
        : storage_(std::in_place_index<0>, std::forward<U>(value)) {}

    /**
     * @brief Construct error result from error value
     * @param error Error value
     */
    template<typename F, typename = enable_if_not_result<F>>
    constexpr result(F&& error) noexcept(std::is_nothrow_constructible_v<E, F&&>)
        requires(!std::is_same_v<std::decay_t<F>, T> && std::is_constructible_v<E, F&&>)
        : storage_(std::in_place_index<1>, std::forward<F>(error)) {}

    /**
     * @brief Copy constructor
     */
    constexpr result(const result&) = default;

    /**
     * @brief Move constructor
     */
    constexpr result(result&&) noexcept = default;

    /**
     * @brief Copy assignment operator
     */
    constexpr result& operator=(const result&) = default;

    /**
     * @brief Move assignment operator
     */
    constexpr result& operator=(result&&) noexcept = default;

    /**
     * @brief Destructor
     */
    ~result() = default;

    /**
     * @brief Check if result contains a success value
     * @return true if successful, false if error
     */
    constexpr bool is_ok() const noexcept {
        return storage_.index() == 0;
    }

    /**
     * @brief Check if result contains an error
     * @return true if error, false if successful
     */
    constexpr bool is_err() const noexcept {
        return storage_.index() == 1;
    }

    /**
     * @brief Boolean conversion - true if successful
     */
    constexpr explicit operator bool() const noexcept {
        return is_ok();
    }

    /**
     * @brief Get success value (throws if error)
     * @return Reference to success value
     * @throws std::bad_variant_access if result contains error
     */
    constexpr const T& value() const& {
        if (is_err()) {
            throw std::bad_variant_access{};
        }
        return std::get<0>(storage_);
    }

    /**
     * @brief Get success value (throws if error)
     * @return Reference to success value
     * @throws std::bad_variant_access if result contains error
     */
    constexpr T& value() & {
        if (is_err()) {
            throw std::bad_variant_access{};
        }
        return std::get<0>(storage_);
    }

    /**
     * @brief Get success value (throws if error)
     * @return Moved success value
     * @throws std::bad_variant_access if result contains error
     */
    constexpr T&& value() && {
        if (is_err()) {
            throw std::bad_variant_access{};
        }
        return std::get<0>(std::move(storage_));
    }

    /**
     * @brief Get error value (throws if success)
     * @return Reference to error value
     * @throws std::bad_variant_access if result contains success value
     */
    constexpr const E& error() const& {
        if (is_ok()) {
            throw std::bad_variant_access{};
        }
        return std::get<1>(storage_);
    }

    /**
     * @brief Get error value (throws if success)
     * @return Reference to error value
     * @throws std::bad_variant_access if result contains success value
     */
    constexpr E& error() & {
        if (is_ok()) {
            throw std::bad_variant_access{};
        }
        return std::get<1>(storage_);
    }

    /**
     * @brief Get error value (throws if success)
     * @return Moved error value
     * @throws std::bad_variant_access if result contains success value
     */
    constexpr E&& error() && {
        if (is_ok()) {
            throw std::bad_variant_access{};
        }
        return std::get<1>(std::move(storage_));
    }

    /**
     * @brief Get value or default if error
     * @param default_value Default value to return if error
     * @return Success value or default
     */
    template<typename U>
    constexpr T value_or(U&& default_value) const& {
        return is_ok() ? value() : static_cast<T>(std::forward<U>(default_value));
    }

    /**
     * @brief Get value or default if error
     * @param default_value Default value to return if error
     * @return Success value or default
     */
    template<typename U>
    constexpr T value_or(U&& default_value) && {
        return is_ok() ? std::move(*this).value() : static_cast<T>(std::forward<U>(default_value));
    }

    /**
     * @brief Transform success value with function
     * @tparam F Function type
     * @param func Function to apply to success value
     * @return New result with transformed value or original error
     */
    template<typename F>
    constexpr auto map(F&& func) const& -> result<std::invoke_result_t<F, const T&>, E> {
        using U = std::invoke_result_t<F, const T&>;
        if (is_ok()) {
            if constexpr (std::is_void_v<U>) {
                std::invoke(std::forward<F>(func), value());
                return ok();
            } else {
                return ok(std::invoke(std::forward<F>(func), value()));
            }
        } else {
            return err(error());
        }
    }

    /**
     * @brief Transform success value with function
     * @tparam F Function type
     * @param func Function to apply to success value
     * @return New result with transformed value or original error
     */
    template<typename F>
    constexpr auto map(F&& func) && -> result<std::invoke_result_t<F, T&&>, E> {
        using U = std::invoke_result_t<F, T&&>;
        if (is_ok()) {
            if constexpr (std::is_void_v<U>) {
                std::invoke(std::forward<F>(func), std::move(*this).value());
                return ok();
            } else {
                return ok(std::invoke(std::forward<F>(func), std::move(*this).value()));
            }
        } else {
            return err(std::move(*this).error());
        }
    }

    /**
     * @brief Transform error value with function
     * @tparam F Function type
     * @param func Function to apply to error value
     * @return New result with original value or transformed error
     */
    template<typename F>
    constexpr auto map_err(F&& func) const& -> result<T, std::invoke_result_t<F, const E&>> {
        if (is_err()) {
            return err(std::invoke(std::forward<F>(func), error()));
        } else {
            return ok(value());
        }
    }

    /**
     * @brief Transform error value with function
     * @tparam F Function type
     * @param func Function to apply to error value
     * @return New result with original value or transformed error
     */
    template<typename F>
    constexpr auto map_err(F&& func) && -> result<T, std::invoke_result_t<F, E&&>> {
        if (is_err()) {
            return err(std::invoke(std::forward<F>(func), std::move(*this).error()));
        } else {
            return ok(std::move(*this).value());
        }
    }

    /**
     * @brief Chain operations that can fail
     * @tparam F Function type
     * @param func Function that returns a result
     * @return Result of function or original error
     */
    template<typename F>
    constexpr auto and_then(F&& func) const& -> std::invoke_result_t<F, const T&> {
        using result_type = std::invoke_result_t<F, const T&>;
        static_assert(std::is_same_v<typename result_type::error_type, E>,
                     "Error types must match");
        
        if (is_ok()) {
            return std::invoke(std::forward<F>(func), value());
        } else {
            return result_type::err(error());
        }
    }

    /**
     * @brief Chain operations that can fail
     * @tparam F Function type
     * @param func Function that returns a result
     * @return Result of function or original error
     */
    template<typename F>
    constexpr auto and_then(F&& func) && -> std::invoke_result_t<F, T&&> {
        using result_type = std::invoke_result_t<F, T&&>;
        static_assert(std::is_same_v<typename result_type::error_type, E>,
                     "Error types must match");
        
        if (is_ok()) {
            return std::invoke(std::forward<F>(func), std::move(*this).value());
        } else {
            return result_type::err(std::move(*this).error());
        }
    }

    /**
     * @brief Provide alternative on error
     * @tparam F Function type
     * @param func Function that returns a result
     * @return Original result if success, or result of function
     */
    template<typename F>
    constexpr auto or_else(F&& func) const& -> std::invoke_result_t<F, const E&> {
        using result_type = std::invoke_result_t<F, const E&>;
        static_assert(std::is_same_v<typename result_type::value_type, T>,
                     "Value types must match");
        
        if (is_err()) {
            return std::invoke(std::forward<F>(func), error());
        } else {
            return result_type::ok(value());
        }
    }

    /**
     * @brief Provide alternative on error
     * @tparam F Function type
     * @param func Function that returns a result
     * @return Original result if success, or result of function
     */
    template<typename F>
    constexpr auto or_else(F&& func) && -> std::invoke_result_t<F, E&&> {
        using result_type = std::invoke_result_t<F, E&&>;
        static_assert(std::is_same_v<typename result_type::value_type, T>,
                     "Value types must match");
        
        if (is_err()) {
            return std::invoke(std::forward<F>(func), std::move(*this).error());
        } else {
            return result_type::ok(std::move(*this).value());
        }
    }

    /**
     * @brief Equality comparison
     */
    constexpr bool operator==(const result& other) const 
        noexcept(noexcept(std::declval<T>() == std::declval<T>()) && 
                 noexcept(std::declval<E>() == std::declval<E>())) {
        if (storage_.index() != other.storage_.index()) {
            return false;
        }
        return storage_ == other.storage_;
    }

    /**
     * @brief Inequality comparison
     */
    constexpr bool operator!=(const result& other) const 
        noexcept(noexcept(std::declval<result>() == std::declval<result>())) {
        return !(*this == other);
    }

    // Factory functions
    
    /**
     * @brief Create successful result
     * @tparam Args Argument types
     * @param args Arguments to construct T
     * @return Successful result
     */
    template<typename... Args>
    static constexpr result ok(Args&&... args) 
        noexcept(std::is_nothrow_constructible_v<T, Args...>) {
        return result{std::in_place_index<0>, std::forward<Args>(args)...};
    }

    /**
     * @brief Create error result
     * @tparam Args Argument types
     * @param args Arguments to construct E
     * @return Error result
     */
    template<typename... Args>
    static constexpr result err(Args&&... args) 
        noexcept(std::is_nothrow_constructible_v<E, Args...>) {
        return result{std::in_place_index<1>, std::forward<Args>(args)...};
    }

private:
    // Private constructor for in_place construction
    template<std::size_t I, typename... Args>
    constexpr result(std::in_place_index_t<I> tag, Args&&... args)
        : storage_(tag, std::forward<Args>(args)...) {}
};

// Specialization for void success type
template<typename E>
class result<void, E> {
private:
    std::optional<E> error_;

public:
    using value_type = void;
    using error_type = E;

    /**
     * @brief Default constructor - creates successful result
     */
    constexpr result() noexcept : error_{} {}

    /**
     * @brief Construct error result
     * @param error Error value
     */
    template<typename F>
    constexpr result(F&& error) noexcept(std::is_nothrow_constructible_v<E, F&&>)
        requires(std::is_constructible_v<E, F&&>)
        : error_(std::forward<F>(error)) {}

    /**
     * @brief Copy constructor
     */
    constexpr result(const result&) = default;

    /**
     * @brief Move constructor
     */
    constexpr result(result&&) noexcept = default;

    /**
     * @brief Copy assignment operator
     */
    constexpr result& operator=(const result&) = default;

    /**
     * @brief Move assignment operator
     */
    constexpr result& operator=(result&&) noexcept = default;

    /**
     * @brief Destructor
     */
    ~result() = default;

    /**
     * @brief Check if result is successful
     */
    constexpr bool is_ok() const noexcept {
        return !error_.has_value();
    }

    /**
     * @brief Check if result contains error
     */
    constexpr bool is_err() const noexcept {
        return error_.has_value();
    }

    /**
     * @brief Boolean conversion
     */
    constexpr explicit operator bool() const noexcept {
        return is_ok();
    }

    /**
     * @brief Get error value
     */
    constexpr const E& error() const& {
        return error_.value();
    }

    /**
     * @brief Get error value
     */
    constexpr E& error() & {
        return error_.value();
    }

    /**
     * @brief Get error value
     */
    constexpr E&& error() && {
        return std::move(error_).value();
    }

    /**
     * @brief Transform with function
     */
    template<typename F>
    constexpr auto map(F&& func) const& -> result<std::invoke_result_t<F>, E> {
        using U = std::invoke_result_t<F>;
        if (is_ok()) {
            if constexpr (std::is_void_v<U>) {
                std::invoke(std::forward<F>(func));
                return ok();
            } else {
                return result<U, E>::ok(std::invoke(std::forward<F>(func)));
            }
        } else {
            return result<U, E>::err(error());
        }
    }

    /**
     * @brief Transform with function
     */
    template<typename F>
    constexpr auto map(F&& func) && -> result<std::invoke_result_t<F>, E> {
        using U = std::invoke_result_t<F>;
        if (is_ok()) {
            if constexpr (std::is_void_v<U>) {
                std::invoke(std::forward<F>(func));
                return ok();
            } else {
                return result<U, E>::ok(std::invoke(std::forward<F>(func)));
            }
        } else {
            return result<U, E>::err(std::move(*this).error());
        }
    }

    /**
     * @brief Transform error
     */
    template<typename F>
    constexpr auto map_err(F&& func) const& -> result<void, std::invoke_result_t<F, const E&>> {
        using G = std::invoke_result_t<F, const E&>;
        if (is_err()) {
            return result<void, G>::err(std::invoke(std::forward<F>(func), error()));
        } else {
            return result<void, G>::ok();
        }
    }

    /**
     * @brief Transform error
     */
    template<typename F>
    constexpr auto map_err(F&& func) && -> result<void, std::invoke_result_t<F, E&&>> {
        using G = std::invoke_result_t<F, E&&>;
        if (is_err()) {
            return result<void, G>::err(std::invoke(std::forward<F>(func), std::move(*this).error()));
        } else {
            return result<void, G>::ok();
        }
    }

    /**
     * @brief Chain operations
     */
    template<typename F>
    constexpr auto and_then(F&& func) const& -> std::invoke_result_t<F> {
        using result_type = std::invoke_result_t<F>;
        static_assert(std::is_same_v<typename result_type::error_type, E>,
                     "Error types must match");
        
        if (is_ok()) {
            return std::invoke(std::forward<F>(func));
        } else {
            return result_type::err(error());
        }
    }

    /**
     * @brief Chain operations
     */
    template<typename F>
    constexpr auto and_then(F&& func) && -> std::invoke_result_t<F> {
        using result_type = std::invoke_result_t<F>;
        static_assert(std::is_same_v<typename result_type::error_type, E>,
                     "Error types must match");
        
        if (is_ok()) {
            return std::invoke(std::forward<F>(func));
        } else {
            return result_type::err(std::move(*this).error());
        }
    }

    /**
     * @brief Provide alternative
     */
    template<typename F>
    constexpr auto or_else(F&& func) const& -> std::invoke_result_t<F, const E&> {
        using result_type = std::invoke_result_t<F, const E&>;
        static_assert(std::is_void_v<typename result_type::value_type>,
                     "Value types must match");
        
        if (is_err()) {
            return std::invoke(std::forward<F>(func), error());
        } else {
            return result_type::ok();
        }
    }

    /**
     * @brief Provide alternative
     */
    template<typename F>
    constexpr auto or_else(F&& func) && -> std::invoke_result_t<F, E&&> {
        using result_type = std::invoke_result_t<F, E&&>;
        static_assert(std::is_void_v<typename result_type::value_type>,
                     "Value types must match");
        
        if (is_err()) {
            return std::invoke(std::forward<F>(func), std::move(*this).error());
        } else {
            return result_type::ok();
        }
    }

    /**
     * @brief Equality comparison
     */
    constexpr bool operator==(const result& other) const 
        noexcept(noexcept(std::declval<E>() == std::declval<E>())) {
        return error_ == other.error_;
    }

    /**
     * @brief Inequality comparison
     */
    constexpr bool operator!=(const result& other) const 
        noexcept(noexcept(std::declval<result>() == std::declval<result>())) {
        return !(*this == other);
    }

    // Factory functions
    
    /**
     * @brief Create successful result
     */
    static constexpr result ok() noexcept {
        return result{};
    }

    /**
     * @brief Create error result
     */
    template<typename... Args>
    static constexpr result err(Args&&... args) 
        noexcept(std::is_nothrow_constructible_v<E, Args...>) {
        return result{E{std::forward<Args>(args)...}};
    }
};

// Convenience type aliases
template<typename T>
using result_void = result<T, void>;

template<typename E>
using void_result = result<void, E>;

} // namespace ss::core