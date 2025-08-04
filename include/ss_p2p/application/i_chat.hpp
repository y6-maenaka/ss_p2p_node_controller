#pragma once

#include "../core/interfaces.hpp"
#include "../core/types.hpp"
#include "../core/result.hpp"
#include "../message.hpp"
#include "i_message_bus.hpp"
#include "i_p2p_node.hpp"

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <optional>
#include <boost/asio.hpp>
#include <json.hpp>

namespace ss::application {

/**
 * @brief Chat configuration
 * 
 * Contains configuration parameters for chat service operation,
 * including encryption settings, message history, and user management.
 */
struct chat_config {
    /// Maximum message length in characters
    std::size_t max_message_length = 4096;
    
    /// Maximum number of messages to keep in history
    std::size_t max_history_size = 10000;
    
    /// Message history retention period
    std::chrono::hours history_retention{24 * 7}; // 7 days
    
    /// Enable end-to-end encryption
    bool enable_encryption = true;
    
    /// Enable message persistence
    bool enable_persistence = true;
    
    /// Persistence file path
    std::string persistence_path = "./chat_history.db";
    
    /// Maximum number of chat rooms
    std::size_t max_chat_rooms = 100;
    
    /// Maximum users per chat room
    std::size_t max_users_per_room = 1000;
    
    /// User nickname length limits
    std::size_t min_nickname_length = 1;
    std::size_t max_nickname_length = 32;
    
    /// Message delivery timeout
    std::chrono::milliseconds message_timeout{30000};
    
    /// Typing indicator timeout
    std::chrono::milliseconds typing_timeout{3000};
    
    /// Presence update interval
    std::chrono::milliseconds presence_interval{30000};
    
    /// Enable typing indicators
    bool enable_typing_indicators = true;
    
    /// Enable user presence
    bool enable_presence = true;
    
    /// Enable message read receipts
    bool enable_read_receipts = true;
    
    /// Enable file transfers
    bool enable_file_transfer = false;
    
    /// Maximum file transfer size (bytes)
    std::size_t max_file_size = 10 * 1024 * 1024; // 10MB
    
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
    static core::result<chat_config, std::string> from_json(const nlohmann::json& config_json);
    
    /**
     * @brief Convert configuration to JSON
     * @return JSON representation of configuration
     */
    nlohmann::json to_json() const;
};

/**
 * @brief User information
 */
struct user_info {
    /// User unique identifier
    core::node_id user_id;
    
    /// User nickname
    std::string nickname;
    
    /// User endpoint
    core::endpoint endpoint;
    
    /// User status
    enum class status {
        offline,
        online,
        away,
        busy,
        invisible
    } status = status::offline;
    
    /// Custom status message
    std::string status_message;
    
    /// User avatar hash (optional)
    std::string avatar_hash;
    
    /// User capabilities
    std::unordered_set<std::string> capabilities;
    
    /// Last seen timestamp
    std::chrono::steady_clock::time_point last_seen;
    
    /// User joined timestamp
    std::chrono::steady_clock::time_point joined_at;
    
    /// Public key for encryption (hex encoded)
    std::string public_key;
    
    /**
     * @brief Check if user is online
     * @param timeout Timeout for considering user offline
     * @return true if online, false otherwise
     */
    bool is_online(std::chrono::milliseconds timeout = std::chrono::milliseconds{60000}) const;
    
    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const;
    
    /**
     * @brief Create from JSON
     * @param json_data JSON data
     * @return User info or error
     */
    static core::result<user_info, std::string> from_json(const nlohmann::json& json_data);
};

/**
 * @brief Chat message
 */
struct chat_message {
    /// Message unique identifier
    core::message_id id;
    
    /// Sender user ID
    core::node_id sender_id;
    
    /// Sender nickname at time of sending
    std::string sender_nickname;
    
    /// Target (room ID or user ID for direct message)
    std::string target;
    
    /// Message type
    enum class type {
        text,           // Regular text message
        system,         // System message (user joined/left, etc.)
        private_msg,    // Direct message to specific user
        typing,         // Typing indicator
        file,           // File transfer message
        image,          // Image message
        emoji_reaction, // Emoji reaction to another message
        command         // Bot/system command
    } message_type = type::text;
    
    /// Message content
    std::string content;
    
    /// Message timestamp
    std::chrono::steady_clock::time_point timestamp;
    
    /// Reply to message ID (optional)
    std::optional<core::message_id> reply_to;
    
