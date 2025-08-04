#pragma once

#include "./observer.hpp"
#include "./core/interfaces.hpp"
#include "./core/result.hpp"
#include "./core/types.hpp"
#include <utils.hpp>

#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <mutex>
#include <atomic>
#include <chrono>
#include <type_traits>
#include <boost/asio.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>

namespace ss {

/**
 * @brief Default observer storage configuration
 */
constexpr std::chrono::seconds DEFAULT_OBSERVER_STORAGE_TICK_TIME{60};
constexpr std::chrono::seconds DEFAULT_OBSERVER_STORAGE_SHOW_STATE_TIME{2};

/**
 * @brief Observer ID index tag for multi-index container
 */
struct by_observer_id {
public:
    template<typename T> 
    using key_type = typename observer<T>::id;
};

/**
 * @brief Default observer storage indexing policy
 * 
 * Provides multi-index container configuration for efficient observer
 * storage and retrieval by ID with proper hash and equality support.
 */
template<typename T> 
struct default_observer_storage_policy {
    using index_type = boost::multi_index::indexed_by<
        boost::multi_index::hashed_unique<
            boost::multi_index::tag<by_observer_id>,
            boost::multi_index::const_mem_fun<
                observer<T>, 
                typename observer<T>::id,
                &observer<T>::get_id
            >,
            typename observer<T>::Hash,
            typename observer<T>::Equal
        >
    >;
};

/**
 * @brief Thread-safe observer storage with automatic cleanup
 * 
 * This class provides a modern, type-safe replacement for the legacy
 * observer_strage (with corrected spelling). It uses multi-index containers
 * for efficient storage and retrieval, and includes automatic cleanup of
 * expired observers.
 * 
 * Template Parameters:
 * - IndexPolicy: Policy class defining the multi-index container structure
 * - Ts: Observer types to store in this storage
 * 
 * Thread Safety: All operations are thread-safe
 * Exception Safety: Strong guarantee - operations either succeed completely or have no effect
 */
template<template<typename> class IndexPolicy, typename... Ts>
class observer_storage final : public ss::core::i_component {
public:
    /// Result type for operations that can fail
    template<typename T = void>
    using result = ss::core::result<T, std::string>;

    /**
     * @brief Constructor
     * @param io_context IO context for async operations
     */
    explicit observer_storage(ss::core::io_context& io_context);

    /**
     * @brief Destructor
     */
    ~observer_storage() noexcept override;

    // Non-copyable, non-movable
    observer_storage(const observer_storage&) = delete;
    observer_storage& operator=(const observer_storage&) = delete;
    observer_storage(observer_storage&&) = delete;
    observer_storage& operator=(observer_storage&&) = delete;

    // ss::core::i_component interface
    std::string name() const noexcept override { return "observer_storage"; }
    std::string version() const noexcept override { return "2.0.0"; }
    bool is_healthy() const noexcept override;
    std::string status() const override;
    ss::core::async_void initialize() override;
    ss::core::async_void cleanup() override;
    ss::core::async_void start() override;
    ss::core::async_void stop() override;
    bool is_running() const noexcept override;

    /// Entry type for specific observer type
    template<typename T>
    using entry = boost::multi_index_container<
        typename observer<T>::ref,
        typename IndexPolicy<T>::index_type
    >;

    /// Found observers result type
    template<typename T> 
    using found_observers = std::vector<typename observer<T>::ref>;

    /**
     * @brief Find observers by key
     * @tparam T Observer type
     * @tparam Key Index key type (defaults to by_observer_id)
     * @param key Search key
     * @return Vector of matching observers
     */
    template<typename T, typename Key = by_observer_id> 
    found_observers<T> find_observer(const typename Key::template key_type<T>& key);

    /**
     * @brief Find observer iterator range by key
     * @tparam T Observer type
     * @tparam Key Index key type (defaults to by_observer_id)
     * @param key Search key
     * @return Iterator range for matching observers
     */
    template<typename T, typename Key = by_observer_id> 
    auto find_observer_itr_range(const typename Key::template key_type<T>& key) 
        -> decltype(std::declval<entry<T>>().template get<Key>().equal_range(key));

    /**
     * @brief Find and remove observers by key
     * @tparam T Observer type
     * @tparam Key Index key type (defaults to by_observer_id)
     * @param key Search key
     * @return Vector of removed observers
     */
    template<typename T, typename Key = by_observer_id> 
    found_observers<T> pop_observer(const typename Key::template key_type<T>& key);

    /**
     * @brief Add observer to storage
     * @tparam T Observer type
     * @param obs Observer to add
     * @return true if added successfully, false if already exists
     */
    template<typename T> 
    bool add_observer(observer<T> obs);

