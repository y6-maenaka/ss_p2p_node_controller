#include "../../../../include/ss_p2p/application/i_chat.hpp"
#include "../../../../include/ss_p2p/core/result.hpp"
#include "../../../../include/ss_p2p/ss_logger.hpp"
#include "../../../../include/crypto_utils/crypto_utils.hpp"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <deque>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <regex>

namespace ss::application {

/**
 * @brief Implementation of Chat service interface
 * 
 * Provides high-level chat functionality including text messaging,
 * user management, chat rooms, and presence tracking built on top
 * of the P2P node and message bus.
 */
class chat_service_impl : public i_chat {
public:
    /**
     * @brief Constructor
     * @param io_context Boost.Asio IO context
     */
    explicit chat_service_impl(boost::asio::io_context& io_context)
        : io_context_(io_context)
        , stats_{}
        , running_(false)
        , next_message_id_(1)
        , presence_timer_(io_context)
        , maintenance_timer_(io_context)
        , logger_(std::make_shared<ss_logger>("ChatService"))
    {
        stats_.start_time = std::chrono::steady_clock::now();
    }

    /**
     * @brief Destructor
     */
    ~chat_service_impl() override {
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
        return "ChatService";
    }

    bool is_running() const noexcept override {
        return running_.load();
    }

    bool is_healthy() const noexcept override {
        std::shared_lock lock(mutex_);
        return running_.load() && p2p_node_ && message_bus_ && 
               p2p_node_->is_healthy() && message_bus_->is_healthy();
    }

    std::string status() const override {
        std::shared_lock lock(mutex_);
        
        nlohmann::json status_json;
        status_json["name"] = name();
        status_json["running"] = is_running();
        status_json["healthy"] = is_healthy();
        status_json["current_user"] = current_user_.to_json();
        status_json["room_count"] = chat_rooms_.size();
        status_json["user_count"] = users_.size();
        status_json["stats"] = get_stats().to_json();
        
        return status_json.dump(2);
    }

    // ========================================
    // Lifecycle Management
    // ========================================

    core::async_void initialize(
        const chat_config& config,
        std::shared_ptr<i_p2p_node> p2p_node,
        std::shared_ptr<i_message_bus> message_bus
    ) override {
        std::unique_lock lock(mutex_);
        
        logger_->info("Initializing chat service");
        
        auto validation_result = config.validate();
        if (!validation_result.is_ok()) {
            throw std::invalid_argument("Invalid configuration: " + validation_result.error());
        }
        
        if (!p2p_node || !message_bus) {
            throw std::invalid_argument("P2P node and message bus are required");
        }
        
        config_ = config;
        p2p_node_ = std::move(p2p_node);
        message_bus_ = std::move(message_bus);
        
        // Initialize current user
        current_user_.user_id = p2p_node_->get_node_id();
        current_user_.endpoint = p2p_node_->get_local_endpoint();
        current_user_.nickname = "User_" + current_user_.user_id.to_hex().substr(0, 8);
        current_user_.status = user_info::status::offline;
        current_user_.joined_at = std::chrono::steady_clock::now();
        
        if (config_.enable_encryption) {
            // Generate key pair for encryption
            // current_user_.public_key = generate_public_key();
        }
        
        co_return;
    }

    core::async_void start() override {
        std::unique_lock lock(mutex_);
        
        if (running_.load()) {
            logger_->warn("Chat service already running");
            co_return;
        }
        
        logger_->info("Starting chat service");
        
        try {
            // Register message handlers
            setup_message_handlers();
            
            // Set current user online
            current_user_.status = user_info::status::online;
            current_user_.last_seen = std::chrono::steady_clock::now();
            
            running_.store(true);
            
            // Start periodic tasks
            schedule_presence_update();
            schedule_maintenance();
            
            // Load state if persistence is enabled
            if (config_.enable_persistence) {
                load_chat_history();
            }
            
            logger_->info("Chat service started successfully");
            
        } catch (const std::exception& e) {
            running_.store(false);
            logger_->error("Failed to start chat service: " + std::string(e.what()));
            throw;
        }
        
        co_return;
    }

