#include "../../../../include/ss_p2p/application/i_message_bus.hpp"
#include "../../../../include/ss_p2p/core/result.hpp"
#include "../../../../include/ss_p2p/ss_logger.hpp"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <deque>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <regex>
#include <future>

namespace ss::application {

/**
 * @brief Topic queue with priority and TTL support
 */
class topic_queue {
public:
    /**
     * @brief Message entry with priority and expiration
     */
    struct message_entry {
        bus_message message;
        std::chrono::steady_clock::time_point enqueue_time;
        
        message_entry(bus_message msg)
            : message(std::move(msg))
            , enqueue_time(std::chrono::steady_clock::now())
        {}
        
        bool is_expired() const {
            return message.metadata.is_expired();
        }
        
        bool operator<(const message_entry& other) const {
            // Higher priority comes first
            return message.metadata.priority < other.message.metadata.priority;
        }
    };

    explicit topic_queue(std::size_t max_size = 10000)
        : max_size_(max_size) {}

    bool enqueue(bus_message message, message_bus_config::backpressure_policy policy) {
        std::unique_lock lock(mutex_);
        
        // Remove expired messages first
        cleanup_expired_messages();
        
        // Check if queue is full
        if (queue_.size() >= max_size_) {
            switch (policy) {
                case message_bus_config::backpressure_policy::drop_oldest:
                    if (!queue_.empty()) {
                        queue_.pop();
                    }
                    break;
                case message_bus_config::backpressure_policy::drop_newest:
                    return false;
                case message_bus_config::backpressure_policy::reject_message:
                    return false;
                case message_bus_config::backpressure_policy::block_sender:
                    // TODO: Implement blocking behavior
                    return false;
            }
        }
        
        queue_.emplace(std::move(message));
        condition_.notify_one();
        return true;
    }

    std::optional<bus_message> dequeue() {
        std::unique_lock lock(mutex_);
        
        cleanup_expired_messages();
        
        if (queue_.empty()) {
            return std::nullopt;
        }
        
        auto message = std::move(const_cast<bus_message&>(queue_.top().message));
        queue_.pop();
        return message;
    }

    std::size_t size() const {
        std::shared_lock lock(mutex_);
        return queue_.size();
    }

    void clear() {
        std::unique_lock lock(mutex_);
        
        // Clear the priority queue by swapping with empty queue
        std::priority_queue<message_entry> empty_queue;
        queue_.swap(empty_queue);
        
        condition_.notify_all();
    }

    std::vector<bus_message> get_all_messages() const {
        std::shared_lock lock(mutex_);
        
        std::vector<bus_message> messages;
        
        // Create a copy of the queue to iterate over
        auto temp_queue = queue_;
        while (!temp_queue.empty()) {
            if (!temp_queue.top().is_expired()) {
                messages.push_back(temp_queue.top().message);
            }
            temp_queue.pop();
        }
        
        return messages;
    }

private:
    void cleanup_expired_messages() {
        // This is a simplified cleanup - in practice, you might want a more efficient approach
        std::vector<message_entry> valid_messages;
        
        while (!queue_.empty()) {
            if (!queue_.top().is_expired()) {
                valid_messages.push_back(queue_.top());
            }
            queue_.pop();
        }
        
        for (auto& entry : valid_messages) {
            queue_.push(std::move(entry));
        }
    }

    mutable std::shared_mutex mutex_;
    std::priority_queue<message_entry> queue_;
    std::size_t max_size_;
    std::condition_variable_any condition_;
};

/**
 * @brief Implementation of Message Bus interface
 * 
 * Provides publish/subscribe messaging with topics, filtering, and
 * reliable delivery using async processing and backpressure control.
 */
class message_bus_impl : public i_message_bus {
public:
    /**
     * @brief Constructor
     * @param io_context Boost.Asio IO context
     */
    explicit message_bus_impl(boost::asio::io_context& io_context)
        : io_context_(io_context)
        , stats_{}
        , running_(false)
        , next_subscription_id_(1)
        , processing_timer_(io_context)
        , health_check_timer_(io_context)
        , logger_(std::make_shared<ss_logger>("MessageBus"))
    {
        stats_.start_time = std::chrono::steady_clock::now();
    }

    /**
     * @brief Destructor
     */
    ~message_bus_impl() override {
        if (running_.load()) {
            try {
                auto stop_future = boost::asio::co_spawn(io_context_, stop(), boost::asio::use_future);
                if (stop_future.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
                    logger_->error("Forced shutdown due to timeout");
                }
            } catch (const std::exception& e) {
                logger_->error("Error during shutdown: " + std::string(e.what()));
            }
        }
    }

    // ========================================
    // i_component Implementation
    // ========================================

    std::string name() const noexcept override {
        return "MessageBus";
    }

    std::string version() const noexcept override {
        return "1.0.0";
    }

    bool is_running() const noexcept override {
        return running_.load();
    }

    bool is_healthy() const noexcept override {
        std::shared_lock lock(mutex_);
        return running_.load() && 
               get_queue_size() < config_.max_queue_size * 0.9; // Consider unhealthy if 90% full
    }

    std::string status() const override {
        std::shared_lock lock(mutex_);
        
        nlohmann::json status_json;
        status_json["name"] = name();
        status_json["version"] = version();
        status_json["running"] = is_running();
        status_json["healthy"] = is_healthy();
        status_json["queue_size"] = get_queue_size();
        status_json["max_queue_size"] = config_.max_queue_size;
        status_json["topic_count"] = topic_queues_.size();
        status_json["subscription_count"] = subscriptions_.size();
        status_json["stats"] = get_stats().to_json();
        
        return status_json.dump(2);
    }