    /// Message tags/metadata
    std::unordered_map<std::string, std::string> metadata;
    
    /// Message delivery status
    enum class delivery_status {
        pending,    // Not yet sent
        sent,       // Sent to network
        delivered,  // Delivered to recipient(s)
        read,       // Read by recipient(s)
        failed      // Failed to deliver
    } status = delivery_status::pending;
    
    /// Encryption status
    bool encrypted = false;
    
    /// Message expiration time (for temporary messages)
    std::optional<std::chrono::steady_clock::time_point> expires_at;
    
    /**
     * @brief Check if message has expired
     * @return true if expired, false otherwise
     */
    bool is_expired() const;
    
    /**
     * @brief Get message age
     * @return Message age duration
     */
    std::chrono::milliseconds age() const;
    
    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const;
    
    /**
     * @brief Create from JSON
     * @param json_data JSON data
     * @return Chat message or error
     */
    static core::result<chat_message, std::string> from_json(const nlohmann::json& json_data);
    
    /**
     * @brief Create text message
     * @param sender_id Sender user ID
     * @param sender_nickname Sender nickname
     * @param target Target room or user
     * @param content Message content
     * @return Text message
     */
    static chat_message create_text_message(
        const core::node_id& sender_id,
        const std::string& sender_nickname,
        const std::string& target,
        const std::string& content
    );
    
    /**
     * @brief Create system message
     * @param target Target room
     * @param content System message content
     * @return System message
     */
    static chat_message create_system_message(
        const std::string& target,
        const std::string& content
    );
};

/**
 * @brief Chat room information
 */
struct chat_room {
    /// Room unique identifier
    std::string room_id;
    
    /// Room display name
    std::string name;
    
    /// Room description
    std::string description;
    
    /// Room creator user ID
    core::node_id creator_id;
    
    /// Room creation timestamp
    std::chrono::steady_clock::time_point created_at;
    
    /// Room type
    enum class type {
        public_room,    // Open to all users
        private_room,   // Invitation only
        direct_message, // 1-on-1 conversation
        group_message   // Small group conversation
    } room_type = type::public_room;
    
    /// Maximum number of users
    std::size_t max_users = 1000;
    
    /// Current user count
    std::size_t user_count = 0;
    
    /// Room settings
    struct settings {
        bool allow_anonymous = false;
        bool moderated = false;
        bool persistent = true;
        bool encrypted = true;
        std::chrono::hours message_retention{24 * 30}; // 30 days
    } settings;
    
    /// Room tags/categories
    std::unordered_set<std::string> tags;
    
    /// Room moderators
    std::unordered_set<core::node_id> moderators;
    
    /**
     * @brief Check if user is moderator
     * @param user_id User ID to check
     * @return true if moderator, false otherwise
     */
    bool is_moderator(const core::node_id& user_id) const;
    
    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const;
    
    /**
     * @brief Create from JSON
     * @param json_data JSON data
     * @return Chat room or error
     */
    static core::result<chat_room, std::string> from_json(const nlohmann::json& json_data);
};

/**
 * @brief Chat statistics
 */
struct chat_stats {
    /// Total messages sent
    std::uint64_t messages_sent = 0;
    
    /// Total messages received
    std::uint64_t messages_received = 0;
    
    /// Active users count
    std::uint64_t active_users = 0;
    
    /// Active rooms count
    std::uint64_t active_rooms = 0;
    
    /// Average message length
    double avg_message_length = 0.0;
    
    /// Messages per second
    double message_rate = 0.0;
    
    /// Chat service uptime
    std::chrono::steady_clock::time_point start_time;
    