    core::async_void stop() override {
        std::unique_lock lock(mutex_);
        
        if (!running_.load()) {
            logger_->warn("Chat service not running");
            co_return;
        }
        
        logger_->info("Stopping chat service");
        
        try {
            running_.store(false);
            
            // Cancel timers
            presence_timer_.cancel();
            maintenance_timer_.cancel();
            
            // Set user offline
            current_user_.status = user_info::status::offline;
            
            // Announce departure from all rooms
            for (const auto& room_id : joined_rooms_) {
                co_await announce_user_left(room_id);
            }
            
            // Save state if persistence is enabled
            if (config_.enable_persistence) {
                save_chat_history();
            }
            
            // Unregister message handlers
            cleanup_message_handlers();
            
            logger_->info("Chat service stopped successfully");
            
        } catch (const std::exception& e) {
            logger_->error("Error during stop: " + std::string(e.what()));
        }
        
        co_return;
    }

    // ========================================
    // User Management API
    // ========================================

    core::async_result<user_info> register_user(
        const std::string& nickname,
        const std::string& status_message,
        const std::string& public_key
    ) override {
        if (!is_running()) {
            co_return user_info{};
        }
        
        try {
            std::unique_lock lock(mutex_);
            
            // Validate nickname
            if (nickname.length() < config_.min_nickname_length || 
                nickname.length() > config_.max_nickname_length) {
                logger_->error("Invalid nickname length");
                co_return user_info{};
            }
            
            // Check if nickname is already taken
            for (const auto& [id, user] : users_) {
                if (user.nickname == nickname && user.user_id != current_user_.user_id) {
                    logger_->error("Nickname already taken: " + nickname);
                    co_return user_info{};
                }
            }
            
            // Update current user
            current_user_.nickname = nickname;
            current_user_.status_message = status_message;
            current_user_.public_key = public_key;
            current_user_.status = user_info::status::online;
            current_user_.last_seen = std::chrono::steady_clock::now();
            
            // Add to users map
            users_[current_user_.user_id] = current_user_;
            
            logger_->info("User registered: " + nickname);
            
            co_return current_user_;
            
        } catch (const std::exception& e) {
            logger_->error("Register user error: " + std::string(e.what()));
            co_return user_info{};
        }
    }

    core::async_result<bool> update_user_profile(
        const std::string& nickname,
        user_info::status status,
        const std::string& status_message
    ) override {
        if (!is_running()) {
            co_return false;
        }
        
        try {
            std::unique_lock lock(mutex_);
            
            bool changed = false;
            
            if (!nickname.empty() && nickname != current_user_.nickname) {
                // Validate and check uniqueness
                if (nickname.length() >= config_.min_nickname_length && 
                    nickname.length() <= config_.max_nickname_length) {
                    
                    bool unique = true;
                    for (const auto& [id, user] : users_) {
                        if (user.nickname == nickname && user.user_id != current_user_.user_id) {
                            unique = false;
                            break;
                        }
                    }
                    
                    if (unique) {
                        current_user_.nickname = nickname;
                        changed = true;
                    }
                }
            }
            
            if (status != current_user_.status) {
                current_user_.status = status;
                changed = true;
            }
            
            if (status_message != current_user_.status_message) {
                current_user_.status_message = status_message;
                changed = true;
            }
            
            if (changed) {
                current_user_.last_seen = std::chrono::steady_clock::now();
                users_[current_user_.user_id] = current_user_;
                
                // Notify status change handlers
                if (user_status_changed_handler_) {
                    user_status_changed_handler_(current_user_);
                }
                
                logger_->info("User profile updated: " + current_user_.nickname);
            }
            
            co_return changed;
            
        } catch (const std::exception& e) {
            logger_->error("Update user profile error: " + std::string(e.what()));
            co_return false;
        }
    }

