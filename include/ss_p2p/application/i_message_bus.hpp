#pragma once

#include "../core/interfaces.hpp"
#include "../core/types.hpp"
#include "../core/result.hpp"
#include "../message.hpp"

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <chrono>
#include <atomic>
#include <boost/asio.hpp>
#include <json.hpp>

namespace ss::application {

/**
 * @brief Message bus configuration
 * 
 * Contains configuration parameters for message bus operation,
 * including queue sizes, timeouts, and delivery policies.
 */
struct message_bus_config {
    /// Maximum number of queued messages per topic
    std::size_t max_queue_size = 10000;
    
    /// Maximum number of subscribers per topic
    std::size_t max_subscribers_per_topic = 1000;
    
    /// Message TTL (time to live) in milliseconds
    std::chrono::milliseconds message_ttl{300000}; // 5 minutes
    
    /// Dead letter queue size
    std::size_t dead_letter_queue_size = 1000;
    
    /// Enable message persistence
    bool enable_persistence = false;
    
    /// Persistence file path (if enabled)
    std::string persistence_path = "./message_bus.dat";
    
    /// Delivery mode enum
    enum class delivery_mode {
        at_most_once,   // Fire and forget
        at_least_once,  // May deliver duplicates
        exactly_once    // Guaranteed single delivery (requires persistence)
    };
    /// Delivery mode setting
    delivery_mode delivery = delivery_mode::at_least_once;
    
    /// Message compression
    bool enable_compression = false;
    
    /// Compression threshold (bytes)
    std::size_t compression_threshold = 1024;
    
    /// Worker thread count for async processing
    std::size_t worker_threads = 4;
    
    /// Backpressure policy enum
    enum class backpressure_policy {
        drop_oldest,    // Drop oldest messages when queue is full
        drop_newest,    // Drop newest messages when queue is full
        block_sender,   // Block sender until queue has space
        reject_message  // Reject new messages when queue is full
    };
    /// Backpressure policy setting
    backpressure_policy backpressure = backpressure_policy::drop_oldest;
    
    /**
     * @brief Validate configuration
     * @return Result indicating success or validation error
     */
    core::result<void, std::string> validate() const;
    
    /**
     * @brief Load configuration from JSON
     * @param config_json JSON configuration object
     * @return Result containing loaded config or error
     */
    static core::result<message_bus_config, std::string> from_json(const nlohmann::json& config_json);
    
    /**
     * @brief Convert configuration to JSON
     * @return JSON representation of configuration
     */
    nlohmann::json to_json() const;
};

/**
 * @brief Message metadata for routing and delivery
 */
struct message_metadata {
    /// Message unique identifier
    core::message_id id;
    
    /// Topic name
    std::string topic;
    
    /// Sender endpoint
    core::endpoint sender;
    
    /// Message timestamp
    std::chrono::steady_clock::time_point timestamp;
    
    /// Message priority (higher = more priority)
    std::uint8_t priority = 0;
    
    /// Message tags for filtering
    std::unordered_set<std::string> tags;
    
    /// Message expiration time
    std::chrono::steady_clock::time_point expires_at;
    
    /// Delivery attempt count
    std::uint32_t delivery_attempts = 0;
    
    /// Maximum delivery attempts
    std::uint32_t max_delivery_attempts = 3;
    
    /**
     * @brief Check if message has expired
     * @return true if expired, false otherwise
     */
    bool is_expired() const;
    
    /**
     * @brief Check if message should be retried
     * @return true if should retry, false otherwise
     */
    bool should_retry() const;
    
    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const;
};

/**
 * @brief Wrapped message with metadata
 */
struct bus_message {
    /// Message metadata
    message_metadata metadata;
    
    /// Actual message payload
    ss::message payload;
    
    /**
     * @brief Constructor
     * @param topic Message topic
     * @param msg Message payload
     * @param sender Sender endpoint
     */
    bus_message(const std::string& topic, const ss::message& msg, const core::endpoint& sender);
    
    /**
     * @brief Constructor with metadata
     * @param meta Message metadata
     * @param msg Message payload
     */
    bus_message(const message_metadata& meta, const ss::message& msg);
    
    /**
     * @brief Serialize to bytes
     * @return Serialized data
     */
    std::vector<std::uint8_t> serialize() const;
    
    /**
     * @brief Deserialize from bytes
     * @param data Serialized data
     * @return Deserialized message or error
     */
    static core::result<bus_message, std::string> deserialize(const std::vector<std::uint8_t>& data);
    
    /**
     * @brief Get estimated size in bytes
     * @return Size estimate
     */
    std::size_t size() const;
};

/**
 * @brief Message subscription information
 */
struct subscription_info {
    /// Subscription unique identifier
    std::uint64_t id;
    