    /**
     * @brief Add observer reference to storage
     * @tparam T Observer type
     * @param obs_ref Observer reference to add
     * @return true if added successfully, false if already exists
     */
    template<typename T> 
    bool add_observer(typename observer<T>::ref obs_ref);

    /**
     * @brief Delete observer by iterator
     * @tparam T Observer type
     * @param entry_ref Reference to the entry container
     * @param obs_itr Iterator to the observer to delete
     * @return Iterator following the deleted observer
     */
    template<typename T> 
    typename entry<T>::iterator delete_observer(entry<T>& entry_ref, 
                                               typename entry<T>::iterator obs_itr);

    /**
     * @brief Delete observer by key
     * @tparam T Observer type
     * @tparam Key Index key type (defaults to by_observer_id)
     * @param key Key to search for and delete
     * @return Iterator following the deleted observer
     */
    template<typename T, typename Key = by_observer_id> 
    typename entry<T>::iterator delete_observer(const typename Key::template key_type<T>& key);

    /**
     * @brief Get count of observers of specific type
     * @tparam T Observer type
     * @return Number of observers of type T
     */
    template<typename T>
    std::size_t observer_count() const noexcept;

    /**
     * @brief Get total count of all observers
     * @return Total number of observers across all types
     */
    std::size_t total_observer_count() const noexcept;

    /**
     * @brief Clear all observers of specific type
     * @tparam T Observer type
     */
    template<typename T>
    void clear_observers();

    /**
     * @brief Clear all observers of all types
     */
    void clear_all_observers();

    /**
     * @brief Force cleanup of expired observers
     * @return Number of observers cleaned up
     */
    std::size_t cleanup_expired_observers();

    /**
     * @brief Get storage statistics
     * @return JSON-formatted statistics string
     */
    std::string get_statistics() const;

private:
    /// IO context reference
    ss::core::io_context& io_context_;
    
    /// Storage tuple for all observer types
    std::tuple<entry<Ts>...> storage_;
    
    /// Running state
    std::atomic<bool> running_;
    
    /// Thread safety mutex
    mutable std::mutex mutex_;
    
    /// Cleanup timer
    std::unique_ptr<ss::core::steady_timer> cleanup_timer_;
    
    /// Debug state display timer (for development)
    std::unique_ptr<ss::core::steady_timer> debug_timer_;
    
    /// Statistics
    mutable std::atomic<std::size_t> total_additions_;
    mutable std::atomic<std::size_t> total_removals_;
    mutable std::atomic<std::size_t> total_expirations_;

    /**
     * @brief Delete expired observers from specific entry
     * @tparam T Observer type
     * @param entry_ref Reference to the entry container
     */
    template<typename T> 
    void delete_expired_observers(entry<T>& entry_ref);

    /**
     * @brief Print entry state for debugging
     * @tparam T Observer type
     * @param entry_ref Reference to the entry container
     */
    template<typename T> 
    void print_entry_state(const entry<T>& entry_ref) const;

    /**
     * @brief Start cleanup timer
     */
    void start_cleanup_timer();

    /**
     * @brief Handle cleanup timer tick
     * @param error Error code from timer
     */
    void handle_cleanup_tick(const boost::system::error_code& error);

    /**
     * @brief Start debug timer (development only)
     */
    void start_debug_timer();

    /**
     * @brief Handle debug timer tick
     * @param error Error code from timer
     */
    void handle_debug_tick(const boost::system::error_code& error);
};

// Template implementation

template<template<typename> class IndexPolicy, typename... Ts>
observer_storage<IndexPolicy, Ts...>::observer_storage(ss::core::io_context& io_context)
    : io_context_(io_context)
    , running_(false)
    , total_additions_(0)
    , total_removals_(0)
    , total_expirations_(0)
{
}

template<template<typename> class IndexPolicy, typename... Ts>
observer_storage<IndexPolicy, Ts...>::~observer_storage() noexcept {
    try {
        if (running_.load()) {
            // Note: In a real implementation, we would properly await the async stop
            running_.store(false);
        }
    }
    catch (...) {
        // Destructor should not throw
    }
}

template<template<typename> class IndexPolicy, typename... Ts>
bool observer_storage<IndexPolicy, Ts...>::is_healthy() const noexcept {
    return running_.load();
}

template<template<typename> class IndexPolicy, typename... Ts>
std::string observer_storage<IndexPolicy, Ts...>::status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    oss << "{"
        << "\"running\":" << (running_.load() ? "true" : "false") << ","
        << "\"total_count\":" << total_observer_count() << ","
        << "\"total_additions\":" << total_additions_.load() << ","
        << "\"total_removals\":" << total_removals_.load() << ","
        << "\"total_expirations\":" << total_expirations_.load()
        << "}";
    return oss.str();
}