    // ========================================
    // Lifecycle Management
    // ========================================

    core::async_void initialize(const message_bus_config& config) override {
        std::unique_lock lock(mutex_);
        
        logger_->info("Initializing message bus with config");
        
        auto validation_result = config.validate();
        if (!validation_result.is_ok()) {
            throw std::invalid_argument("Invalid configuration: " + validation_result.error());
        }
        
        config_ = config;
        
        // Initialize worker threads
        worker_threads_.reserve(config_.worker_threads);
        
        co_return;
    }

    core::async_void start() override {
        std::unique_lock lock(mutex_);
        
        if (running_.load()) {
            logger_->warn("Message bus already running");
            co_return;
        }
        
        logger_->info("Starting message bus");
        
        try {
            running_.store(true);
            
            // Start worker threads
            for (std::size_t i = 0; i < config_.worker_threads; ++i) {
                worker_threads_.emplace_back(&message_bus_impl::worker_thread, this);
            }
            
            // Start periodic processing
            schedule_message_processing();
            schedule_health_check();
            
            // Load state if persistence is enabled
            if (config_.enable_persistence) {
                auto load_result = co_await load_state();
                if (!load_result) {
                    logger_->warn("Failed to load persisted state");
                }
            }
            
            logger_->info("Message bus started successfully");
            
        } catch (const std::exception& e) {
            running_.store(false);
            logger_->error("Failed to start message bus: " + std::string(e.what()));
            throw;
        }
        
        co_return;
    }

    core::async_void stop() override {
        std::unique_lock lock(mutex_);
        
        if (!running_.load()) {
            logger_->warn("Message bus not running");
            co_return;
        }
        
        logger_->info("Stopping message bus");
        
        try {
            running_.store(false);
            
            // Cancel timers
            processing_timer_.cancel();
            health_check_timer_.cancel();
            
            // Notify all workers to stop
            {
                std::unique_lock worker_lock(worker_mutex_);
                worker_condition_.notify_all();
            }
            
            // Wait for worker threads to finish
            for (auto& thread : worker_threads_) {
                if (thread.joinable()) {
                    thread.join();
                }
            }
            worker_threads_.clear();
            
            // Save state if persistence is enabled
            if (config_.enable_persistence) {
                auto save_result = co_await save_state();
                if (!save_result) {
                    logger_->warn("Failed to save state");
                }
            }
            
            logger_->info("Message bus stopped successfully");
            
        } catch (const std::exception& e) {
            logger_->error("Error during stop: " + std::string(e.what()));
        }
        
        co_return;
    }

    // ========================================
    // Publishing API
    // ========================================

    core::async_result<bool> publish(
        const std::string& topic,
        const ss::message& message,
        const core::endpoint& sender,
        std::uint8_t priority,
        const std::unordered_set<std::string>& tags
    ) override {
        if (!is_running()) {
            co_return false;
        }
        
        try {
            bus_message bus_msg(topic, message, sender);
            bus_msg.metadata.priority = priority;
            bus_msg.metadata.tags = tags;
            bus_msg.metadata.expires_at = std::chrono::steady_clock::now() + config_.message_ttl;
            
            co_return co_await publish(bus_msg);
            
        } catch (const std::exception& e) {
            logger_->error("Publish error: " + std::string(e.what()));
            co_return false;
        }
    }

    core::async_result<bool> publish(const bus_message& msg) override {
        if (!is_running()) {
            co_return false;
        }
        
        try {
            std::unique_lock lock(mutex_);
            
            // Get or create topic queue
            auto& queue = get_or_create_topic_queue(msg.metadata.topic);
            
            // Enqueue message
            bool success = queue.enqueue(msg, config_.backpressure_policy);
            
            if (success) {
                stats_.messages_published++;
                stats_.current_queue_size++;
                if (stats_.current_queue_size > stats_.peak_queue_size) {
                    stats_.peak_queue_size = stats_.current_queue_size;
                }
                
                // Wake up workers
                {
                    std::unique_lock worker_lock(worker_mutex_);
                    worker_condition_.notify_one();
                }
                
                logger_->debug("Published message to topic: " + msg.metadata.topic);
            } else {
                stats_.messages_dropped++;
                logger_->warn("Dropped message for topic: " + msg.metadata.topic);
            }
            
            co_return success;
            
        } catch (const std::exception& e) {
            logger_->error("Publish error: " + std::string(e.what()));
            stats_.messages_dropped++;
            co_return false;
        }
    }

    core::async_result<bool> publish_with_timeout(
        const std::string& topic,
        const ss::message& message,
        const core::endpoint& sender,
        std::chrono::milliseconds timeout,
        std::uint8_t priority
    ) override {
        if (!is_running()) {
            co_return false;
        }
        
        try {
            // Create timer for timeout
            boost::asio::steady_timer timer(io_context_, timeout);
            bool timed_out = false;
            
            timer.async_wait([&timed_out](const boost::system::error_code& ec) {
                if (!ec) {
                    timed_out = true;
                }
            });
            
            // Publish message
            auto result = co_await publish(topic, message, sender, priority);
            
            timer.cancel();
            
            if (timed_out) {
                logger_->warn("Publish timeout for topic: " + topic);
                co_return false;
            }
            
            co_return result;
            
        } catch (const std::exception& e) {
            logger_->error("Publish with timeout error: " + std::string(e.what()));
            co_return false;
        }
    }

    // ========================================
    // Subscription API
    // ========================================