    std::optional<user_info> get_user(const core::node_id& user_id) const override {
        std::shared_lock lock(mutex_);
        
        auto it = users_.find(user_id);
        if (it != users_.end()) {
            return it->second;
        }
        
        return std::nullopt;
    }

    std::optional<user_info> get_user_by_nickname(const std::string& nickname) const override {
        std::shared_lock lock(mutex_);
        
        for (const auto& [id, user] : users_) {
            if (user.nickname == nickname) {
                return user;
            }
        }
        
        return std::nullopt;
    }

    std::vector<user_info> get_online_users() const override {
        std::shared_lock lock(mutex_);
        
        std::vector<user_info> online_users;
        auto timeout = std::chrono::milliseconds(60000); // 1 minute timeout
        
        for (const auto& [id, user] : users_) {
            if (user.is_online(timeout)) {
                online_users.push_back(user);
            }
        }
        
        return online_users;
    }

    std::vector<user_info> get_room_users(const std::string& room_id) const override {
        std::shared_lock lock(mutex_);
        
        std::vector<user_info> room_users;
        
        auto it = room_users_.find(room_id);
        if (it != room_users_.end()) {
            for (const auto& user_id : it->second) {
                auto user_it = users_.find(user_id);
                if (user_it != users_.end()) {
                    room_users.push_back(user_it->second);
                }
            }
        }
        
        return room_users;
    }

    user_info get_current_user() const override {
        std::shared_lock lock(mutex_);
        return current_user_;
    }

    // ========================================
    // Message API
    // ========================================

    core::async_result<core::message_id> send_message(
        const std::string& room_id,
        const std::string& content,
        const std::optional<core::message_id>& reply_to
    ) override {
        if (!is_running() || content.length() > config_.max_message_length) {
            co_return core::message_id{};
        }
        
        try {
            auto message = chat_message::create_text_message(
                current_user_.user_id,
                current_user_.nickname,
                room_id,
                content
            );
            
            if (reply_to) {
                message.reply_to = reply_to;
            }
            
            // Store message in history
            {
                std::unique_lock lock(mutex_);
                auto& history = message_history_[room_id];
                history.push_back(message);
                
                // Limit history size
                if (history.size() > config_.max_history_size) {
                    history.erase(history.begin());
                }
                
                stats_.messages_sent++;
            }
            
            // Convert to bus message and publish
            ss::message ss_msg({{'c','h','a','t',0,0,0,0}});
            ss_msg.set_param("type", "room_message");
            ss_msg.set_param("message", message.to_json());
            
            auto success = co_await message_bus_->publish(
                "chat.room." + room_id,
                ss_msg,
                current_user_.endpoint,
                0,
                {"chat", "message"}
            );
            
            if (success) {
                message.status = chat_message::delivery_status::sent;
                logger_->debug("Sent message to room: " + room_id);
            } else {
                message.status = chat_message::delivery_status::failed;
                logger_->error("Failed to send message to room: " + room_id);
            }
            
            co_return message.id;
            
        } catch (const std::exception& e) {
            logger_->error("Send message error: " + std::string(e.what()));
            co_return core::message_id{};
        }
    }

