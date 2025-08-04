/**
 * @file main.cpp
 * @brief Simple P2P Chat Application Example
 * 
 * This example demonstrates how to use the SS P2P Node Controller library
 * to create a simple peer-to-peer chat application. It showcases:
 * 
 * - P2P node initialization and configuration
 * - Message bus setup for pub/sub messaging
 * - Chat service integration
 * - Command-line interface for user interaction
 * - Service management for daemon mode
 */

#include "../../include/ss_p2p/application/i_p2p_node.hpp"
#include "../../include/ss_p2p/application/i_message_bus.hpp"
#include "../../include/ss_p2p/application/i_chat.hpp"
#include "../../include/ss_p2p/application/service_interface.hpp"
#include "../../include/ss_p2p/core/types.hpp"
#include "../../include/ss_p2p/ss_logger.hpp"

#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <vector>
#include <memory>
#include <boost/asio.hpp>
#include <json.hpp>

using namespace ss::application;
using namespace ss::core;

/**
 * @brief Simple Chat Application
 * 
 * Demonstrates integration of all application layer components:
 * P2P Node, Message Bus, Chat Service, and Service Interface.
 */
class simple_chat_app {
public:
    /**
     * @brief Constructor
     * @param io_context Boost.Asio IO context
     */
    explicit simple_chat_app(boost::asio::io_context& io_context)
        : io_context_(io_context)
        , running_(false)
        , logger_(std::make_shared<ss_logger>("ChatApp"))
    {}

    /**
     * @brief Initialize the chat application
     * @param config_file Path to configuration file
     * @return true if successful, false otherwise
     */
    bool initialize(const std::string& config_file = "chat_config.json") {
        try {
            // Load configuration
            if (!load_configuration(config_file)) {
                logger_->error("Failed to load configuration");
                return false;
            }

            // Create P2P node
            p2p_node_ = create_p2p_node(io_context_, node_config_);
            if (!p2p_node_) {
                logger_->error("Failed to create P2P node");
                return false;
            }

            // Create message bus
            message_bus_ = create_message_bus(io_context_, bus_config_);
            if (!message_bus_) {
                logger_->error("Failed to create message bus");
                return false;
            }

            // Create chat service
            chat_service_ = create_chat_service(io_context_, chat_config_);
            if (!chat_service_) {
                logger_->error("Failed to create chat service");
                return false;
            }

            // Setup event handlers
            setup_event_handlers();

            logger_->info("Chat application initialized successfully");
            return true;

        } catch (const std::exception& e) {
            logger_->error("Initialization error: " + std::string(e.what()));
            return false;
        }
    }

    /**
     * @brief Start the chat application
     */
    boost::asio::awaitable<bool> start() {
        try {
            logger_->info("Starting chat application...");

            // Initialize components
            co_await p2p_node_->initialize(node_config_);
            co_await message_bus_->initialize(bus_config_);
            co_await chat_service_->initialize(chat_config_, p2p_node_, message_bus_);

            // Start components in order
            co_await p2p_node_->start();
            co_await message_bus_->start();
            co_await chat_service_->start();

            running_.store(true);

            logger_->info("Chat application started successfully");
            logger_->info("Node ID: " + p2p_node_->get_node_id().to_hex());
            logger_->info("Local endpoint: " + p2p_node_->get_local_endpoint().to_string());

            co_return true;

        } catch (const std::exception& e) {
            logger_->error("Start error: " + std::string(e.what()));
            co_return false;
        }
    }

    /**
     * @brief Stop the chat application
     */
    boost::asio::awaitable<void> stop() {
        try {
            logger_->info("Stopping chat application...");

            running_.store(false);

            // Stop components in reverse order
            if (chat_service_) {
                co_await chat_service_->stop();
            }
            if (message_bus_) {
                co_await message_bus_->stop();
            }
            if (p2p_node_) {
                co_await p2p_node_->stop();
            }

            logger_->info("Chat application stopped");

        } catch (const std::exception& e) {
            logger_->error("Stop error: " + std::string(e.what()));
        }
    }

    /**
     * @brief Run interactive command line interface
     */
    void run_cli() {
        if (!running_.load()) {
            std::cout << "Application not running\n";
            return;
        }

        std::cout << "\n=== SS P2P Chat Application ===\n";
        std::cout << "Type 'help' for available commands\n\n";

        // Register user
        std::string nickname;
        std::cout << "Enter your nickname: ";
        std::getline(std::cin, nickname);

        if (!nickname.empty()) {
            boost::asio::co_spawn(io_context_,
                [this, nickname]() -> boost::asio::awaitable<void> {
                    auto user = co_await chat_service_->register_user(nickname);
                    if (!user.nickname.empty()) {
                        std::cout << "Registered as: " << user.nickname << "\n";
                    } else {
                        std::cout << "Failed to register user\n";
                    }
                },
                boost::asio::detached
            );
        }

        // Command loop
        std::string input;
        while (running_.load() && std::getline(std::cin, input)) {
            if (input.empty()) continue;

            if (input == "quit" || input == "exit") {
                break;
            }

            process_cli_command(input);
        }
    }