    std::uint64_t subscribe(
        const std::string& topic_pattern,
        message_subscriber subscriber,
        const std::unordered_set<std::string>& required_tags,
        const std::unordered_set<std::string>& excluded_tags,
        std::uint8_t min_priority
    ) override {
        std::unique_lock lock(mutex_);
        
        auto subscription_id = next_subscription_id_++;
        
        subscription_info info;
        info.id = subscription_id;
        info.topic_pattern = topic_pattern;
        info.required_tags = required_tags;
        info.excluded_tags = excluded_tags;
        info.min_priority = min_priority;
        info.created_at = std::chrono::steady_clock::now();
        
        subscriptions_[subscription_id] = {info, std::move(subscriber)};
        stats_.active_subscriptions++;
        
        logger_->info("Added subscription " + std::to_string(subscription_id) + 
                     " for pattern: " + topic_pattern);
        
        return subscription_id;
    }

    std::uint64_t subscribe_with_filter(
        const std::string& topic_pattern,
        message_subscriber subscriber,
        message_filter filter
    ) override {
        std::unique_lock lock(mutex_);
        
        auto subscription_id = next_subscription_id_++;
        
        subscription_info info;
        info.id = subscription_id;
        info.topic_pattern = topic_pattern;
        info.created_at = std::chrono::steady_clock::now();
        
        subscriptions_[subscription_id] = {info, std::move(subscriber), std::move(filter)};
        stats_.active_subscriptions++;
        
        logger_->info("Added filtered subscription " + std::to_string(subscription_id) + 
                     " for pattern: " + topic_pattern);
        
        return subscription_id;
    }

    bool unsubscribe(std::uint64_t subscription_id) override {
        std::unique_lock lock(mutex_);
        
        auto it = subscriptions_.find(subscription_id);
        if (it != subscriptions_.end()) {
            subscriptions_.erase(it);
            stats_.active_subscriptions--;
            
            logger_->info("Removed subscription: " + std::to_string(subscription_id));
            return true;
        }
        
        return false;
    }

    std::size_t unsubscribe_topic(const std::string& topic_pattern) override {
        std::unique_lock lock(mutex_);
        
        std::size_t removed = 0;
        auto it = subscriptions_.begin();
        
        while (it != subscriptions_.end()) {
            if (it->second.info.topic_pattern == topic_pattern) {
                it = subscriptions_.erase(it);
                removed++;
            } else {
                ++it;
            }
        }
        
        stats_.active_subscriptions -= removed;
        
        logger_->info("Removed " + std::to_string(removed) + 
                     " subscriptions for pattern: " + topic_pattern);
        
        return removed;
    }

    std::optional<subscription_info> get_subscription(std::uint64_t subscription_id) const override {
        std::shared_lock lock(mutex_);
        
        auto it = subscriptions_.find(subscription_id);
        if (it != subscriptions_.end()) {
            return it->second.info;
        }
        
        return std::nullopt;
    }

    std::vector<subscription_info> get_subscriptions() const override {
        std::shared_lock lock(mutex_);
        
        std::vector<subscription_info> result;
        result.reserve(subscriptions_.size());
        
        for (const auto& [id, subscription] : subscriptions_) {
            result.push_back(subscription.info);
        }
        
        return result;
    }

    // ========================================
    // Topic Management API
    // ========================================

    bool create_topic(const std::string& topic, std::size_t max_queue_size) override {
        std::unique_lock lock(mutex_);
        
        if (topic_queues_.find(topic) != topic_queues_.end()) {
            return false;
        }
        
        auto queue_size = max_queue_size > 0 ? max_queue_size : config_.max_queue_size;
        topic_queues_[topic] = std::make_unique<topic_queue>(queue_size);
        stats_.active_topics++;
        
        logger_->info("Created topic: " + topic + " with max size: " + std::to_string(queue_size));
        
        return true;
    }

    bool delete_topic(const std::string& topic) override {
        std::unique_lock lock(mutex_);
        
        auto it = topic_queues_.find(topic);
        if (it != topic_queues_.end()) {
            auto cleared_count = it->second->size();
            topic_queues_.erase(it);
            stats_.active_topics--;
            stats_.current_queue_size -= cleared_count;
            
            logger_->info("Deleted topic: " + topic + ", cleared " + 
                         std::to_string(cleared_count) + " messages");
            return true;
        }
        
        return false;
    }

    bool topic_exists(const std::string& topic) const override {
        std::shared_lock lock(mutex_);
        return topic_queues_.find(topic) != topic_queues_.end();
    }

    std::vector<std::string> get_topics() const override {
        std::shared_lock lock(mutex_);
        
        std::vector<std::string> topics;
        topics.reserve(topic_queues_.size());
        
        for (const auto& [topic, queue] : topic_queues_) {
            topics.push_back(topic);
        }
        
        return topics;
    }

    std::size_t get_topic_message_count(const std::string& topic) const override {
        std::shared_lock lock(mutex_);
        
        auto it = topic_queues_.find(topic);
        if (it != topic_queues_.end()) {
            return it->second->size();
        }
        
        return 0;
    }

    std::size_t clear_topic(const std::string& topic) override {
        std::unique_lock lock(mutex_);
        
        auto it = topic_queues_.find(topic);
        if (it != topic_queues_.end()) {
            auto cleared_count = it->second->size();
            it->second->clear();
            stats_.current_queue_size -= cleared_count;
            
            logger_->info("Cleared topic: " + topic + ", removed " + 
                         std::to_string(cleared_count) + " messages");
            return cleared_count;
        }
        
        return 0;
    }

    // ========================================
    // Queue Management API
    // ========================================

    std::size_t get_queue_size() const override {
        std::shared_lock lock(mutex_);
        
        std::size_t total_size = 0;
        for (const auto& [topic, queue] : topic_queues_) {
            total_size += queue->size();
        }
        
        return total_size;
    }