    core::async_result<core::message_id> send_private_message(
        const core::node_id& target_user_id,
        const std::string& content,
        const std::optional<core::message_id>& reply_to
    ) override {
        if (!is_running() || content.length() > config_.max_message_length) {
            co_return core::message_id{};
        }
        
        try {
            auto message = chat_message::create_text_message(
                current_user_.user_id,
                current_user_.nickname,
                target_user_id.to_hex(), // Use user ID as target
                content
            );
            message.message_type = chat_message::type::private_msg;
            
            if (reply_to) {
                message.reply_to = reply_to;
            }
            
            // Store in private message history
            {
                std::unique_lock lock(mutex_);
                auto key = create_private_message_key(current_user_.user_id, target_user_id);
                auto& history = private_message_history_[key];
                history.push_back(message);
                
                if (history.size() > config_.max_history_size) {
                    history.erase(history.begin());
                }
                
                stats_.messages_sent++;
            }
            
            // Send directly to target user
            ss::message ss_msg({{'c','h','a','t',0,0,0,0}});
            ss_msg.set_param("type", "private_message");
            ss_msg.set_param("message", message.to_json());
            
            auto success = co_await p2p_node_->send_message(ss_msg, target_user_id);
            
            if (success) {
                message.status = chat_message::delivery_status::sent;
                logger_->debug("Sent private message to: " + target_user_id.to_hex());
            } else {
                message.status = chat_message::delivery_status::failed;
                logger_->error("Failed to send private message to: " + target_user_id.to_hex());
            }
            
            co_return message.id;
            
        } catch (const std::exception& e) {
            logger_->error("Send private message error: " + std::string(e.what()));
            co_return core::message_id{};
        }
    }

    core::async_void send_typing_indicator(const std::string& room_id, bool typing) override {
        if (!is_running() || !config_.enable_typing_indicators) {
            co_return;
        }
        
        try {
            ss::message ss_msg({{'c','h','a','t',0,0,0,0}});
            ss_msg.set_param("type", "typing_indicator");
            ss_msg.set_param("room_id", room_id);
            ss_msg.set_param("user_id", current_user_.user_id.to_hex());
            ss_msg.set_param("typing", typing);
            
            co_await message_bus_->publish(
                "chat.typing." + room_id,
                ss_msg,
                current_user_.endpoint,
                0,
                {"chat", "typing"}
            );
            
        } catch (const std::exception& e) {
            logger_->error("Send typing indicator error: " + std::string(e.what()));
        }
        
        co_return;
    }

    // ========================================
    // Chat Room API
    // ========================================

    core::async_result<std::string> create_room(
        const std::string& name,
        const std::string& description,
        chat_room::type room_type,
        std::size_t max_users
    ) override {
        if (!is_running()) {
            co_return std::string{};
        }
        
        try {
            std::unique_lock lock(mutex_);
            
            if (chat_rooms_.size() >= config_.max_chat_rooms) {
                logger_->error("Maximum number of chat rooms reached");
                co_return std::string{};
            }
            
            // Generate room ID
            auto room_id = generate_room_id(name);
            
            // Check if room already exists
            if (chat_rooms_.find(room_id) != chat_rooms_.end()) {
                logger_->error("Room already exists: " + room_id);
                co_return std::string{};
            }
            
            // Create room
            chat_room room;
            room.room_id = room_id;
            room.name = name;
            room.description = description;
            room.room_type = room_type;
            room.creator_id = current_user_.user_id;
            room.created_at = std::chrono::steady_clock::now();
            room.max_users = std::min(max_users, config_.max_users_per_room);
            
            chat_rooms_[room_id] = room;
            room_users_[room_id] = {current_user_.user_id};
            
            // Announce room creation
            if (room_created_handler_) {
                room_created_handler_(room);
            }
            
            logger_->info("Created room: " + room_id + " (" + name + ")");
            
            co_return room_id;
            
        } catch (const std::exception& e) {
            logger_->error("Create room error: " + std::string(e.what()));
            co_return std::string{};
        }
    }