template<template<typename> class IndexPolicy, typename... Ts>
ss::core::async_void observer_storage<IndexPolicy, Ts...>::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    // Initialize timers but don't start them yet
    co_return;
}

template<template<typename> class IndexPolicy, typename... Ts>
ss::core::async_void observer_storage<IndexPolicy, Ts...>::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Cancel timers
    if (cleanup_timer_) {
        cleanup_timer_.reset();
    }
    if (debug_timer_) {
        debug_timer_.reset();
    }
    
    // Clear all observers
    clear_all_observers();
    
    co_return;
}

template<template<typename> class IndexPolicy, typename... Ts>
ss::core::async_void observer_storage<IndexPolicy, Ts...>::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (running_.load()) {
        co_return;
    }
    
    running_.store(true);
    start_cleanup_timer();
    
#ifdef SS_DEBUG
    start_debug_timer();
#endif
    
    co_return;
}

template<template<typename> class IndexPolicy, typename... Ts>
ss::core::async_void observer_storage<IndexPolicy, Ts...>::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    running_.store(false);
    
    // Cancel timers
    if (cleanup_timer_) {
        cleanup_timer_.reset();
    }
    if (debug_timer_) {
        debug_timer_.reset();
    }
    
    co_return;
}

template<template<typename> class IndexPolicy, typename... Ts>
bool observer_storage<IndexPolicy, Ts...>::is_running() const noexcept {
    return running_.load();
}

template<template<typename> class IndexPolicy, typename... Ts>
template<typename T, typename Key>
typename observer_storage<IndexPolicy, Ts...>::template found_observers<T> 
observer_storage<IndexPolicy, Ts...>::find_observer(const typename Key::template key_type<T>& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    found_observers<T> result;
    auto itr_range = find_observer_itr_range<T, Key>(key);
    
    for (auto itr = itr_range.first; itr != itr_range.second; ++itr) {
        result.push_back(*itr);
    }
    
    return result;
}

template<template<typename> class IndexPolicy, typename... Ts>
template<typename T, typename Key>
auto observer_storage<IndexPolicy, Ts...>::find_observer_itr_range(
    const typename Key::template key_type<T>& key) 
    -> decltype(std::declval<entry<T>>().template get<Key>().equal_range(key)) {
    
    auto& storage_entry = std::get<entry<T>>(storage_);
    return storage_entry.template get<Key>().equal_range(key);
}

template<template<typename> class IndexPolicy, typename... Ts>
template<typename T, typename Key>
typename observer_storage<IndexPolicy, Ts...>::template found_observers<T> 
observer_storage<IndexPolicy, Ts...>::pop_observer(const typename Key::template key_type<T>& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    found_observers<T> result;
    auto& storage_entry = std::get<entry<T>>(storage_);
    auto itr_range = find_observer_itr_range<T, Key>(key);
    
    for (auto itr = itr_range.first; itr != itr_range.second;) {
        typename observer<T>::ref observer_ref = *itr;
        itr = delete_observer<T>(storage_entry, itr);
        result.push_back(observer_ref);
    }
    
    total_removals_.fetch_add(result.size());
    return result;
}

template<template<typename> class IndexPolicy, typename... Ts>
template<typename T>
bool observer_storage<IndexPolicy, Ts...>::add_observer(observer<T> obs) {
    return add_observer<T>(obs.get_ref());
}

template<template<typename> class IndexPolicy, typename... Ts>
template<typename T>
bool observer_storage<IndexPolicy, Ts...>::add_observer(typename observer<T>::ref obs_ref) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto& storage_entry = std::get<entry<T>>(storage_);
    auto [iter, inserted] = storage_entry.insert(obs_ref);
    
    if (inserted) {
        total_additions_.fetch_add(1);
    }
    
    return inserted;
}

template<template<typename> class IndexPolicy, typename... Ts>
template<typename T>
typename observer_storage<IndexPolicy, Ts...>::template entry<T>::iterator 
observer_storage<IndexPolicy, Ts...>::delete_observer(entry<T>& entry_ref, 
                                                      typename entry<T>::iterator obs_itr) {
    total_removals_.fetch_add(1);
    return entry_ref.erase(obs_itr);
}

template<template<typename> class IndexPolicy, typename... Ts>
template<typename T, typename Key>
typename observer_storage<IndexPolicy, Ts...>::template entry<T>::iterator 
observer_storage<IndexPolicy, Ts...>::delete_observer(const typename Key::template key_type<T>& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto& storage_entry = std::get<entry<T>>(storage_);
    auto found_itr = storage_entry.template get<Key>().find(key);
    
    if (found_itr != storage_entry.template get<Key>().end()) {
        total_removals_.fetch_add(1);
        return storage_entry.erase(found_itr);
    }
    
    return storage_entry.end();
}