    /**
     * @brief Get application status
     */
    nlohmann::json get_status() const {
        nlohmann::json status;

        status["running"] = running_.load();
        
        if (p2p_node_) {
            status["p2p_node"] = nlohmann::json::parse(p2p_node_->status());
        }
        
        if (message_bus_) {
            status["message_bus"] = nlohmann::json::parse(message_bus_->status());
        }
        
        if (chat_service_) {
            status["chat_service"] = nlohmann::json::parse(chat_service_->status());
        }

        return status;
    }

private:
    /**
     * @brief Load configuration from file
     */
    bool load_configuration(const std::string& config_file) {
        try {
            // Create default configuration if file doesn't exist
            if (!std::filesystem::exists(config_file)) {
                create_default_configuration(config_file);
            }

            std::ifstream file(config_file);
            if (!file.is_open()) {
                logger_->error("Cannot open config file: " + config_file);
                return false;
            }

            nlohmann::json config_json;
            file >> config_json;

            // Parse P2P node configuration
            if (config_json.contains("p2p_node")) {
                auto node_result = node_config::from_json(config_json["p2p_node"]);
                if (node_result.is_ok()) {
                    node_config_ = node_result.value();
                } else {
                    logger_->error("Invalid P2P node config: " + node_result.error());
                    return false;
                }
            }

            // Parse message bus configuration
            if (config_json.contains("message_bus")) {
                auto bus_result = message_bus_config::from_json(config_json["message_bus"]);
                if (bus_result.is_ok()) {
                    bus_config_ = bus_result.value();
                } else {
                    logger_->error("Invalid message bus config: " + bus_result.error());
                    return false;
                }
            }

            // Parse chat configuration
            if (config_json.contains("chat")) {
                auto chat_result = chat_config::from_json(config_json["chat"]);
                if (chat_result.is_ok()) {
                    chat_config_ = chat_result.value();
                } else {
                    logger_->error("Invalid chat config: " + chat_result.error());
                    return false;
                }
            }

            logger_->info("Configuration loaded from: " + config_file);
            return true;

        } catch (const std::exception& e) {
            logger_->error("Config loading error: " + std::string(e.what()));
            return false;
        }
    }

    /**
     * @brief Create default configuration file
     */
    void create_default_configuration(const std::string& config_file) {
        try {
            nlohmann::json config;

            // Default P2P node configuration
            config["p2p_node"] = {
                {"local_endpoint", {
                    {"address", "127.0.0.1"},
                    {"port", 8080}
                }},
                {"bootstrap_nodes", nlohmann::json::array()},
                {"max_connections", 1000},
                {"connection_timeout_ms", 5000},
                {"dht_refresh_interval_ms", 60000},
                {"enable_nat_traversal", true},
                {"enable_encryption", true},
                {"log_level", 2}
            };

            // Default message bus configuration
            config["message_bus"] = {
                {"max_queue_size", 10000},
                {"message_ttl_ms", 300000},
                {"enable_persistence", false},
                {"worker_threads", 4}
            };

            // Default chat configuration
            config["chat"] = {
                {"max_message_length", 4096},
                {"max_history_size", 10000},
                {"enable_encryption", true},
                {"enable_persistence", true},
                {"enable_typing_indicators", true},
                {"enable_presence", true}
            };

            std::ofstream file(config_file);
            file << config.dump(2);
            file.close();

            std::cout << "Created default configuration file: " << config_file << "\n";
            std::cout << "Please edit the configuration and restart the application.\n";

        } catch (const std::exception& e) {
            logger_->error("Error creating default config: " + std::string(e.what()));
        }
    }

    /**
     * @brief Setup event handlers for chat service
     */
    void setup_event_handlers() {
        if (!chat_service_) return;

        // Message received handler
        chat_service_->set_message_received_handler(
            [this](const chat_message& message) {
                std::cout << "\n[" << message.sender_nickname << "] " << message.content << "\n> ";
                std::cout.flush();
            }
        );

        // User joined handler
        chat_service_->set_user_joined_handler(
            [this](const user_info& user, const std::string& room_id) {
                std::cout << "\n*** " << user.nickname << " joined room " << room_id << " ***\n> ";
                std::cout.flush();
            }
        );

        // User left handler
        chat_service_->set_user_left_handler(
            [this](const core::node_id& user_id, const std::string& room_id) {
                std::cout << "\n*** User left room " << room_id << " ***\n> ";
                std::cout.flush();
            }
        );

        // Typing indicator handler
        chat_service_->set_typing_indicator_handler(
            [this](const core::node_id& user_id, const std::string& room_id, bool typing) {
                if (typing) {
                    std::cout << "\n*** Someone is typing in " << room_id << " ***\n> ";
                    std::cout.flush();
                }
            }
        );
    }

