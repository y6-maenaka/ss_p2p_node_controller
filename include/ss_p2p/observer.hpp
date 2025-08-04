#pragma once

#include "./core/interfaces.hpp"
#include "./core/result.hpp"
#include "./core/types.hpp"
#include "./message.hpp"

#include <memory>
#include <string>
#include <chrono>
#include <atomic>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

namespace ss {

/**
 * @brief Observer event types for the legacy observer pattern
 */
struct observer_event {
    enum class type {
        message_received,
        send_completed,
        expired,
        error_occurred
    };

    type event_type;
    std::string message;
    boost::asio::ip::udp::endpoint endpoint{};
    boost::system::error_code error_code{};
    std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

/**
 * @brief Legacy observer ID type for backward compatibility
 */
using observer_id = boost::uuids::uuid;

/**
 * @brief Generate UUID from string seed (legacy compatibility)
 * @param seed String seed for UUID generation
 * @return Generated UUID
 */
observer_id generate_uuid_from_str(const std::string& seed);

/**
 * @brief Convert string to observer ID
 * @param from String representation
 * @return Observer ID
 */
observer_id str_to_observer_id(const std::string& from);

/**
 * @brief Convert observer ID to string
 * @param from Observer ID
 * @return String representation
 */
std::string observer_id_to_str(const observer_id& from);

/**
 * @brief Default expire time for observers
 */
constexpr std::chrono::seconds DEFAULT_EXPIRE_TIME{20};

/**
 * @brief Base observer class with timeout management
 * 
 * This class wraps the new core::i_observable interface to maintain
 * backward compatibility with the legacy observer pattern while providing
 * modern timeout management and type safety.
 */
class base_observer : public ss::core::i_component, 
                     public ss::core::i_timeout_managed,
                     public ss::core::i_observable<observer_event> {
public:
    /**
     * @brief Constructor
     * @param io_context IO context for async operations
     * @param type_name Observer type name
     * @param id Observer ID (auto-generated if not provided)
     */
    explicit base_observer(
        ss::core::io_context& io_context,
        std::string type_name,
        const observer_id& id = observer_id{}
    );

    /**
     * @brief Destructor
     */
    ~base_observer() noexcept override;

    // Non-copyable, movable
    base_observer(const base_observer&) = delete;
    base_observer& operator=(const base_observer&) = delete;
    base_observer(base_observer&&) = default;
    base_observer& operator=(base_observer&&) = default;

    // ss::core::i_component interface
    std::string name() const noexcept override { return "base_observer"; }
    std::string version() const noexcept override { return "2.0.0"; }
    bool is_healthy() const noexcept override;
    std::string status() const override;
    ss::core::async_void initialize() override;
    ss::core::async_void cleanup() override;
    ss::core::async_void start() override;
    ss::core::async_void stop() override;
    bool is_running() const noexcept override;

    // ss::core::i_timeout_managed interface
    void set_timeout(std::chrono::milliseconds timeout) noexcept override;
    std::chrono::milliseconds get_timeout() const noexcept override;
    bool is_timed_out() const noexcept override;
    ss::core::async_void cancel() override;

    // ss::core::i_observable<observer_event> interface
    observer_id add_observer(observer_func observer) override;
    bool remove_observer(observer_id id) override;
    void clear_observers() override;
    std::size_t observer_count() const noexcept override;

    /**
     * @brief Get observer ID
     * @return Observer ID
     */
    observer_id get_id() const noexcept;

    /**
     * @brief Get observer ID as string
     * @return Observer ID string
     */
    std::string get_id_str() const;

    /**
     * @brief Check if observer has expired
     * @return true if expired, false otherwise
     */
    bool is_expired() const noexcept;

    /**
     * @brief Get observer type name
     * @return Type name
     */
    const std::string& get_type_name() const noexcept;

    /**
     * @brief Get time left before expiration
     * @return Time left in seconds
     */
    std::chrono::seconds get_expire_time_left() const noexcept;

    /**
     * @brief Force expiration of this observer
     */
    void force_expire() noexcept;

    /**
     * @brief Extend expiration time
     * @param additional_time Additional time to add
     */
    void extend_expire_time(std::chrono::seconds additional_time = DEFAULT_EXPIRE_TIME);

    /**
     * @brief Print observer information (legacy compatibility)
     */
    virtual void print() const;

protected:
    /**
     * @brief Notify all observers of an event
     * @param event The event to notify about
     */
    void notify_observers(const observer_event& event) override;

    /**
     * @brief Handle timeout expiration
     */
    virtual void on_timeout();

    /**
     * @brief Handle message reception (legacy compatibility)
     * @param msg Received message
     * @param endpoint Sender endpoint
     * @return Processing result (0 = success, non-zero = error)
     */
    virtual int income_message(message& msg, boost::asio::ip::udp::endpoint& endpoint);

    /**
     * @brief Handle send completion (legacy compatibility)
     * @param ec Error code from send operation
     */
    virtual void on_send_done(const boost::system::error_code& ec);

private:
    /// IO context reference
    ss::core::io_context& io_context_;
    
    /// Observer ID
    observer_id id_;
    
    /// Observer type name
    std::string type_name_;
    
    /// Expiration time point
    std::atomic<std::chrono::steady_clock::time_point> expire_at_;
    
    /// Force expire flag
    std::atomic<bool> force_expire_flag_;
    
    /// Timeout duration
    std::atomic<std::chrono::milliseconds> timeout_;
    
    /// Running state
    std::atomic<bool> running_;
    
    /// Observer function map
    std::unordered_map<std::uint64_t, observer_func> observers_;
    
    /// Observer ID counter
    std::atomic<std::uint64_t> observer_id_counter_;
    
    /// Thread safety mutex
    mutable std::mutex mutex_;
    
    /// Timeout timer
    std::unique_ptr<ss::core::steady_timer> timeout_timer_;

    /**
     * @brief Start timeout timer
     */
    void start_timeout_timer();

    /**
     * @brief Handle timeout timer expiration
     * @param error Error code from timer
     */
    void handle_timeout(const boost::system::error_code& error);
};

/**
 * @brief Template observer wrapper for specific observer types
 * 
 * This class maintains the legacy observer<T> interface while wrapping
 * the new base_observer implementation.
 */
template<typename T>
class observer final : public std::enable_shared_from_this<observer<T>> {
public:
    using id = observer_id;
    using ref = std::shared_ptr<observer<T>>;