    /// Topic pattern (supports wildcards)
    std::string topic_pattern;
    
    /// Message filters
    std::unordered_set<std::string> required_tags;
    std::unordered_set<std::string> excluded_tags;
    
    /// Minimum message priority
    std::uint8_t min_priority = 0;
    
    /// Subscription created timestamp
    std::chrono::steady_clock::time_point created_at;
    
    /// Last message received timestamp
    std::chrono::steady_clock::time_point last_received;
    
    /// Messages received count
    std::uint64_t messages_received = 0;
    
    /// Subscription active flag
    bool active = true;
    
    /**
     * @brief Check if message matches subscription
     * @param msg Message to check
     * @return true if matches, false otherwise
     */
    bool matches(const bus_message& msg) const;
    
    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const;
};

/**
 * @brief Message bus statistics
 */
struct message_bus_stats {
    /// Total messages published
    std::uint64_t messages_published = 0;
    
    /// Total messages delivered
    std::uint64_t messages_delivered = 0;
    
    /// Total messages dropped
    std::uint64_t messages_dropped = 0;
    
    /// Messages in dead letter queue
    std::uint64_t dead_letter_messages = 0;
    
    /// Active subscriptions count
    std::uint64_t active_subscriptions = 0;
    
    /// Active topics count
    std::uint64_t active_topics = 0;
    
    /// Current queue size
    std::uint64_t current_queue_size = 0;
    
    /// Peak queue size since start
    std::uint64_t peak_queue_size = 0;
    
    /// Average message latency
    std::chrono::microseconds avg_latency{0};
    
    /// Message throughput (messages per second)
    double message_throughput = 0.0;
    
    /// Bus uptime
    std::chrono::steady_clock::time_point start_time;
    
    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const;
    