    core::async_result<bool> join_room(const std::string& room_id) override {
        if (!is_running()) {
            co_return false;
        }
        
        try {
            std::unique_lock lock(mutex_);
            
            // Check if room exists
            auto room_it = chat_rooms_.find(room_id);
            if (room_it == chat_rooms_.end()) {
                logger_->error("Room not found: " + room_id);
                co_return false;
            }
            
            // Check if already joined
            if (std::find(joined_rooms_.begin(), joined_rooms_.end(), room_id) != joined_rooms_.end()) {
                logger_->warn("Already joined room: " + room_id);
                co_return true;
            }
            
            // Check room capacity
            auto& users_in_room = room_users_[room_id];
            if (users_in_room.size() >= room_it->second.max_users) {
                logger_->error("Room is full: " + room_id);
                co_return false;
            }
            
            // Join room
            users_in_room.insert(current_user_.user_id);
            joined_rooms_.push_back(room_id);
            room_it->second.user_count = users_in_room.size();
            
            lock.unlock();
            
            // Subscribe to room messages
            message_bus_->subscribe(
                "chat.room." + room_id,
                [this, room_id](const bus_message& msg) -> core::async_result<bool> {
                    co_return co_await handle_room_message(room_id, msg);
                },
                {"chat", "message"}
            );
            
            // Announce join
            co_await announce_user_joined(room_id);
            
            logger_->info("Joined room: " + room_id);
            
            co_return true;
            
        } catch (const std::exception& e) {
            logger_->error("Join room error: " + std::string(e.what()));
            co_return false;
        }
    }

    core::async_result<bool> leave_room(const std::string& room_id) override {
        if (!is_running()) {
            co_return false;
        }
        
        try {
            std::unique_lock lock(mutex_);
            
            // Check if joined
            auto it = std::find(joined_rooms_.begin(), joined_rooms_.end(), room_id);
            if (it == joined_rooms_.end()) {
                logger_->warn("Not joined to room: " + room_id);
                co_return false;
            }
            
            // Remove from room
            joined_rooms_.erase(it);
            
            auto room_users_it = room_users_.find(room_id);
            if (room_users_it != room_users_.end()) {
                room_users_it->second.erase(current_user_.user_id);
                
                // Update room user count
                auto room_it = chat_rooms_.find(room_id);
                if (room_it != chat_rooms_.end()) {
                    room_it->second.user_count = room_users_it->second.size();
                }
            }
            
            lock.unlock();
            
            // Announce leave
            co_await announce_user_left(room_id);
            
            // Unsubscribe from room messages
            // Note: Would need to track subscription IDs to properly unsubscribe
            
            logger_->info("Left room: " + room_id);
            
            co_return true;
            
        } catch (const std::exception& e) {
            logger_->error("Leave room error: " + std::string(e.what()));
            co_return false;
        }
    }

    std::optional<chat_room> get_room(const std::string& room_id) const override {
        std::shared_lock lock(mutex_);
        
        auto it = chat_rooms_.find(room_id);
        if (it != chat_rooms_.end()) {
            return it->second;
        }
        
        return std::nullopt;
    }

    std::vector<chat_room> get_rooms() const override {
        std::shared_lock lock(mutex_);
        
        std::vector<chat_room> rooms;
        rooms.reserve(chat_rooms_.size());
        
        for (const auto& [id, room] : chat_rooms_) {
            rooms.push_back(room);
        }
        
        return rooms;
    }

    std::vector<std::string> get_joined_rooms() const override {
        std::shared_lock lock(mutex_);
        return joined_rooms_;
    }

    // ========================================
    // Message History API
    // ========================================

    std::vector<chat_message> get_message_history(
        const std::string& room_id,
        std::size_t limit,
        const std::optional<core::message_id>& before_message_id
    ) const override {
        std::shared_lock lock(mutex_);
        
        auto it = message_history_.find(room_id);
        if (it == message_history_.end()) {
            return {};
        }
        
        const auto& history = it->second;
        std::vector<chat_message> result;
        
        if (before_message_id) {
            // Find the message to start from
            auto msg_it = std::find_if(history.rbegin(), history.rend(),
                [&before_message_id](const chat_message& msg) {
                    return msg.id == *before_message_id;
                });
            
            if (msg_it != history.rend()) {
                ++msg_it; // Start from the message before the specified one
                for (std::size_t i = 0; i < limit && msg_it != history.rend(); ++i, ++msg_it) {
                    result.push_back(*msg_it);
                }
            }
        } else {
            // Get the most recent messages
            auto start_it = history.size() > limit ? history.end() - limit : history.begin();
            result.assign(start_it, history.end());
        }
        
        return result;
    }