    /**
     * @brief Constructor from shared pointer
     * @param body Observer body
     */
    explicit observer(std::shared_ptr<T> body);

    /**
     * @brief Constructor from object
     * @param body Observer body object
     */
    explicit observer(T body);

    /**
     * @brief Constructor with forwarded arguments
     * @param io_context IO context
     * @param args Constructor arguments for T
     */
    template<typename... Args>
    explicit observer(ss::core::io_context& io_context, Args&&... args);

    /**
     * @brief Get shared reference to this observer
     * @return Shared pointer to this observer
     */
    ref get_ref();

    /**
     * @brief Get underlying observer body
     * @return Shared pointer to observer body
     */
    std::shared_ptr<T> get();

    /**
     * @brief Initialize observer with arguments
     * @param args Initialization arguments
     */
    template<typename... Args>
    void init(Args&&... args);

    /**
     * @brief Handle incoming message (legacy compatibility)
     * @param msg Message to handle
     * @param ep Sender endpoint
     * @return Processing result
     */
    int income_message(message& msg, boost::asio::ip::udp::endpoint& ep);

    /**
     * @brief Handle send completion (legacy compatibility)
     * @param ec Error code from send operation
     */
    void on_send_done(const boost::system::error_code& ec);

    /**
     * @brief Check if observer has expired
     * @return true if expired, false otherwise
     */
    bool is_expired() const;

    /**
     * @brief Get observer ID
     * @return Observer ID
     */
    id get_id() const;

    /**
     * @brief Get observer type name
     * @return Type name
     */
    const std::string get_type_name() const;

    /**
     * @brief Print observer information
     */
    void print() const;

    /**
     * @brief Get time left before expiration
     * @return Time left in seconds
     */
    std::chrono::seconds get_expire_time_left() const;

    /**
     * @brief Equality comparison
     * @param other Other observer
     * @return true if equal, false otherwise
     */
    bool operator==(const observer<T>& other) const;