    /**
     * @brief Reset counters
     */
    void reset();
};

/**
 * @brief Message subscriber callback type
 * 
 * Callback function that receives messages matching a subscription.
 * Should return true if message was handled successfully, false otherwise.
 */
using message_subscriber = std::function<core::async_result<bool>(const bus_message&)>;

/**
 * @brief Message filter function type
 * 
 * Function that determines if a message should be delivered to a subscriber.
 */
using message_filter = std::function<bool(const bus_message&)>;

/**
 * @brief Dead letter handler callback type
 * 
 * Callback function that handles messages that couldn't be delivered.
 */
using dead_letter_handler = std::function<void(const bus_message&, const std::string& reason)>;

/**
 * @brief Interface for message bus operations
 * 
 * Provides publish/subscribe messaging with topics, filtering, and
 * reliable delivery. Supports async operation with backpressure control
 * and message persistence.
 */
class i_message_bus : public core::i_component,
                      public core::i_configurable<message_bus_config> {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~i_message_bus() = default;

    // ========================================
    // Lifecycle Management
    // ========================================

    /**
     * @brief Initialize message bus with configuration
     * @param config Bus configuration
     * @return Awaitable void result
     */
    virtual core::async_void initialize_bus(const message_bus_config& config) = 0;

    /**
     * @brief Start message bus operation
     * @return Awaitable void result
     */
    virtual core::async_void start() override = 0;

    /**
     * @brief Stop message bus operation
     * @return Awaitable void result
     */
    virtual core::async_void stop() override = 0;

    // ========================================
    // Publishing API
    // ========================================

    /**
     * @brief Publish message to a topic
     * @param topic Topic name
     * @param message Message to publish
     * @param sender Sender endpoint
     * @param priority Message priority (0-255, higher = more priority)
     * @param tags Message tags for filtering
     * @return Awaitable result with success indication
     */
    virtual core::async_result<bool> publish(
        const std::string& topic,
        const ss::message& message,
        const core::endpoint& sender,
        std::uint8_t priority = 0,
        const std::unordered_set<std::string>& tags = {}
    ) = 0;

    /**
     * @brief Publish message with metadata
     * @param msg Complete bus message with metadata
     * @return Awaitable result with success indication
     */
    virtual core::async_result<bool> publish(const bus_message& msg) = 0;

    /**
     * @brief Publish message with timeout
     * @param topic Topic name
     * @param message Message to publish
     * @param sender Sender endpoint
     * @param timeout Publish timeout
     * @param priority Message priority
     * @return Awaitable result with success indication
     */
    virtual core::async_result<bool> publish_with_timeout(
        const std::string& topic,
        const ss::message& message,
        const core::endpoint& sender,
        std::chrono::milliseconds timeout,
        std::uint8_t priority = 0
    ) = 0;

    // ========================================
    // Subscription API
    // ========================================

    /**
     * @brief Subscribe to messages on a topic
     * @param topic_pattern Topic pattern (supports wildcards like "chat.*", "user.*.status")
     * @param subscriber Callback function for received messages
     * @param required_tags Tags that must be present (AND condition)
     * @param excluded_tags Tags that must not be present
     * @param min_priority Minimum message priority
     * @return Subscription ID for later unsubscription
     */
    virtual std::uint64_t subscribe(
        const std::string& topic_pattern,
        message_subscriber subscriber,
        const std::unordered_set<std::string>& required_tags = {},
        const std::unordered_set<std::string>& excluded_tags = {},
        std::uint8_t min_priority = 0
    ) = 0;

    /**
     * @brief Subscribe with custom filter
     * @param topic_pattern Topic pattern
     * @param subscriber Callback function
     * @param filter Custom filter function
     * @return Subscription ID
     */
    virtual std::uint64_t subscribe_with_filter(
        const std::string& topic_pattern,
        message_subscriber subscriber,
        message_filter filter
    ) = 0;

    /**
     * @brief Unsubscribe from messages
     * @param subscription_id Subscription ID returned by subscribe
     * @return true if unsubscribed, false if subscription not found
     */
    virtual bool unsubscribe(std::uint64_t subscription_id) = 0;

    /**
     * @brief Unsubscribe all subscriptions for a topic pattern
     * @param topic_pattern Topic pattern to unsubscribe from
     * @return Number of subscriptions removed
     */
    virtual std::size_t unsubscribe_topic(const std::string& topic_pattern) = 0;

    /**
     * @brief Get subscription information
     * @param subscription_id Subscription ID
     * @return Subscription info if found
     */
    virtual std::optional<subscription_info> get_subscription(std::uint64_t subscription_id) const = 0;

    /**
     * @brief Get all active subscriptions
     * @return Vector of subscription information
     */
    virtual std::vector<subscription_info> get_subscriptions() const = 0;

    // ========================================
    // Topic Management API
    // ========================================

    /**
     * @brief Create topic explicitly
     * @param topic Topic name
     * @param max_queue_size Maximum queue size for this topic (0 = use default)
     * @return true if created, false if already exists
     */
    virtual bool create_topic(const std::string& topic, std::size_t max_queue_size = 0) = 0;

    /**
     * @brief Delete topic and all its queued messages
     * @param topic Topic name
     * @return true if deleted, false if not found
     */
    virtual bool delete_topic(const std::string& topic) = 0;

    /**
     * @brief Check if topic exists
     * @param topic Topic name
     * @return true if exists, false otherwise
     */
    virtual bool topic_exists(const std::string& topic) const = 0;

    /**
     * @brief Get all active topics
     * @return Vector of topic names
     */
    virtual std::vector<std::string> get_topics() const = 0;

    /**
     * @brief Get message count for a topic
     * @param topic Topic name
     * @return Number of queued messages, 0 if topic doesn't exist
     */
    virtual std::size_t get_topic_message_count(const std::string& topic) const = 0;

    /**
     * @brief Clear all messages from a topic
     * @param topic Topic name
     * @return Number of messages cleared
     */
    virtual std::size_t clear_topic(const std::string& topic) = 0;

    // ========================================
    // Queue Management API
    // ========================================

    /**
     * @brief Get current total queue size
     * @return Total number of queued messages
     */
    virtual std::size_t get_queue_size() const = 0;

    /**
     * @brief Get maximum queue size
     * @return Maximum queue size
     */
    virtual std::size_t get_max_queue_size() const = 0;

    /**
     * @brief Set maximum queue size
     * @param max_size New maximum queue size
     */
    virtual void set_max_queue_size(std::size_t max_size) = 0;

    /**
     * @brief Clear all queues
     * @return Number of messages cleared
     */
    virtual std::size_t clear_all_queues() = 0;

    // ========================================
    // Dead Letter Queue API
    // ========================================

    /**
     * @brief Set dead letter handler
     * @param handler Handler for dead letter messages
     */
    virtual void set_dead_letter_handler(dead_letter_handler handler) = 0;

    /**
     * @brief Get dead letter queue size
     * @return Number of messages in dead letter queue
     */
    virtual std::size_t get_dead_letter_queue_size() const = 0;

    /**
     * @brief Get messages from dead letter queue
     * @param max_messages Maximum number of messages to retrieve
     * @return Vector of dead letter messages
     */
    virtual std::vector<bus_message> get_dead_letter_messages(std::size_t max_messages = 100) const = 0;

    /**
     * @brief Clear dead letter queue
     * @return Number of messages cleared
     */
    virtual std::size_t clear_dead_letter_queue() = 0;

    /**
     * @brief Retry message from dead letter queue
     * @param message_id Message ID to retry
     * @return true if message was retried, false if not found
     */
    virtual core::async_result<bool> retry_dead_letter_message(const core::message_id& message_id) = 0;

    // ========================================
    // Statistics and Monitoring API
    // ========================================

    /**
     * @brief Get message bus statistics
     * @return Current statistics
     */
    virtual message_bus_stats get_stats() const = 0;

    /**
     * @brief Reset statistics counters
     */
    virtual void reset_stats() = 0;

    /**
     * @brief Get health status
     * @return true if healthy, false otherwise
     */
    virtual bool is_healthy() const noexcept override = 0;

    /**
     * @brief Get detailed status information
     * @return Status information as JSON string
     */
    virtual std::string status() const override = 0;

    // ========================================
    // Persistence API
    // ========================================

    /**
     * @brief Save message bus state to disk
     * @param file_path File path to save to (optional, uses config path if not provided)
     * @return Awaitable result with success indication
     */
    virtual core::async_result<bool> save_state(const std::string& file_path = "") = 0;

    /**
     * @brief Load message bus state from disk
     * @param file_path File path to load from (optional, uses config path if not provided)
     * @return Awaitable result with success indication
     */
    virtual core::async_result<bool> load_state(const std::string& file_path = "") = 0;

    // ========================================
    // Configuration API
    // ========================================

    /**
     * @brief Configure message bus
     * @param config New configuration
     * @return Result indicating success or error
     */
    virtual core::result<void, std::string> configure(const message_bus_config& config) override = 0;

    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    virtual message_bus_config get_config() const override = 0;

protected:
    /**
     * @brief Protected default constructor
     */
    i_message_bus() = default;

    /**
     * @brief Protected copy constructor
     */
    i_message_bus(const i_message_bus&) = default;

    /**
     * @brief Protected move constructor
     */
    i_message_bus(i_message_bus&&) = default;

    /**
     * @brief Protected copy assignment
     */
    i_message_bus& operator=(const i_message_bus&) = default;

    /**
     * @brief Protected move assignment
     */
    i_message_bus& operator=(i_message_bus&&) = default;
};

/**
 * @brief Factory function for creating message bus instances
 * @param io_context Boost.Asio IO context
 * @param config Message bus configuration
 * @return Unique pointer to message bus instance
 */
std::unique_ptr<i_message_bus> create_message_bus(
    boost::asio::io_context& io_context,
    const message_bus_config& config = {}
);

/**
 * @brief Builder class for message bus configuration
 * 
 * Provides a fluent interface for configuring message bus with
 * validation and sensible defaults.
 */
class message_bus_builder {
public:
    /**
     * @brief Constructor
     */
    explicit message_bus_builder(boost::asio::io_context& io_context);