    std::vector<chat_message> get_private_message_history(
        const core::node_id& user_id,
        std::size_t limit,
        const std::optional<core::message_id>& before_message_id
    ) const override {
        std::shared_lock lock(mutex_);
        
        auto key = create_private_message_key(current_user_.user_id, user_id);
        auto it = private_message_history_.find(key);
        if (it == private_message_history_.end()) {
            return {};
        }
        
        const auto& history = it->second;
        std::vector<chat_message> result;
        
        if (before_message_id) {
            auto msg_it = std::find_if(history.rbegin(), history.rend(),
                [&before_message_id](const chat_message& msg) {
                    return msg.id == *before_message_id;
                });
            
            if (msg_it != history.rend()) {
                ++msg_it;
                for (std::size_t i = 0; i < limit && msg_it != history.rend(); ++i, ++msg_it) {
                    result.push_back(*msg_it);
                }
            }
        } else {
            auto start_it = history.size() > limit ? history.end() - limit : history.begin();
            result.assign(start_it, history.end());
        }
        
        return result;
    }

    // ========================================
    // Statistics and Configuration
    // ========================================

    chat_stats get_stats() const override {
        std::shared_lock lock(mutex_);
        
        auto current_stats = stats_;
        current_stats.active_users = get_online_users().size();
        current_stats.active_rooms = chat_rooms_.size();
        
        // Calculate average message length
        std::size_t total_length = 0;
        std::size_t total_messages = 0;
        
        for (const auto& [room_id, history] : message_history_) {
            for (const auto& msg : history) {
                total_length += msg.content.length();
                total_messages++;
            }
        }
        
        if (total_messages > 0) {
            current_stats.avg_message_length = static_cast<double>(total_length) / total_messages;
        }
        
        return current_stats;
    }

    core::result<void, std::string> configure(const chat_config& config) override {
        auto validation_result = config.validate();
        if (!validation_result.is_ok()) {
            return validation_result;
        }
        
        std::unique_lock lock(mutex_);
        config_ = config;
        
        return core::result<void, std::string>::ok();
    }

    chat_config get_config() const override {
        std::shared_lock lock(mutex_);
        return config_;
    }

    // ========================================
    // Event Handlers (simplified stubs)
    // ========================================

    void set_message_received_handler(message_received_handler handler) override {
        std::unique_lock lock(mutex_);
        message_received_handler_ = std::move(handler);
    }

    void set_user_joined_handler(user_joined_handler handler) override {
        std::unique_lock lock(mutex_);
        user_joined_handler_ = std::move(handler);
    }

    void set_user_left_handler(user_left_handler handler) override {
        std::unique_lock lock(mutex_);
        user_left_handler_ = std::move(handler);
    }

    void set_user_status_changed_handler(user_status_changed_handler handler) override {
        std::unique_lock lock(mutex_);
        user_status_changed_handler_ = std::move(handler);
    }

    void set_typing_indicator_handler(typing_indicator_handler handler) override {
        std::unique_lock lock(mutex_);
        typing_indicator_handler_ = std::move(handler);
    }

    void set_room_created_handler(room_created_handler handler) override {
        std::unique_lock lock(mutex_);
        room_created_handler_ = std::move(handler);
    }

    void set_room_deleted_handler(room_deleted_handler handler) override {
        std::unique_lock lock(mutex_);
        room_deleted_handler_ = std::move(handler);
    }

    // ========================================
    // Simplified implementations for remaining methods
    // ========================================

    core::async_result<bool> mark_message_read(const core::message_id& message_id) override {
        // Simplified implementation
        co_return true;
    }

    core::async_result<bool> delete_message(const core::message_id& message_id) override {
        // Simplified implementation
        co_return false;
    }

