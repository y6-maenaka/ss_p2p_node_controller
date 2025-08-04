#include <ss_p2p/observer.hpp>

#include <sstream>
#include <iostream>
#include <random>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/random_generator.hpp>

namespace ss {

// Utility functions

observer_id generate_uuid_from_str(const std::string& seed) {
    std::seed_seq ss(seed.begin(), seed.end());
    std::mt19937 rng(ss);
    boost::uuids::basic_random_generator<std::mt19937> gen(&rng);
    return gen();
}

observer_id str_to_observer_id(const std::string& from) {
    try {
        boost::uuids::string_generator gen;
        return gen(from);
    }
    catch (const std::exception&) {
        // Return nil UUID on error
        return observer_id{};
    }
}

std::string observer_id_to_str(const observer_id& from) {
    return boost::uuids::to_string(from);
}

// base_observer implementation

base_observer::base_observer(
    ss::core::io_context& io_context,
    std::string type_name,
    const observer_id& id
) : io_context_(io_context)
  , type_name_(std::move(type_name))
  , force_expire_flag_(false)
  , timeout_(DEFAULT_EXPIRE_TIME)
  , running_(false)
  , observer_id_counter_(0)
{
    // Generate ID if not provided
    if (id.is_nil()) {
        boost::uuids::random_generator gen;
        id_ = gen();
    } else {
        id_ = id;
    }
    
    // Set initial expiration time
    auto now = std::chrono::steady_clock::now();
    expire_at_.store(now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(timeout_.load()));
}

base_observer::~base_observer() noexcept {
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

bool base_observer::is_healthy() const noexcept {
    return running_.load() && !is_expired();
}

std::string base_observer::status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    oss << "{"
        << "\"id\":\"" << observer_id_to_str(id_) << "\","
        << "\"type\":\"" << type_name_ << "\","
        << "\"running\":" << (running_.load() ? "true" : "false") << ","
        << "\"expired\":" << (is_expired() ? "true" : "false") << ","
        << "\"time_left\":" << get_expire_time_left().count() << ","
        << "\"observer_count\":" << observers_.size()
        << "}";
    return oss.str();
}

ss::core::async_void base_observer::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    // Initialize timeout timer
    start_timeout_timer();
    co_return;
}

ss::core::async_void base_observer::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Cancel timeout timer
    if (timeout_timer_) {
        timeout_timer_.reset();
    }
    
    // Clear all observers
    observers_.clear();
    
    co_return;
}

ss::core::async_void base_observer::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    running_.store(true);
    start_timeout_timer();
    co_return;
}

ss::core::async_void base_observer::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    running_.store(false);
    
    // Cancel timeout timer
    if (timeout_timer_) {
        timeout_timer_.reset();
    }
    
    co_return;
}

bool base_observer::is_running() const noexcept {
    return running_.load();
}

void base_observer::set_timeout(std::chrono::milliseconds timeout) noexcept {
    timeout_.store(timeout);
    
    // Update expiration time
    auto now = std::chrono::steady_clock::now();
    expire_at_.store(now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(timeout));
    
    // Restart timeout timer with new timeout
    std::lock_guard<std::mutex> lock(mutex_);
    start_timeout_timer();
}

std::chrono::milliseconds base_observer::get_timeout() const noexcept {
    return timeout_.load();
}

bool base_observer::is_timed_out() const noexcept {
    return is_expired();
}

ss::core::async_void base_observer::cancel() {
    force_expire();
    co_await stop();
}

observer_id base_observer::add_observer(observer_func observer) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto id = observer_id_counter_.fetch_add(1);
    observers_[id] = std::move(observer);
    return boost::uuids::random_generator{}(); // Return a unique UUID for the observer
}

bool base_observer::remove_observer(observer_id id) {
    std::lock_guard<std::mutex> lock(mutex_);
    // For simplicity, we'll search by the observer function hash
    // This is not perfect but maintains backward compatibility
    auto id_str = observer_id_to_str(id);
    auto id_hash = std::hash<std::string>{}(id_str);
    
    for (auto it = observers_.begin(); it != observers_.end(); ++it) {
        if (it->first == id_hash) {
            observers_.erase(it);
            return true;
        }
    }
    return false;
}