    /**
     * @brief Process CLI command
     */
    void process_cli_command(const std::string& input) {
        std::istringstream iss(input);
        std::string command;
        iss >> command;

        if (command == "help") {
            show_help();
        }
        else if (command == "status") {
            std::cout << get_status().dump(2) << "\n";
        }
        else if (command == "rooms") {
            list_rooms();
        }
        else if (command == "users") {
            list_users();
        }
        else if (command == "create") {
            std::string room_name;
            iss >> room_name;
            if (!room_name.empty()) {
                create_room(room_name);
            } else {
                std::cout << "Usage: create <room_name>\n";
            }
        }
        else if (command == "join") {
            std::string room_id;
            iss >> room_id;
            if (!room_id.empty()) {
                join_room(room_id);
            } else {
                std::cout << "Usage: join <room_id>\n";
            }
        }
        else if (command == "leave") {
            std::string room_id;
            iss >> room_id;
            if (!room_id.empty()) {
                leave_room(room_id);
            } else {
                std::cout << "Usage: leave <room_id>\n";
            }
        }
        else if (command == "msg") {
            std::string room_id;
            iss >> room_id;
            
            std::string message;
            std::getline(iss, message);
            if (!message.empty()) {
                message = message.substr(1); // Remove leading space
                send_message(room_id, message);
            } else {
                std::cout << "Usage: msg <room_id> <message>\n";
            }
        }
        else if (command == "pm") {
            std::string nickname;
            iss >> nickname;
            
            std::string message;
            std::getline(iss, message);
            if (!message.empty()) {
                message = message.substr(1); // Remove leading space
                send_private_message(nickname, message);
            } else {
                std::cout << "Usage: pm <nickname> <message>\n";
            }
        }
        else {
            std::cout << "Unknown command: " << command << "\n";
            std::cout << "Type 'help' for available commands\n";
        }

        std::cout << "> ";
        std::cout.flush();
    }

    /**
     * @brief Show help information
     */
    void show_help() {
        std::cout << "\nAvailable commands:\n";
        std::cout << "  help                    - Show this help\n";
        std::cout << "  status                  - Show application status\n";
        std::cout << "  rooms                   - List available rooms\n";
        std::cout << "  users                   - List online users\n";
        std::cout << "  create <room_name>      - Create a new room\n";
        std::cout << "  join <room_id>          - Join a room\n";
        std::cout << "  leave <room_id>         - Leave a room\n";
        std::cout << "  msg <room_id> <message> - Send message to room\n";
        std::cout << "  pm <nickname> <message> - Send private message\n";
        std::cout << "  quit/exit               - Exit application\n\n";
    }

    /**
     * @brief List available rooms
     */
    void list_rooms() {
        if (!chat_service_) return;

        auto rooms = chat_service_->get_rooms();
        if (rooms.empty()) {
            std::cout << "No rooms available\n";
            return;
        }

        std::cout << "\nAvailable rooms:\n";
        for (const auto& room : rooms) {
            std::cout << "  " << room.room_id << " - " << room.name 
                      << " (" << room.user_count << " users)\n";
        }
        std::cout << "\n";
    }

    /**
     * @brief List online users
     */
    void list_users() {
        if (!chat_service_) return;

        auto users = chat_service_->get_online_users();
        if (users.empty()) {
            std::cout << "No users online\n";
            return;
        }

        std::cout << "\nOnline users:\n";
        for (const auto& user : users) {
            std::cout << "  " << user.nickname << " - " 
                      << static_cast<int>(user.status) << "\n";
        }
        std::cout << "\n";
    }

    /**
     * @brief Create a new room
     */
    void create_room(const std::string& room_name) {
        if (!chat_service_) return;

        boost::asio::co_spawn(io_context_,
            [this, room_name]() -> boost::asio::awaitable<void> {
                auto room_id = co_await chat_service_->create_room(room_name);
                if (!room_id.empty()) {
                    std::cout << "Created room: " << room_id << "\n";
                } else {
                    std::cout << "Failed to create room\n";
                }
            },
            boost::asio::detached
        );
    }

    /**
     * @brief Join a room
     */
    void join_room(const std::string& room_id) {
        if (!chat_service_) return;

        boost::asio::co_spawn(io_context_,
            [this, room_id]() -> boost::asio::awaitable<void> {
                auto success = co_await chat_service_->join_room(room_id);
                if (success) {
                    std::cout << "Joined room: " << room_id << "\n";
                } else {
                    std::cout << "Failed to join room: " << room_id << "\n";
                }
            },
            boost::asio::detached
        );
    }