    core::async_result<bool> edit_message(const core::message_id& message_id, const std::string& new_content) override {
        // Simplified implementation
        co_return false;
    }

    core::async_result<bool> delete_room(const std::string& room_id) override {
        // Simplified implementation
        co_return false;
    }

    std::vector<chat_message> search_messages(const std::string& query, const std::string& room_id, std::size_t limit) const override {
        // Simplified implementation
        return {};
    }

    std::size_t clear_message_history(const std::string& room_id) override {
        // Simplified implementation
        return 0;
    }

    core::async_result<core::message_id> send_file(const std::string& room_id, const std::string& file_path, const std::string& description) override {
        // Simplified implementation
        co_return core::message_id{};
    }

    core::async_result<core::message_id> send_file_to_user(const core::node_id& target_user_id, const std::string& file_path, const std::string& description) override {
        // Simplified implementation
        co_return core::message_id{};
    }

    core::async_result<bool> download_file(const core::message_id& message_id, const std::string& save_path) override {
        // Simplified implementation
        co_return false;
    }

private:
    // Helper methods
    std::string generate_room_id(const std::string& name) {
        return "room_" + std::to_string(std::hash<std::string>{}(name + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())));
    }

    std::string create_private_message_key(const core::node_id& user1, const core::node_id& user2) const {
        auto id1 = user1.to_hex();
        auto id2 = user2.to_hex();
        return id1 < id2 ? id1 + "_" + id2 : id2 + "_" + id1;
    }

    void setup_message_handlers() {
        // Register with message bus for chat messages
        message_bus_->subscribe("chat.*", 
            [this](const bus_message& msg) -> core::async_result<bool> {
                co_return co_await handle_chat_message(msg); 
            },
            {"chat"}
        );
    }

    void cleanup_message_handlers() {
        // Would need to track subscription IDs to properly cleanup
    }

    core::async_result<bool> handle_chat_message(const bus_message& msg) {
        // Simplified message handling
        co_return true;
    }

    core::async_result<bool> handle_room_message(const std::string& room_id, const bus_message& msg) {
        // Simplified room message handling
        co_return true;
    }

    core::async_void announce_user_joined(const std::string& room_id) {
        // Simplified user join announcement
        co_return;
    }

    core::async_void announce_user_left(const std::string& room_id) {
        // Simplified user leave announcement  
        co_return;
    }

    void schedule_presence_update() {
        if (!running_.load()) return;
        
        presence_timer_.expires_after(config_.presence_interval);
        presence_timer_.async_wait([this](const boost::system::error_code& ec) {
            if (!ec && running_.load()) {
                // Update presence
                schedule_presence_update();
            }
        });
    }

    void schedule_maintenance() {
        if (!running_.load()) return;
        
        maintenance_timer_.expires_after(std::chrono::minutes(5));
        maintenance_timer_.async_wait([this](const boost::system::error_code& ec) {
            if (!ec && running_.load()) {
                // Perform maintenance
                schedule_maintenance();
            }
        });
    }

    void load_chat_history() {
        // Simplified history loading
    }

    void save_chat_history() {
        // Simplified history saving
    }

    // Core dependencies and state
    boost::asio::io_context& io_context_;
    std::shared_ptr<i_p2p_node> p2p_node_;
    std::shared_ptr<i_message_bus> message_bus_;
    
    mutable std::shared_mutex mutex_;
    chat_config config_;
    chat_stats stats_;
    std::atomic<bool> running_;
    
    // User management
    user_info current_user_;
    std::unordered_map<core::node_id, user_info> users_;
    
    // Room management
    std::unordered_map<std::string, chat_room> chat_rooms_;
    std::unordered_map<std::string, std::unordered_set<core::node_id>> room_users_;
    std::vector<std::string> joined_rooms_;
    
    // Message history
    std::unordered_map<std::string, std::deque<chat_message>> message_history_;
    std::unordered_map<std::string, std::deque<chat_message>> private_message_history_;
    std::atomic<std::uint64_t> next_message_id_;
    