    /**
     * @brief Inequality comparison
     * @param other Other observer
     * @return true if not equal, false otherwise
     */
    bool operator!=(const observer<T>& other) const;

    /// Hash functor for use in containers
    struct Hash {
        std::size_t operator()(const observer<T>& obs) const;
        std::size_t operator()(const std::shared_ptr<observer<T>>& obs_ref) const;
        std::size_t operator()(const id& obs_id) const;
    };

    /// Equality functor for use in containers
    struct Equal {
        bool operator()(const observer<T>& lhs, const observer<T>& rhs) const;
        bool operator()(const ref& lhs_ref, const ref& rhs_ref) const;
        bool operator()(const id& lhs_id, const id& rhs_id) const;
    };

private:
    /// Observer body
    std::shared_ptr<T> body_;
};

// Template implementation

template<typename T>
observer<T>::observer(std::shared_ptr<T> body) : body_(std::move(body)) {
    if (!body_) {
        throw std::invalid_argument("Observer body cannot be null");
    }
}

template<typename T>
observer<T>::observer(T body) : body_(std::make_shared<T>(std::move(body))) {}

template<typename T>
template<typename... Args>
observer<T>::observer(ss::core::io_context& io_context, Args&&... args)
    : body_(std::make_shared<T>(io_context, std::forward<Args>(args)...)) {}

template<typename T>
typename observer<T>::ref observer<T>::get_ref() {
    return std::make_shared<observer<T>>(body_);
}

template<typename T>
std::shared_ptr<T> observer<T>::get() {
    return body_;
}

template<typename T>
template<typename... Args>
void observer<T>::init(Args&&... args) {
    if (body_) {
        body_->init(std::forward<Args>(args)...);
    }
}

template<typename T>
int observer<T>::income_message(message& msg, boost::asio::ip::udp::endpoint& ep) {
    if (body_) {
        return body_->income_message(msg, ep);
    }
    return -1;
}

template<typename T>
void observer<T>::on_send_done(const boost::system::error_code& ec) {
    if (body_) {
        body_->on_send_done(ec);
    }
}

template<typename T>
bool observer<T>::is_expired() const {
    if (body_) {
        return body_->is_expired();
    }
    return true;
}

template<typename T>
typename observer<T>::id observer<T>::get_id() const {
    if (body_) {
        return body_->get_id();
    }
    return observer_id{};
}

template<typename T>
const std::string observer<T>::get_type_name() const {
    if (body_) {
        return body_->get_type_name();
    }
    return "unknown";
}

template<typename T>
void observer<T>::print() const {
    if (body_) {
        body_->print();
    }
}

template<typename T>
std::chrono::seconds observer<T>::get_expire_time_left() const {
    if (body_) {
        return body_->get_expire_time_left();
    }
    return std::chrono::seconds{0};
}

template<typename T>
bool observer<T>::operator==(const observer<T>& other) const {
    return body_ && other.body_ && body_->get_id() == other.body_->get_id();
}

template<typename T>
bool observer<T>::operator!=(const observer<T>& other) const {
    return !(*this == other);
}

template<typename T>
std::size_t observer<T>::Hash::operator()(const observer<T>& obs) const {
    return std::hash<std::string>{}(observer_id_to_str(obs.get_id()));
}

template<typename T>
std::size_t observer<T>::Hash::operator()(const std::shared_ptr<observer<T>>& obs_ref) const {
    if (obs_ref) {
        return std::hash<std::string>{}(observer_id_to_str(obs_ref->get_id()));
    }
    return 0;
}

template<typename T>
std::size_t observer<T>::Hash::operator()(const id& obs_id) const {
    return std::hash<std::string>{}(observer_id_to_str(obs_id));
}

template<typename T>
bool observer<T>::Equal::operator()(const observer<T>& lhs, const observer<T>& rhs) const {
    return lhs == rhs;
}

template<typename T>
bool observer<T>::Equal::operator()(const ref& lhs_ref, const ref& rhs_ref) const {
    if (lhs_ref && rhs_ref) {
        return *lhs_ref == *rhs_ref;
    }
    return lhs_ref == rhs_ref;
}

template<typename T>
bool observer<T>::Equal::operator()(const id& lhs_id, const id& rhs_id) const {
    return lhs_id == rhs_id;
}

} // namespace ss