    /**
     * @brief Convert to JSON
     * @return JSON representation
     */
    nlohmann::json to_json() const;
};

/**
 * @brief Chat event callback types
 */
using message_received_handler = std::function<void(const chat_message&)>;
using user_joined_handler = std::function<void(const user_info&, const std::string& room_id)>;
using user_left_handler = std::function<void(const core::node_id&, const std::string& room_id)>;
using user_status_changed_handler = std::function<void(const user_info&)>;
using typing_indicator_handler = std::function<void(const core::node_id&, const std::string& room_id, bool typing)>;
using room_created_handler = std::function<void(const chat_room&)>;
using room_deleted_handler = std::function<void(const std::string& room_id)>;

/**
 * @brief Interface for chat service operations
 * 
 * Provides high-level chat functionality including text messaging,
 * user management, chat rooms, presence tracking, and file transfers.
 * Built on top of the P2P node and message bus.
 */
class i_chat : public core::i_component,
               public core::i_configurable<chat_config> {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~i_chat() = default;

    // ========================================
    // Lifecycle Management
    // ========================================

    /**
     * @brief Initialize chat service
     * @param config Chat configuration
     * @param p2p_node P2P node instance
     * @param message_bus Message bus instance
     * @return Awaitable void result
     */
    virtual core::async_void initialize(
        const chat_config& config,
        std::shared_ptr<i_p2p_node> p2p_node,
        std::shared_ptr<i_message_bus> message_bus
    ) = 0;

    /**
     * @brief Start chat service
     * @return Awaitable void result
     */
    virtual core::async_void start() override = 0;

    /**
     * @brief Stop chat service
     * @return Awaitable void result
     */
    virtual core::async_void stop() override = 0;

    // ========================================
    // User Management API
    // ========================================

    /**
     * @brief Register user with chat service
     * @param nickname User nickname
     * @param status_message Initial status message
     * @param public_key User's public key for encryption
     * @return Awaitable result with user info
     */
    virtual core::async_result<user_info> register_user(
        const std::string& nickname,
        const std::string& status_message = "",
        const std::string& public_key = ""
    ) = 0;

    /**
     * @brief Update user profile
     * @param nickname New nickname (empty to keep current)
     * @param status New status
     * @param status_message New status message
     * @return Awaitable result with success indication
     */
    virtual core::async_result<bool> update_user_profile(
        const std::string& nickname = "",
        user_info::status status = user_info::status::online,
        const std::string& status_message = ""
    ) = 0;

    /**
     * @brief Get user information
     * @param user_id User ID
     * @return User info if found
     */
    virtual std::optional<user_info> get_user(const core::node_id& user_id) const = 0;

    /**
     * @brief Get user by nickname
     * @param nickname User nickname
     * @return User info if found
     */
    virtual std::optional<user_info> get_user_by_nickname(const std::string& nickname) const = 0;

    /**
     * @brief Get all online users
     * @return Vector of online user info
     */
    virtual std::vector<user_info> get_online_users() const = 0;

    /**
     * @brief Get users in a specific room
     * @param room_id Room ID
     * @return Vector of user info
     */
    virtual std::vector<user_info> get_room_users(const std::string& room_id) const = 0;

    /**
     * @brief Get current user info
     * @return Current user info
     */
    virtual user_info get_current_user() const = 0;

    // ========================================
    // Message API
    // ========================================

    /**
     * @brief Send text message to room
     * @param room_id Target room ID
     * @param content Message content
     * @param reply_to Reply to message ID (optional)
     * @return Awaitable result with message ID
     */
    virtual core::async_result<core::message_id> send_message(
        const std::string& room_id,
        const std::string& content,
        const std::optional<core::message_id>& reply_to = std::nullopt
    ) = 0;

    /**
     * @brief Send private message to specific user
     * @param target_user_id Target user ID
     * @param content Message content
     * @param reply_to Reply to message ID (optional)
     * @return Awaitable result with message ID
     */
    virtual core::async_result<core::message_id> send_private_message(
        const core::node_id& target_user_id,
        const std::string& content,
        const std::optional<core::message_id>& reply_to = std::nullopt
    ) = 0;

    /**
     * @brief Send typing indicator
     * @param room_id Target room ID
     * @param typing True if typing, false if stopped
     * @return Awaitable void result
     */
    virtual core::async_void send_typing_indicator(
        const std::string& room_id,
        bool typing
    ) = 0;

    /**
     * @brief Mark message as read
     * @param message_id Message ID
     * @return Awaitable result with success indication
     */
    virtual core::async_result<bool> mark_message_read(const core::message_id& message_id) = 0;

    /**
     * @brief Delete message (if sender or moderator)
     * @param message_id Message ID
     * @return Awaitable result with success indication
     */
    virtual core::async_result<bool> delete_message(const core::message_id& message_id) = 0;

    /**
     * @brief Edit message (if sender)
     * @param message_id Message ID
     * @param new_content New message content
     * @return Awaitable result with success indication
     */
    virtual core::async_result<bool> edit_message(
        const core::message_id& message_id,
        const std::string& new_content
    ) = 0;

    // ========================================
    // Chat Room API
    // ========================================

    /**
     * @brief Create new chat room
     * @param name Room name
     * @param description Room description
     * @param room_type Room type
     * @param max_users Maximum users allowed
     * @return Awaitable result with room ID
     */
    virtual core::async_result<std::string> create_room(
        const std::string& name,
        const std::string& description = "",
        chat_room::type room_type = chat_room::type::public_room,
        std::size_t max_users = 1000
    ) = 0;

    /**
     * @brief Join chat room
     * @param room_id Room ID to join
     * @return Awaitable result with success indication
     */
    virtual core::async_result<bool> join_room(const std::string& room_id) = 0;

    /**
     * @brief Leave chat room
     * @param room_id Room ID to leave
     * @return Awaitable result with success indication
     */
    virtual core::async_result<bool> leave_room(const std::string& room_id) = 0;

    /**
     * @brief Delete chat room (if creator or moderator)
     * @param room_id Room ID to delete
     * @return Awaitable result with success indication
     */
    virtual core::async_result<bool> delete_room(const std::string& room_id) = 0;

    /**
     * @brief Get room information
     * @param room_id Room ID
     * @return Room info if found
     */
    virtual std::optional<chat_room> get_room(const std::string& room_id) const = 0;

    /**
     * @brief Get all available rooms
     * @return Vector of room info
     */
    virtual std::vector<chat_room> get_rooms() const = 0;

    /**
     * @brief Get rooms user is currently in
     * @return Vector of room IDs
     */
    virtual std::vector<std::string> get_joined_rooms() const = 0;

    // ========================================
    // Message History API
    // ========================================

    /**
     * @brief Get message history for room
     * @param room_id Room ID
     * @param limit Maximum number of messages
     * @param before_message_id Get messages before this ID (optional)
     * @return Vector of chat messages
     */
    virtual std::vector<chat_message> get_message_history(
        const std::string& room_id,
        std::size_t limit = 100,
        const std::optional<core::message_id>& before_message_id = std::nullopt
    ) const = 0;

    /**
     * @brief Get private message history with user
     * @param user_id Target user ID
     * @param limit Maximum number of messages
     * @param before_message_id Get messages before this ID (optional)
     * @return Vector of chat messages
     */
    virtual std::vector<chat_message> get_private_message_history(
        const core::node_id& user_id,
        std::size_t limit = 100,
        const std::optional<core::message_id>& before_message_id = std::nullopt
    ) const = 0;

    /**
     * @brief Search messages
     * @param query Search query
     * @param room_id Room to search in (optional, searches all if empty)
     * @param limit Maximum results
     * @return Vector of matching messages
     */
    virtual std::vector<chat_message> search_messages(
        const std::string& query,
        const std::string& room_id = "",
        std::size_t limit = 50
    ) const = 0;

    /**
     * @brief Clear message history for room
     * @param room_id Room ID
     * @return Number of messages cleared
     */
    virtual std::size_t clear_message_history(const std::string& room_id) = 0;

    // ========================================
    // Event Handlers API
    // ========================================

    /**
     * @brief Set message received handler
     * @param handler Handler function
     */
    virtual void set_message_received_handler(message_received_handler handler) = 0;

    /**
     * @brief Set user joined handler
     * @param handler Handler function
     */
    virtual void set_user_joined_handler(user_joined_handler handler) = 0;

    /**
     * @brief Set user left handler
     * @param handler Handler function
     */
    virtual void set_user_left_handler(user_left_handler handler) = 0;

    /**
     * @brief Set user status changed handler
     * @param handler Handler function
     */
    virtual void set_user_status_changed_handler(user_status_changed_handler handler) = 0;

    /**
     * @brief Set typing indicator handler
     * @param handler Handler function
     */
    virtual void set_typing_indicator_handler(typing_indicator_handler handler) = 0;

    /**
     * @brief Set room created handler
     * @param handler Handler function
     */
    virtual void set_room_created_handler(room_created_handler handler) = 0;

    /**
     * @brief Set room deleted handler
     * @param handler Handler function
     */
    virtual void set_room_deleted_handler(room_deleted_handler handler) = 0;

    // ========================================
    // File Transfer API (if enabled)
    // ========================================

    /**
     * @brief Send file to room
     * @param room_id Target room ID
     * @param file_path Local file path
     * @param description File description
     * @return Awaitable result with message ID
     */
    virtual core::async_result<core::message_id> send_file(
        const std::string& room_id,
        const std::string& file_path,
        const std::string& description = ""
    ) = 0;

    /**
     * @brief Send file to specific user
     * @param target_user_id Target user ID
     * @param file_path Local file path
     * @param description File description
     * @return Awaitable result with message ID
     */
    virtual core::async_result<core::message_id> send_file_to_user(
        const core::node_id& target_user_id,
        const std::string& file_path,
        const std::string& description = ""
    ) = 0;

    /**
     * @brief Download file from message
     * @param message_id File message ID
     * @param save_path Local save path
     * @return Awaitable result with success indication
     */
    virtual core::async_result<bool> download_file(
        const core::message_id& message_id,
        const std::string& save_path
    ) = 0;

    // ========================================
    // Statistics and Status API
    // ========================================

    /**
     * @brief Get chat statistics
     * @return Current statistics
     */
    virtual chat_stats get_stats() const = 0;

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
    // Configuration API
    // ========================================

    /**
     * @brief Configure chat service
     * @param config New configuration
     * @return Result indicating success or error
     */
    virtual core::result<void, std::string> configure(const chat_config& config) override = 0;

    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    virtual chat_config get_config() const override = 0;

protected:
    /**
     * @brief Protected default constructor
     */
    i_chat() = default;

    /**
     * @brief Protected copy constructor
     */
    i_chat(const i_chat&) = default;

    /**
     * @brief Protected move constructor
     */
    i_chat(i_chat&&) = default;

    /**
     * @brief Protected copy assignment
     */
    i_chat& operator=(const i_chat&) = default;

    /**
     * @brief Protected move assignment
     */
    i_chat& operator=(i_chat&&) = default;
};

/**
 * @brief Factory function for creating chat service instances
 * @param io_context Boost.Asio IO context
 * @param config Chat configuration
 * @return Unique pointer to chat service instance
 */
std::unique_ptr<i_chat> create_chat_service(
    boost::asio::io_context& io_context,
    const chat_config& config = {}
);

/**
 * @brief Builder class for chat service configuration
 * 
 * Provides a fluent interface for configuring chat service with
 * validation and sensible defaults.
 */
class chat_builder {
public:
    /**
     * @brief Constructor
     */
    explicit chat_builder(boost::asio::io_context& io_context);