    std::size_t get_max_queue_size() const override {
        std::shared_lock lock(mutex_);
        return config_.max_queue_size;
    }

    void set_max_queue_size(std::size_t max_size) override {
        std::unique_lock lock(mutex_);
        config_.max_queue_size = max_size;
        
        logger_->info("Updated max queue size to: " + std::to_string(max_size));
    }

    std::size_t clear_all_queues() override {
        std::unique_lock lock(mutex_);
        
        std::size_t total_cleared = 0;
        
        for (auto& [topic, queue] : topic_queues_) {
            total_cleared += queue->size();
            queue->clear();
        }
        
        stats_.current_queue_size = 0;
        
        logger_->info("Cleared all queues, removed " + std::to_string(total_cleared) + " messages");
        
        return total_cleared;
    }

    // ========================================
    // Dead Letter Queue API
    // ========================================

    void set_dead_letter_handler(dead_letter_handler handler) override {
        std::unique_lock lock(mutex_);
        dead_letter_handler_ = std::move(handler);
    }

    std::size_t get_dead_letter_queue_size() const override {
        std::shared_lock lock(mutex_);
        return dead_letter_queue_.size();
    }

    std::vector<bus_message> get_dead_letter_messages(std::size_t max_messages) const override {
        std::shared_lock lock(mutex_);
        
        std::vector<bus_message> messages;
        messages.reserve(std::min(max_messages, dead_letter_queue_.size()));
        
        auto it = dead_letter_queue_.begin();
        for (std::size_t i = 0; i < max_messages && it != dead_letter_queue_.end(); ++i, ++it) {
            messages.push_back(*it);
        }
        
        return messages;
    }

    std::size_t clear_dead_letter_queue() override {
        std::unique_lock lock(mutex_);
        
        auto cleared_count = dead_letter_queue_.size();
        dead_letter_queue_.clear();
        
        logger_->info("Cleared dead letter queue, removed " + 
                     std::to_string(cleared_count) + " messages");
        
        return cleared_count;
    }

    core::async_result<bool> retry_dead_letter_message(const core::message_id& message_id) override {
        std::unique_lock lock(mutex_);
        
        auto it = std::find_if(dead_letter_queue_.begin(), dead_letter_queue_.end(),
            [&message_id](const bus_message& msg) {
                return msg.metadata.id == message_id;
            });
        
        if (it != dead_letter_queue_.end()) {
            auto message = *it;
            dead_letter_queue_.erase(it);
            
            // Reset delivery attempts
            message.metadata.delivery_attempts = 0;
            
            lock.unlock();
            
            auto result = co_await publish(message);
            
            logger_->info("Retried dead letter message: " + std::to_string(message_id.value()));
            
            co_return result;
        }
        
        co_return false;
    }

    // ========================================
    // Statistics and Monitoring API
    // ========================================

    message_bus_stats get_stats() const override {
        std::shared_lock lock(mutex_);
        
        auto current_stats = stats_;
        current_stats.current_queue_size = get_queue_size();
        current_stats.active_subscriptions = subscriptions_.size();
        current_stats.active_topics = topic_queues_.size();
        current_stats.dead_letter_messages = dead_letter_queue_.size();
        
        // Calculate message throughput
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - stats_.start_time);
        if (uptime.count() > 0) {
            current_stats.message_throughput = static_cast<double>(stats_.messages_delivered) / uptime.count();
        }
        
        return current_stats;
    }

    void reset_stats() override {
        std::unique_lock lock(mutex_);
        
        stats_ = message_bus_stats{};
        stats_.start_time = std::chrono::steady_clock::now();
        
        logger_->info("Reset statistics");
    }

    // ========================================
    // Persistence API
    // ========================================

    core::async_result<bool> save_state(const std::string& file_path) override {
        try {
            std::string path = file_path.empty() ? config_.persistence_path : file_path;
            
            nlohmann::json state_json;
            
            // Save configuration
            state_json["config"] = config_.to_json();
            
            // Save subscriptions
            {
                std::shared_lock lock(mutex_);
                state_json["subscriptions"] = nlohmann::json::array();
                for (const auto& [id, subscription] : subscriptions_) {
                    state_json["subscriptions"].push_back(subscription.info.to_json());
                }
            }
            
            // Save topic queues (message content)
            {
                std::shared_lock lock(mutex_);
                state_json["topics"] = nlohmann::json::object();
                for (const auto& [topic, queue] : topic_queues_) {
                    auto messages = queue->get_all_messages();
                    nlohmann::json message_array = nlohmann::json::array();
                    for (const auto& msg : messages) {
                        // Convert message to JSON (simplified)
                        nlohmann::json msg_json;
                        msg_json["metadata"] = msg.metadata.to_json();
                        // Note: ss::message serialization would need to be implemented
                        message_array.push_back(msg_json);
                    }
                    state_json["topics"][topic] = message_array;
                }
            }
            
            // Write to file
            std::ofstream file(path);
            if (!file.is_open()) {
                logger_->error("Cannot open state file for writing: " + path);
                co_return false;
            }
            
            file << state_json.dump(2);
            file.close();
            
            logger_->info("Saved state to: " + path);
            co_return true;
            
        } catch (const std::exception& e) {
            logger_->error("Save state error: " + std::string(e.what()));
            co_return false;
        }
    }