    /**
     * @brief Set maximum queue size
     * @param size Maximum queue size
     * @return Reference to builder for chaining
     */
    message_bus_builder& with_max_queue_size(std::size_t size);

    /**
     * @brief Set message TTL
     * @param ttl Message time to live
     * @return Reference to builder for chaining
     */
    message_bus_builder& with_message_ttl(std::chrono::milliseconds ttl);

    /**
     * @brief Enable persistence
     * @param enabled True to enable, false to disable
     * @param persistence_path Path for persistence file
     * @return Reference to builder for chaining
     */
    message_bus_builder& with_persistence(bool enabled, const std::string& persistence_path = "");

    /**
     * @brief Set delivery mode
     * @param mode Delivery mode
     * @return Reference to builder for chaining
     */
    message_bus_builder& with_delivery_mode(message_bus_config::delivery_mode mode);

    /**
     * @brief Set backpressure policy
     * @param policy Backpressure policy
     * @return Reference to builder for chaining
     */
    message_bus_builder& with_backpressure_policy(message_bus_config::backpressure_policy policy);

    /**
     * @brief Enable compression
     * @param enabled True to enable, false to disable
     * @param threshold Compression threshold in bytes
     * @return Reference to builder for chaining
     */
    message_bus_builder& with_compression(bool enabled, std::size_t threshold = 1024);

    /**
     * @brief Set worker thread count
     * @param threads Number of worker threads
     * @return Reference to builder for chaining
     */
    message_bus_builder& with_worker_threads(std::size_t threads);

    /**
     * @brief Load configuration from JSON file
     * @param config_path Path to configuration file
     * @return Reference to builder for chaining
     */
    message_bus_builder& with_config_file(const std::string& config_path);

    /**
     * @brief Build and create the message bus
     * @return Unique pointer to created message bus
     * @throws std::invalid_argument if configuration is invalid
     */
    std::unique_ptr<i_message_bus> build();

private:
    boost::asio::io_context& io_context_;
    message_bus_config config_;
};

} // namespace ss::application