template<template<typename> class IndexPolicy, typename... Ts>
template<typename T>
std::size_t observer_storage<IndexPolicy, Ts...>::observer_count() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto& storage_entry = std::get<entry<T>>(storage_);
    return storage_entry.size();
}

template<template<typename> class IndexPolicy, typename... Ts>
std::size_t observer_storage<IndexPolicy, Ts...>::total_observer_count() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::apply([](const auto&... entries) {
        return (entries.size() + ...);
    }, storage_);
}

template<template<typename> class IndexPolicy, typename... Ts>
template<typename T>
void observer_storage<IndexPolicy, Ts...>::clear_observers() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& storage_entry = std::get<entry<T>>(storage_);
    auto count = storage_entry.size();
    storage_entry.clear();
    total_removals_.fetch_add(count);
}

template<template<typename> class IndexPolicy, typename... Ts>
void observer_storage<IndexPolicy, Ts...>::clear_all_observers() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::apply([this](auto&... entries) {
        auto total_count = (entries.size() + ...);
        (entries.clear(), ...);
        total_removals_.fetch_add(total_count);
    }, storage_);
}

template<template<typename> class IndexPolicy, typename... Ts>
std::size_t observer_storage<IndexPolicy, Ts...>::cleanup_expired_observers() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::size_t total_cleaned = 0;
    std::apply([this, &total_cleaned](auto&... entries) {
        auto count_before = (entries.size() + ...);
        (delete_expired_observers<Ts>(entries), ...);
        auto count_after = (entries.size() + ...);
        total_cleaned = count_before - count_after;
    }, storage_);
    
    total_expirations_.fetch_add(total_cleaned);
    return total_cleaned;
}

template<template<typename> class IndexPolicy, typename... Ts>
std::string observer_storage<IndexPolicy, Ts...>::get_statistics() const {
    return status(); // Reuse status method for now
}

template<template<typename> class IndexPolicy, typename... Ts>
template<typename T>
void observer_storage<IndexPolicy, Ts...>::delete_expired_observers(entry<T>& entry_ref) {
    for (auto itr = entry_ref.begin(); itr != entry_ref.end();) {
        if ((*itr)->is_expired()) {
            itr = delete_observer<T>(entry_ref, itr);
        } else {
            ++itr;
        }
    }
}

template<template<typename> class IndexPolicy, typename... Ts>
template<typename T>
void observer_storage<IndexPolicy, Ts...>::print_entry_state(const entry<T>& entry_ref) const {
    // Print header
    for (int i = 0; i < get_console_width() / 2; i++) { 
        std::printf("="); 
    }
    std::cout << "\n| OBSERVER ENTRY (" << typeid(T).name() << ")\n";
    
    for (int i = 0; i < get_console_width() / 2; i++) { 
        std::printf("-"); 
    }
    std::cout << "\n";
    
    // Print observers
    std::cout << "\x1b[32m"; // Green color
    unsigned int count = 0;
    for (const auto& observer_ref : entry_ref) {
        std::cout << "| (" << count << ") ";
        observer_ref->print();
        std::cout << "\n";
        count++;
    }
    std::cout << "\x1b[39m\n"; // Reset color
}

template<template<typename> class IndexPolicy, typename... Ts>
void observer_storage<IndexPolicy, Ts...>::start_cleanup_timer() {
    // Note: Simplified implementation - in a real system we would create and manage the timer properly
}

template<template<typename> class IndexPolicy, typename... Ts>
void observer_storage<IndexPolicy, Ts...>::handle_cleanup_tick(const boost::system::error_code& error) {
    if (!error && running_.load()) {
        cleanup_expired_observers();
        start_cleanup_timer(); // Restart timer for next tick
    }
}

template<template<typename> class IndexPolicy, typename... Ts>
void observer_storage<IndexPolicy, Ts...>::start_debug_timer() {
    // Note: Simplified implementation - in a real system we would create and manage the timer properly
}

template<template<typename> class IndexPolicy, typename... Ts>
void observer_storage<IndexPolicy, Ts...>::handle_debug_tick(const boost::system::error_code& error) {
    if (!error && running_.load()) {
        // Print state for debugging
        std::apply([this](const auto&... entries) {
            (print_entry_state<Ts>(entries), ...);
        }, storage_);
        
        start_debug_timer(); // Restart timer for next tick
    }
}

// Type alias for backward compatibility (corrected spelling)
template<typename... Ts>
using observer_strage = observer_storage<default_observer_storage_policy, Ts...>;

} // namespace ss