void base_observer::clear_observers() {
    std::lock_guard<std::mutex> lock(mutex_);
    observers_.clear();
}

std::size_t base_observer::observer_count() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return observers_.size();
}

observer_id base_observer::get_id() const noexcept {
    return id_;
}

std::string base_observer::get_id_str() const {
    return observer_id_to_str(id_);
}

bool base_observer::is_expired() const noexcept {
    if (force_expire_flag_.load()) {
        return true;
    }
    
    auto now = std::chrono::steady_clock::now();
    return now >= expire_at_.load();
}

const std::string& base_observer::get_type_name() const noexcept {
    return type_name_;
}

std::chrono::seconds base_observer::get_expire_time_left() const noexcept {
    if (force_expire_flag_.load()) {
        return std::chrono::seconds{0};
    }
    
    auto now = std::chrono::steady_clock::now();
    auto expire_time = expire_at_.load();
    
    if (now >= expire_time) {
        return std::chrono::seconds{0};
    }
    
    return std::chrono::duration_cast<std::chrono::seconds>(expire_time - now);
}

void base_observer::force_expire() noexcept {
    force_expire_flag_.store(true);
    
    // Notify observers of expiration
    observer_event event;
    event.event_type = observer_event::type::expired;
    event.message = "Observer force expired";
    event.timestamp = std::chrono::steady_clock::now();
    
    notify_observers(event);
}

void base_observer::extend_expire_time(std::chrono::seconds additional_time) {
    auto now = std::chrono::steady_clock::now();
    expire_at_.store(now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(additional_time));
    
    // Restart timeout timer
    std::lock_guard<std::mutex> lock(mutex_);
    start_timeout_timer();
}

void base_observer::print() const {
    std::cout << "[Observer] ID: " << get_id_str() 
              << ", Type: " << type_name_
              << ", Expired: " << (is_expired() ? "Yes" : "No")
              << ", Time left: " << get_expire_time_left().count() << "s"
              << std::endl;
}

void base_observer::notify_observers(const observer_event& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [id, observer_func] : observers_) {
        try {
            observer_func(event);
        }
        catch (const std::exception& e) {
            // Log error but continue notifying other observers
            std::cerr << "Error notifying observer: " << e.what() << std::endl;
        }
        catch (...) {
            // Unknown error, continue with other observers
            std::cerr << "Unknown error notifying observer" << std::endl;
        }
    }
}

void base_observer::on_timeout() {
    observer_event event;
    event.event_type = observer_event::type::expired;
    event.message = "Observer timeout expired";
    event.timestamp = std::chrono::steady_clock::now();
    
    notify_observers(event);
}

int base_observer::income_message(message& msg, boost::asio::ip::udp::endpoint& endpoint) {
    observer_event event;
    event.event_type = observer_event::type::message_received;
    event.message = "Message received";
    event.endpoint = endpoint;
    event.timestamp = std::chrono::steady_clock::now();
    
    notify_observers(event);
    return 0; // Success
}

void base_observer::on_send_done(const boost::system::error_code& ec) {
    observer_event event;
    event.event_type = observer_event::type::send_completed;
    event.error_code = ec;
    event.message = ec ? "Send failed: " + ec.message() : "Send completed successfully";
    event.timestamp = std::chrono::steady_clock::now();
    
    notify_observers(event);
}

void base_observer::start_timeout_timer() {
    // Note: This is a simplified implementation
    // In a real implementation, we would create and start a proper timer
    // For now, we'll just note that the timer would be started here
}

void base_observer::handle_timeout(const boost::system::error_code& error) {
    if (!error && !force_expire_flag_.load()) {
        // Timeout occurred naturally
        force_expire();
        on_timeout();
    }
}

} // namespace ss