    /**
     * @brief Set maximum message length
     * @param length Maximum message length in characters
     * @return Reference to builder for chaining
     */
    chat_builder& with_max_message_length(std::size_t length);

    /**
     * @brief Set history retention
     * @param max_size Maximum history size
     * @param retention Retention period
     * @return Reference to builder for chaining
     */
    chat_builder& with_history_settings(std::size_t max_size, std::chrono::hours retention);

    /**
     * @brief Enable/disable encryption
     * @param enabled True to enable, false to disable
     * @return Reference to builder for chaining
     */
    chat_builder& with_encryption(bool enabled);

    /**
     * @brief Enable/disable persistence
     * @param enabled True to enable, false to disable
     * @param persistence_path Path for persistence file
     * @return Reference to builder for chaining
     */
    chat_builder& with_persistence(bool enabled, const std::string& persistence_path = "");

    /**
     * @brief Set room limits
     * @param max_rooms Maximum number of rooms
     * @param max_users_per_room Maximum users per room
     * @return Reference to builder for chaining
     */
    chat_builder& with_room_limits(std::size_t max_rooms, std::size_t max_users_per_room);

    /**
     * @brief Enable/disable features
     * @param typing_indicators Enable typing indicators
     * @param presence Enable user presence
     * @param read_receipts Enable read receipts
     * @param file_transfer Enable file transfer
     * @return Reference to builder for chaining
     */
    chat_builder& with_features(
        bool typing_indicators,
        bool presence,
        bool read_receipts,
        bool file_transfer = false
    );

    /**
     * @brief Set file transfer settings
     * @param max_file_size Maximum file size in bytes
     * @return Reference to builder for chaining
     */
    chat_builder& with_file_transfer_settings(std::size_t max_file_size);

    /**
     * @brief Load configuration from JSON file
     * @param config_path Path to configuration file
     * @return Reference to builder for chaining
     */
    chat_builder& with_config_file(const std::string& config_path);

    /**
     * @brief Build and create the chat service
     * @return Unique pointer to created chat service
     * @throws std::invalid_argument if configuration is invalid
     */
    std::unique_ptr<i_chat> build();

private:
    boost::asio::io_context& io_context_;
    chat_config config_;
};

} // namespace ss::application