    core::async_result<bool> load_state(const std::string& file_path) override {
        try {
            std::string path = file_path.empty() ? config_.persistence_path : file_path;
            
            std::ifstream file(path);
            if (!file.is_open()) {
                logger_->warn("State file not found: " + path);
                co_return false;
            }
            
            nlohmann::json state_json;
            file >> state_json;
            file.close();
            
            // Load configuration (merge with current)
            if (state_json.contains("config")) {
                // Optionally merge saved config with current config
                logger_->info("Loaded configuration from state file");
            }
            
            // Load subscriptions (simplified - would need to restore handlers)
            if (state_json.contains("subscriptions") && state_json["subscriptions"].is_array()) {
                std::unique_lock lock(mutex_);
                for (const auto& sub_json : state_json["subscriptions"]) {
                    // Note: Cannot restore the actual subscriber callbacks from JSON
                    // This would require a different approach for persistence
                    logger_->info("Found subscription in state file (handler not restored)");
                }
            }
            
            // Load topic messages
            if (state_json.contains("topics") && state_json["topics"].is_object()) {
                std::unique_lock lock(mutex_);
                for (const auto& [topic, messages] : state_json["topics"].items()) {
                    if (messages.is_array()) {
                        create_topic(topic);
                        logger_->info("Restored topic: " + topic + " with " + 
                                    std::to_string(messages.size()) + " messages");
                        // Note: Actual message restoration would require proper deserialization
                    }
                }
            }
            
            logger_->info("Loaded state from: " + path);
            co_return true;
            
        } catch (const std::exception& e) {
            logger_->error("Load state error: " + std::string(e.what()));
            co_return false;
        }
    }

    // ========================================
    // Configuration API
    // ========================================

    core::result<void, std::string> configure(const message_bus_config& config) override {
        auto validation_result = config.validate();
        if (!validation_result.is_ok()) {
            return validation_result;
        }
        
        std::unique_lock lock(mutex_);
        config_ = config;
        
        return core::result<void, std::string>::ok();
    }

    message_bus_config get_config() const override {
        std::shared_lock lock(mutex_);
        return config_;
    }

private:
    /**
     * @brief Subscription data
     */
    struct subscription_data {
        subscription_info info;
        message_subscriber subscriber;
        std::optional<message_filter> filter;
        
        subscription_data(subscription_info info, message_subscriber subscriber)
            : info(std::move(info)), subscriber(std::move(subscriber)) {}
        
        subscription_data(subscription_info info, message_subscriber subscriber, message_filter filter)
            : info(std::move(info)), subscriber(std::move(subscriber)), filter(std::move(filter)) {}
    };

    /**
     * @brief Get or create topic queue
     */
    topic_queue& get_or_create_topic_queue(const std::string& topic) {
        auto it = topic_queues_.find(topic);
        if (it == topic_queues_.end()) {
            auto [new_it, inserted] = topic_queues_.emplace(
                topic, std::make_unique<topic_queue>(config_.max_queue_size)
            );
            if (inserted) {
                stats_.active_topics++;
            }
            return *new_it->second;
        }
        return *it->second;
    }

    /**
     * @brief Check if topic matches pattern (supports wildcards)
     */
    bool topic_matches_pattern(const std::string& topic, const std::string& pattern) const {
        // Simple wildcard matching: * matches any sequence, ? matches single char
        std::string regex_pattern = pattern;
        
        // Escape special regex characters except * and ?
        regex_pattern = std::regex_replace(regex_pattern, std::regex(R"([\.\+\^\$\(\)\[\]\{\}\\|\-])"), R"(\$&)");
        
        // Convert wildcards to regex
        regex_pattern = std::regex_replace(regex_pattern, std::regex(R"(\*)"), ".*");
        regex_pattern = std::regex_replace(regex_pattern, std::regex(R"(\?)"), ".");
        
        std::regex topic_regex("^" + regex_pattern + "$");
        return std::regex_match(topic, topic_regex);
    }

    /**
     * @brief Worker thread function
     */
    void worker_thread() {
        logger_->debug("Worker thread started");
        
        while (running_.load()) {
            try {
                process_messages();
                
                // Wait for new messages or shutdown signal
                std::unique_lock lock(worker_mutex_);
                worker_condition_.wait_for(lock, std::chrono::milliseconds(100), [this] {
                    return !running_.load() || has_pending_messages();
                });
                
            } catch (const std::exception& e) {
                logger_->error("Worker thread error: " + std::string(e.what()));
            }
        }
        
        logger_->debug("Worker thread stopped");
    }