    /**
     * @brief Leave a room
     */
    void leave_room(const std::string& room_id) {
        if (!chat_service_) return;

        boost::asio::co_spawn(io_context_,
            [this, room_id]() -> boost::asio::awaitable<void> {
                auto success = co_await chat_service_->leave_room(room_id);
                if (success) {
                    std::cout << "Left room: " << room_id << "\n";
                } else {
                    std::cout << "Failed to leave room: " << room_id << "\n";
                }
            },
            boost::asio::detached
        );
    }

    /**
     * @brief Send message to room
     */
    void send_message(const std::string& room_id, const std::string& message) {
        if (!chat_service_) return;

        boost::asio::co_spawn(io_context_,
            [this, room_id, message]() -> boost::asio::awaitable<void> {
                auto msg_id = co_await chat_service_->send_message(room_id, message);
                if (msg_id.value() != 0) {
                    std::cout << "Message sent to " << room_id << "\n";
                } else {
                    std::cout << "Failed to send message\n";
                }
            },
            boost::asio::detached
        );
    }

    /**
     * @brief Send private message
     */
    void send_private_message(const std::string& nickname, const std::string& message) {
        if (!chat_service_) return;

        auto user = chat_service_->get_user_by_nickname(nickname);
        if (!user) {
            std::cout << "User not found: " << nickname << "\n";
            return;
        }

        boost::asio::co_spawn(io_context_,
            [this, user, message]() -> boost::asio::awaitable<void> {
                auto msg_id = co_await chat_service_->send_private_message(user->user_id, message);
                if (msg_id.value() != 0) {
                    std::cout << "Private message sent to " << user->nickname << "\n";
                } else {
                    std::cout << "Failed to send private message\n";
                }
            },
            boost::asio::detached
        );
    }

    // Core components
    boost::asio::io_context& io_context_;
    std::unique_ptr<i_p2p_node> p2p_node_;
    std::unique_ptr<i_message_bus> message_bus_;
    std::unique_ptr<i_chat> chat_service_;

    // Configuration
    node_config node_config_;
    message_bus_config bus_config_;
    chat_config chat_config_;

    // State
    std::atomic<bool> running_;
    std::shared_ptr<ss_logger> logger_;
};

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    try {
        std::cout << "SS P2P Chat Application v1.0\n";
        std::cout << "============================\n\n";

        // Parse command line arguments
        std::string config_file = "chat_config.json";
        bool daemon_mode = false;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--config" && i + 1 < argc) {
                config_file = argv[++i];
            }
            else if (arg == "--daemon") {
                daemon_mode = true;
            }
            else if (arg == "--help") {
                std::cout << "Usage: " << argv[0] << " [options]\n";
                std::cout << "Options:\n";
                std::cout << "  --config <file>  Configuration file path\n";
                std::cout << "  --daemon         Run in daemon mode\n";
                std::cout << "  --help           Show this help\n";
                return 0;
            }
        }

        // Create IO context
        boost::asio::io_context io_context;

        // Create chat application
        simple_chat_app app(io_context);

        // Initialize application
        if (!app.initialize(config_file)) {
            std::cerr << "Failed to initialize application\n";
            return 1;
        }

        // Start application
        bool started = false;
        boost::asio::co_spawn(io_context,
            [&app, &started]() -> boost::asio::awaitable<void> {
                started = co_await app.start();
            },
            boost::asio::detached
        );

        // Run IO context in background thread
        std::thread io_thread([&io_context]() {
            io_context.run();
        });

        // Wait for startup
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        if (!started) {
            std::cerr << "Failed to start application\n";
            io_context.stop();
            if (io_thread.joinable()) {
                io_thread.join();
            }
            return 1;
        }

        if (daemon_mode) {
            std::cout << "Running in daemon mode. Press Ctrl+C to stop.\n";
            
            // Setup signal handling
            boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
            signals.async_wait([&app, &io_context](const boost::system::error_code&, int) {
                boost::asio::co_spawn(io_context,
                    [&app]() -> boost::asio::awaitable<void> {
                        co_await app.stop();
                    },
                    boost::asio::detached
                );
            });

            // Wait for termination
            if (io_thread.joinable()) {
                io_thread.join();
            }
        } else {
            // Run interactive CLI
            app.run_cli();

            // Stop application
            boost::asio::co_spawn(io_context,
                [&app]() -> boost::asio::awaitable<void> {
                    co_await app.stop();
                },
                boost::asio::detached
            );

            // Stop IO context
            io_context.stop();
            if (io_thread.joinable()) {
                io_thread.join();
            }
        }

        std::cout << "Application terminated.\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Application error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Unknown application error\n";
        return 2;
    }
}