    // Event handlers
    message_received_handler message_received_handler_;
    user_joined_handler user_joined_handler_;
    user_left_handler user_left_handler_;
    user_status_changed_handler user_status_changed_handler_;
    typing_indicator_handler typing_indicator_handler_;
    room_created_handler room_created_handler_;
    room_deleted_handler room_deleted_handler_;
    
    // Timers
    boost::asio::steady_timer presence_timer_;
    boost::asio::steady_timer maintenance_timer_;
    
    // Logging
    std::shared_ptr<ss_logger> logger_;
};

// ========================================
// Implementation of supporting structures (simplified)
// ========================================

core::result<void, std::string> chat_config::validate() const {
    if (max_message_length == 0) {
        return core::result<void, std::string>::error("Max message length must be greater than 0");
    }
    if (min_nickname_length == 0 || max_nickname_length == 0 || min_nickname_length > max_nickname_length) {
        return core::result<void, std::string>::error("Invalid nickname length settings");
    }
    return core::result<void, std::string>::ok();
}

core::result<chat_config, std::string> chat_config::from_json(const nlohmann::json& config_json) {
    chat_config config;
    // Simplified JSON parsing
    if (config_json.contains("max_message_length")) {
        config.max_message_length = config_json["max_message_length"];
    }
    return core::result<chat_config, std::string>::ok(std::move(config));
}

nlohmann::json chat_config::to_json() const {
    nlohmann::json json;
    json["max_message_length"] = max_message_length;
    json["enable_encryption"] = enable_encryption;
    return json;
}

bool user_info::is_online(std::chrono::milliseconds timeout) const {
    auto now = std::chrono::steady_clock::now();
    return (now - last_seen) <= timeout && status != status::offline;
}

nlohmann::json user_info::to_json() const {
    nlohmann::json json;
    json["user_id"] = user_id.to_hex();
    json["nickname"] = nickname;
    json["status"] = static_cast<int>(status);
    json["status_message"] = status_message;
    return json;
}

core::result<user_info, std::string> user_info::from_json(const nlohmann::json& json_data) {
    // Simplified implementation
    user_info info;
    if (json_data.contains("nickname")) {
        info.nickname = json_data["nickname"];
    }
    return core::result<user_info, std::string>::ok(std::move(info));
}

chat_message chat_message::create_text_message(
    const core::node_id& sender_id,
    const std::string& sender_nickname,
    const std::string& target,
    const std::string& content
) {
    chat_message msg;
    msg.id = core::message_id::generate();
    msg.sender_id = sender_id;
    msg.sender_nickname = sender_nickname;
    msg.target = target;
    msg.content = content;
    msg.message_type = type::text;
    msg.timestamp = std::chrono::steady_clock::now();
    return msg;
}

chat_message chat_message::create_system_message(
    const std::string& target,
    const std::string& content
) {
    chat_message msg;
    msg.id = core::message_id::generate();
    msg.target = target;
    msg.content = content;
    msg.message_type = type::system;
    msg.timestamp = std::chrono::steady_clock::now();
    return msg;
}

nlohmann::json chat_message::to_json() const {
    nlohmann::json json;
    json["id"] = id.value();
    json["sender_id"] = sender_id.to_hex();
    json["sender_nickname"] = sender_nickname;
    json["target"] = target;
    json["type"] = static_cast<int>(message_type);
    json["content"] = content;
    return json;
}

nlohmann::json chat_stats::to_json() const {
    nlohmann::json json;
    json["messages_sent"] = messages_sent;
    json["messages_received"] = messages_received;
    json["active_users"] = active_users;
    json["active_rooms"] = active_rooms;
    return json;
}

// Factory function
std::unique_ptr<i_chat> create_chat_service(
    boost::asio::io_context& io_context,
    const chat_config& config
) {
    return std::make_unique<chat_service_impl>(io_context);
}

} // namespace ss::application