    /**
     * @brief Check if there are pending messages
     */
    bool has_pending_messages() const {
        std::shared_lock lock(mutex_);
        for (const auto& [topic, queue] : topic_queues_) {
            if (queue->size() > 0) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Process pending messages
     */
    void process_messages() {
        std::vector<std::pair<std::string, bus_message>> messages_to_process;
        
        // Collect messages from all topics
        {
            std::shared_lock lock(mutex_);
            for (const auto& [topic, queue] : topic_queues_) {
                auto message = queue->dequeue();
                if (message) {
                    messages_to_process.emplace_back(topic, std::move(*message));
                }
            }
        }
        
        // Process collected messages
        for (const auto& [topic, message] : messages_to_process) {
            process_single_message(message);
        }
    }

    /**
     * @brief Process a single message
     */
    void process_single_message(const bus_message& message) {
        auto start_time = std::chrono::steady_clock::now();
        
        try {
            std::vector<std::uint64_t> matching_subscriptions;
            
            // Find matching subscriptions
            {
                std::shared_lock lock(mutex_);
                for (const auto& [id, subscription] : subscriptions_) {
                    if (subscription_matches_message(subscription, message)) {
                        matching_subscriptions.push_back(id);
                    }
                }
            }
            
            // Deliver to subscribers
            std::size_t successful_deliveries = 0;
            for (auto subscription_id : matching_subscriptions) {
                if (deliver_to_subscription(subscription_id, message)) {
                    successful_deliveries++;
                }
            }
            
            if (successful_deliveries > 0) {
                stats_.messages_delivered++;
                stats_.current_queue_size--;
                
                // Update latency statistics
                auto end_time = std::chrono::steady_clock::now();
                auto latency = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
                update_latency_stats(latency);
                
            } else {
                // No subscribers - could be considered dropped or moved to dead letter queue
                handle_undelivered_message(message, "No matching subscribers");
            }
            
        } catch (const std::exception& e) {
            logger_->error("Message processing error: " + std::string(e.what()));
            handle_undelivered_message(message, "Processing error: " + std::string(e.what()));
        }
    }

    /**
     * @brief Check if subscription matches message
     */
    bool subscription_matches_message(const subscription_data& subscription, const bus_message& message) const {
        // Check topic pattern
        if (!topic_matches_pattern(message.metadata.topic, subscription.info.topic_pattern)) {
            return false;
        }
        
        // Check priority
        if (message.metadata.priority < subscription.info.min_priority) {
            return false;
        }
        
        // Check required tags
        for (const auto& required_tag : subscription.info.required_tags) {
            if (message.metadata.tags.find(required_tag) == message.metadata.tags.end()) {
                return false;
            }
        }
        
        // Check excluded tags
        for (const auto& excluded_tag : subscription.info.excluded_tags) {
            if (message.metadata.tags.find(excluded_tag) != message.metadata.tags.end()) {
                return false;
            }
        }
        
        // Check custom filter
        if (subscription.filter && !(*subscription.filter)(message)) {
            return false;
        }
        
        return true;
    }

    /**
     * @brief Deliver message to subscription
     */
    bool deliver_to_subscription(std::uint64_t subscription_id, const bus_message& message) {
        try {
            message_subscriber subscriber;
            
            // Get subscriber (copy to avoid holding lock during callback)
            {
                std::shared_lock lock(mutex_);
                auto it = subscriptions_.find(subscription_id);
                if (it == subscriptions_.end()) {
                    return false;
                }
                subscriber = it->second.subscriber;
            }
            
            // Call subscriber asynchronously
            boost::asio::co_spawn(io_context_, 
                [subscriber, message]() -> core::async_result<bool> {
                    return co_await subscriber(message);
                },
                [this, subscription_id](std::exception_ptr eptr, bool success) {
                    if (eptr) {
                        try {
                            std::rethrow_exception(eptr);
                        } catch (const std::exception& e) {
                            logger_->error("Subscriber callback error: " + std::string(e.what()));
                        }
                    } else if (success) {
                        // Update subscription statistics
                        std::unique_lock lock(mutex_);
                        auto it = subscriptions_.find(subscription_id);
                        if (it != subscriptions_.end()) {
                            it->second.info.messages_received++;
                            it->second.info.last_received = std::chrono::steady_clock::now();
                        }
                    }
                }
            );
            
            return true;
            
        } catch (const std::exception& e) {
            logger_->error("Delivery error: " + std::string(e.what()));
            return false;
        }
    }

    /**
     * @brief Handle undelivered message
     */
    void handle_undelivered_message(const bus_message& message, const std::string& reason) {
        // Check if should retry
        if (message.metadata.should_retry()) {
            // Increment delivery attempts and re-queue
            auto retry_message = message;
            retry_message.metadata.delivery_attempts++;
            
            boost::asio::co_spawn(io_context_,
                [this, retry_message]() -> core::async_result<bool> {
                    return co_await publish(retry_message);
                },
                boost::asio::detached
            );
            
        } else {
            // Move to dead letter queue
            std::unique_lock lock(mutex_);
            
            if (dead_letter_queue_.size() >= config_.dead_letter_queue_size) {
                dead_letter_queue_.pop_front();
            }
            
            dead_letter_queue_.push_back(message);
            stats_.messages_dropped++;
            
            // Call dead letter handler if available
            if (dead_letter_handler_) {
                try {
                    dead_letter_handler_(message, reason);
                } catch (const std::exception& e) {
                    logger_->error("Dead letter handler error: " + std::string(e.what()));
                }
            }
            
            logger_->warn("Message moved to dead letter queue: " + reason);
        }
    }

    /**
     * @brief Update latency statistics
     */
    void update_latency_stats(std::chrono::microseconds latency) {
        // Simple moving average (in production, you might want a more sophisticated approach)
        stats_.avg_latency = std::chrono::microseconds(
            (stats_.avg_latency.count() + latency.count()) / 2
        );
    }

    /**
     * @brief Schedule periodic message processing
     */
    void schedule_message_processing() {
        if (!running_.load()) {
            return;
        }
        
        processing_timer_.expires_after(std::chrono::milliseconds(10));
        processing_timer_.async_wait([this](const boost::system::error_code& ec) {
            if (!ec && running_.load()) {
                // Wake up workers
                {
                    std::unique_lock lock(worker_mutex_);
                    worker_condition_.notify_all();
                }
                schedule_message_processing();
            }
        });
    }

    /**
     * @brief Schedule periodic health check
     */
    void schedule_health_check() {
        if (!running_.load()) {
            return;
        }
        
        health_check_timer_.expires_after(std::chrono::seconds(30));
        health_check_timer_.async_wait([this](const boost::system::error_code& ec) {
            if (!ec && running_.load()) {
                perform_health_check();
                schedule_health_check();
            }
        });
    }

    /**
     * @brief Perform health check
     */
    void perform_health_check() {
        try {
            auto current_stats = get_stats();
            
            // Log health information
            logger_->debug("Health check - Queue size: " + std::to_string(current_stats.current_queue_size) +
                          "/" + std::to_string(config_.max_queue_size) +
                          ", Topics: " + std::to_string(current_stats.active_topics) +
                          ", Subscriptions: " + std::to_string(current_stats.active_subscriptions));
            
            // Check for potential issues
            if (current_stats.current_queue_size > config_.max_queue_size * 0.8) {
                logger_->warn("Queue size approaching limit: " + 
                             std::to_string(current_stats.current_queue_size));
            }
            
            if (current_stats.dead_letter_messages > config_.dead_letter_queue_size * 0.8) {
                logger_->warn("Dead letter queue approaching limit: " + 
                             std::to_string(current_stats.dead_letter_messages));
            }
            
        } catch (const std::exception& e) {
            logger_->error("Health check error: " + std::string(e.what()));
        }
    }

    // Core dependencies
    boost::asio::io_context& io_context_;
    
    // Configuration and state
    mutable std::shared_mutex mutex_;
    message_bus_config config_;
    message_bus_stats stats_;
    std::atomic<bool> running_;
    
    // Topic management
    std::unordered_map<std::string, std::unique_ptr<topic_queue>> topic_queues_;
    
    // Subscription management
    std::unordered_map<std::uint64_t, subscription_data> subscriptions_;
    std::atomic<std::uint64_t> next_subscription_id_;
    
    // Dead letter queue
    std::deque<bus_message> dead_letter_queue_;
    dead_letter_handler dead_letter_handler_;
    
    // Worker threads
    mutable std::mutex worker_mutex_;
    std::condition_variable worker_condition_;
    std::vector<std::thread> worker_threads_;
    
    // Timers
    boost::asio::steady_timer processing_timer_;
    boost::asio::steady_timer health_check_timer_;
    
    // Logging
    std::shared_ptr<ss_logger> logger_;
};

// ========================================
// Message structures implementation
// ========================================

bus_message::bus_message(const std::string& topic, const ss::message& msg, const core::endpoint& sender)
    : payload(msg) {
    metadata.id = core::message_id::generate();
    metadata.topic = topic;
    metadata.sender = sender;
    metadata.timestamp = std::chrono::steady_clock::now();
    metadata.expires_at = metadata.timestamp + std::chrono::minutes(5); // Default TTL
}

bus_message::bus_message(const message_metadata& meta, const ss::message& msg)
    : metadata(meta), payload(msg) {
}

std::vector<std::uint8_t> bus_message::serialize() const {
    // Simplified serialization - in practice, you'd want a more robust format
    nlohmann::json json;
    json["metadata"] = metadata.to_json();
    // Note: ss::message serialization would need to be implemented
    
    std::string json_str = json.dump();
    return std::vector<std::uint8_t>(json_str.begin(), json_str.end());
}

core::result<bus_message, std::string> bus_message::deserialize(const std::vector<std::uint8_t>& data) {
    try {
        std::string json_str(data.begin(), data.end());
        nlohmann::json json = nlohmann::json::parse(json_str);
        
        // This is a simplified implementation
        // In practice, you'd need proper message deserialization
        
        return core::result<bus_message, std::string>::error("Deserialization not fully implemented");
        
    } catch (const std::exception& e) {
        return core::result<bus_message, std::string>::error(
            "Deserialization error: " + std::string(e.what())
        );
    }
}

std::size_t bus_message::size() const {
    // Estimate size (in practice, you'd calculate this more accurately)
    return metadata.topic.size() + 1024; // Approximate
}

bool message_metadata::is_expired() const {
    return std::chrono::steady_clock::now() >= expires_at;
}

bool message_metadata::should_retry() const {
    return delivery_attempts < max_delivery_attempts && !is_expired();
}

nlohmann::json message_metadata::to_json() const {
    nlohmann::json json;
    
    json["id"] = id.value();
    json["topic"] = topic;
    json["sender"] = {
        {"address", sender.address()},
        {"port", sender.port()}
    };
    json["priority"] = priority;
    json["tags"] = nlohmann::json::array();
    for (const auto& tag : tags) {
        json["tags"].push_back(tag);
    }
    json["delivery_attempts"] = delivery_attempts;
    json["max_delivery_attempts"] = max_delivery_attempts;
    
    return json;
}

bool subscription_info::matches(const bus_message& msg) const {
    // This is implemented in the message_bus_impl class
    return true; // Placeholder
}

nlohmann::json subscription_info::to_json() const {
    nlohmann::json json;
    
    json["id"] = id;
    json["topic_pattern"] = topic_pattern;
    json["required_tags"] = nlohmann::json::array();
    for (const auto& tag : required_tags) {
        json["required_tags"].push_back(tag);
    }
    json["excluded_tags"] = nlohmann::json::array();
    for (const auto& tag : excluded_tags) {
        json["excluded_tags"].push_back(tag);
    }
    json["min_priority"] = min_priority;
    json["messages_received"] = messages_received;
    json["active"] = active;
    
    return json;
}

nlohmann::json message_bus_stats::to_json() const {
    nlohmann::json json;
    
    json["messages_published"] = messages_published;
    json["messages_delivered"] = messages_delivered;
    json["messages_dropped"] = messages_dropped;
    json["dead_letter_messages"] = dead_letter_messages;
    json["active_subscriptions"] = active_subscriptions;
    json["active_topics"] = active_topics;
    json["current_queue_size"] = current_queue_size;
    json["peak_queue_size"] = peak_queue_size;
    json["avg_latency_us"] = avg_latency.count();
    json["message_throughput"] = message_throughput;
    
    return json;
}

void message_bus_stats::reset() {
    *this = message_bus_stats{};
    start_time = std::chrono::steady_clock::now();
}

// ========================================
// Configuration implementation
// ========================================

core::result<void, std::string> message_bus_config::validate() const {
    if (max_queue_size == 0) {
        return core::result<void, std::string>::error("Max queue size must be greater than 0");
    }
    
    if (max_subscribers_per_topic == 0) {
        return core::result<void, std::string>::error("Max subscribers per topic must be greater than 0");
    }
    
    if (message_ttl.count() <= 0) {
        return core::result<void, std::string>::error("Message TTL must be positive");
    }
    
    if (worker_threads == 0) {
        return core::result<void, std::string>::error("Worker threads must be greater than 0");
    }
    
    if (enable_persistence && persistence_path.empty()) {
        return core::result<void, std::string>::error("Persistence path cannot be empty when persistence is enabled");
    }
    
    return core::result<void, std::string>::ok();
}

core::result<message_bus_config, std::string> message_bus_config::from_json(const nlohmann::json& config_json) {
    try {
        message_bus_config config;
        
        if (config_json.contains("max_queue_size")) {
            config.max_queue_size = config_json["max_queue_size"].get<std::size_t>();
        }
        
        if (config_json.contains("message_ttl_ms")) {
            config.message_ttl = std::chrono::milliseconds(
                config_json["message_ttl_ms"].get<std::uint64_t>()
            );
        }
        
        if (config_json.contains("enable_persistence")) {
            config.enable_persistence = config_json["enable_persistence"].get<bool>();
        }
        
        if (config_json.contains("persistence_path")) {
            config.persistence_path = config_json["persistence_path"].get<std::string>();
        }
        
        if (config_json.contains("worker_threads")) {
            config.worker_threads = config_json["worker_threads"].get<std::size_t>();
        }
        
        // Add other configuration loading as needed
        
        auto validation_result = config.validate();
        if (!validation_result.is_ok()) {
            return core::result<message_bus_config, std::string>::error(validation_result.error());
        }
        
        return core::result<message_bus_config, std::string>::ok(std::move(config));
        
    } catch (const std::exception& e) {
        return core::result<message_bus_config, std::string>::error(
            "JSON parsing error: " + std::string(e.what())
        );
    }
}

nlohmann::json message_bus_config::to_json() const {
    nlohmann::json json;
    
    json["max_queue_size"] = max_queue_size;
    json["max_subscribers_per_topic"] = max_subscribers_per_topic;
    json["message_ttl_ms"] = message_ttl.count();
    json["dead_letter_queue_size"] = dead_letter_queue_size;
    json["enable_persistence"] = enable_persistence;
    json["persistence_path"] = persistence_path;
    json["delivery_mode"] = static_cast<int>(delivery_mode);
    json["enable_compression"] = enable_compression;
    json["compression_threshold"] = compression_threshold;
    json["worker_threads"] = worker_threads;
    json["backpressure_policy"] = static_cast<int>(backpressure_policy);
    
    return json;
}

// ========================================
// Factory Functions
// ========================================

std::unique_ptr<i_message_bus> create_message_bus(
    boost::asio::io_context& io_context,
    const message_bus_config& config
) {
    auto bus = std::make_unique<message_bus_impl>(io_context);
    
    if (config.max_queue_size > 0) {
        // Configure immediately if config is provided
        auto configure_result = bus->configure(config);
        if (!configure_result.is_ok()) {
            throw std::invalid_argument("Invalid configuration: " + configure_result.error());
        }
    }
    
    return std::move(bus);
}

// ========================================
// Builder Implementation
// ========================================

message_bus_builder::message_bus_builder(boost::asio::io_context& io_context)
    : io_context_(io_context) {
}

message_bus_builder& message_bus_builder::with_max_queue_size(std::size_t size) {
    config_.max_queue_size = size;
    return *this;
}

message_bus_builder& message_bus_builder::with_message_ttl(std::chrono::milliseconds ttl) {
    config_.message_ttl = ttl;
    return *this;
}

message_bus_builder& message_bus_builder::with_persistence(bool enabled, const std::string& persistence_path) {
    config_.enable_persistence = enabled;
    if (!persistence_path.empty()) {
        config_.persistence_path = persistence_path;
    }
    return *this;
}

message_bus_builder& message_bus_builder::with_delivery_mode(message_bus_config::delivery_mode mode) {
    config_.delivery_mode = mode;
    return *this;
}

message_bus_builder& message_bus_builder::with_backpressure_policy(message_bus_config::backpressure_policy policy) {
    config_.backpressure_policy = policy;
    return *this;
}

message_bus_builder& message_bus_builder::with_compression(bool enabled, std::size_t threshold) {
    config_.enable_compression = enabled;
    config_.compression_threshold = threshold;
    return *this;
}

message_bus_builder& message_bus_builder::with_worker_threads(std::size_t threads) {
    config_.worker_threads = threads;
    return *this;
}

message_bus_builder& message_bus_builder::with_config_file(const std::string& config_path) {
    try {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open config file: " + config_path);
        }
        
        nlohmann::json config_json;
        file >> config_json;
        
        auto config_result = message_bus_config::from_json(config_json);
        if (!config_result.is_ok()) {
            throw std::runtime_error("Invalid config file: " + config_result.error());
        }
        
        config_ = config_result.value();
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Error loading config file: " + std::string(e.what()));
    }
    
    return *this;
}

std::unique_ptr<i_message_bus> message_bus_builder::build() {
    auto validation_result = config_.validate();
    if (!validation_result.is_ok()) {
        throw std::invalid_argument("Invalid configuration: " + validation_result.error());
    }
    
    return create_message_bus(io_context_, config_);
}

} // namespace ss::application