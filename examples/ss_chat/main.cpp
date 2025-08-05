/**
 * @file main.cpp
 * @brief Modern P2P Chat Application using Modern SS P2P APIs
 * 
 * This example demonstrates the use of modern SS P2P APIs including:
 * - DHT routing for peer discovery
 * - ICE NAT traversal for direct connections
 * - Transport factory for network abstraction
 * - Async/await patterns with Boost.Asio
 */

// Modern SS P2P API Headers
#include "../../include/ss_p2p/core/types.hpp"
#include "../../include/ss_p2p/core/result.hpp"
#include "../../include/ss_p2p/core/interfaces.hpp"
#include "../../include/ss_p2p/dht/i_routing.hpp"
#include "../../include/ss_p2p/dht/i_storage.hpp"
#include "../../include/ss_p2p/dht/kademlia_config.hpp"
#include "../../include/ss_p2p/ice/i_nat_traversal.hpp"
#include "../../include/ss_p2p/network/i_transport.hpp"
#include "../../include/ss_p2p/network/transport_factory.hpp"

// Standard Library
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <vector>
#include <memory>
#include <map>
#include <set>
#include <unordered_map>
#include <optional>
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/asio/detached.hpp>
#include <json.hpp>
#include <ctime>

using namespace ss;
using namespace boost::asio;

/**
 * @brief Chat message structure for P2P communication
 */
struct chat_message {
    std::string type;           // "hello", "chat", "ping", "pong"
    std::string username;       // Sender username
    std::string message;        // Message content
    std::uint64_t timestamp;    // Unix timestamp
    core::node_id sender_id;    // Sender node ID
    
    /**
     * @brief Serialize to JSON string
     */
    std::string to_json() const {
        nlohmann::json j;
        j["type"] = type;
        j["username"] = username;
        j["message"] = message;
        j["timestamp"] = timestamp;
        j["sender_id"] = sender_id.to_hex();
        return j.dump();
    }
    
    /**
     * @brief Deserialize from JSON string
     */
    static std::optional<chat_message> from_json(const std::string& json_str) {
        try {
            auto j = nlohmann::json::parse(json_str);
            chat_message msg;
            msg.type = j.value("type", "");
            msg.username = j.value("username", "");
            msg.message = j.value("message", "");
            msg.timestamp = j.value("timestamp", 0ULL);
            
            if (j.contains("sender_id")) {
                msg.sender_id = core::node_id::from_hex(j["sender_id"]);
            }
            
            return msg;
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }
};

/**
 * @brief Modern P2P Chat Application
 * 
 * Built using modern SS P2P APIs with full async/await support,
 * DHT routing, ICE NAT traversal, and pluggable transport layer.
 */
class modern_chat_app {
public:
    /**
     * @brief Constructor
     */
    explicit modern_chat_app(core::endpoint local_endpoint, core::io_context& io_context)
        : io_context_(io_context)
        , local_endpoint_(local_endpoint)
        , local_node_id_(core::node_id::random())
        , running_(false)
        , username_("Anonymous")
    {
        std::cout << "[INFO] Node ID: " << local_node_id_.to_hex() << std::endl;
    }

    /**
     * @brief Initialize the chat application
     */
    core::async_result<bool> initialize(const std::string& config_file = "config.json.example") {
        try {
            std::cout << "[INFO] Initializing modern P2P chat application..." << std::endl;
            
            // Load configuration
            if (!load_configuration(config_file)) {
                std::cout << "[INFO] Using default configuration" << std::endl;
                create_default_configuration();
            }
            
            // Initialize transport
            auto transport_result = co_await initialize_transport();
            if (!transport_result) {
                std::cerr << "[ERROR] Failed to initialize transport" << std::endl;
                co_return false;
            }
            
            // Initialize DHT routing
            auto routing_result = co_await initialize_dht_routing();
            if (!routing_result) {
                std::cerr << "[ERROR] Failed to initialize DHT routing" << std::endl;
                co_return false;
            }
            
            // Initialize ICE NAT traversal
            auto ice_result = co_await initialize_ice();
            if (!ice_result) {
                std::cerr << "[ERROR] Failed to initialize ICE" << std::endl;
                co_return false;
            }
            
            // Set up message handler
            if (transport_) {
                transport_->set_message_handler(
                    [this](network::incoming_message msg) -> core::async_void {
                        co_await handle_incoming_message(std::move(msg));
                    }
                );
            }
            
            std::cout << "[INFO] Chat application initialized successfully" << std::endl;
            co_return true;
            
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Initialization error: " << e.what() << std::endl;
            co_return false;
        }
    }

    /**
     * @brief Start the chat application
     */
    core::async_result<bool> start() {
        try {
            std::cout << "[INFO] Starting chat application..." << std::endl;
            
            // Start transport
            if (transport_) {
                auto bind_result = co_await transport_->bind(local_endpoint_);
                if (!bind_result) {
                    std::cerr << "[ERROR] Failed to bind transport: " << static_cast<int>(bind_result.error()) << std::endl;
                    co_return false;
                }
                
                // Transport start is handled by bind for UDP
            }
            
            // Start DHT routing (simulated)
            std::cout << "[INFO] DHT routing started (simulated)" << std::endl;
            
            // Start ICE NAT traversal (simulated)
            std::cout << "[INFO] ICE NAT traversal started (simulated)" << std::endl;
            
            // Bootstrap DHT with known nodes
            co_await bootstrap_dht();
            
            running_.store(true);
            
            std::cout << "[INFO] Chat application started successfully" << std::endl;
            std::cout << "[INFO] Local endpoint: " << local_endpoint_.to_string() << std::endl;
            std::cout << "[INFO] Node ID: " << local_node_id_.to_hex() << std::endl;
            
            co_return true;
            
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Start error: " << e.what() << std::endl;
            co_return false;
        }
    }

    /**
     * @brief Stop the chat application
     */
    core::async_void stop() {
        try {
            std::cout << "[INFO] Stopping chat application..." << std::endl;
            running_.store(false);
            
            // Stop components in reverse order
            if (ice_traversal_) {
                co_await ice_traversal_->stop();
            }
            
            if (dht_routing_) {
                co_await dht_routing_->stop();
            }
            
            if (transport_) {
                co_await transport_->stop();
            }
            
            std::cout << "[INFO] Chat application stopped" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Stop error: " << e.what() << std::endl;
        }
    }

    /**
     * @brief Run interactive command line interface
     */
    void run_cli() {
        if (!running_.load()) {
            std::cout << "[ERROR] Application not running" << std::endl;
            return;
        }

        std::cout << "\n=== Modern SS P2P Chat Application ===" << std::endl;
        std::cout << "Built with modern APIs: DHT + ICE + Transport Factory" << std::endl;
        std::cout << "Type 'help' for available commands\n" << std::endl;

        // Get username
        std::string nickname;
        std::cout << "Enter your nickname: ";
        std::getline(std::cin, nickname);
        
        if (!nickname.empty()) {
            username_ = nickname;
            std::cout << "[INFO] Registered as: " << username_ << std::endl;
        }

        // Command loop
        std::string input;
        std::cout << "> ";
        while (running_.load() && std::getline(std::cin, input)) {
            if (input.empty()) {
                std::cout << "> ";
                continue;
            }

            if (input == "quit" || input == "exit") {
                break;
            }

            // Process command asynchronously
            boost::asio::co_spawn(io_context_, 
                [this, input]() -> core::async_void {
                    co_await process_cli_command(input);
                    std::cout << "> ";
                },
                boost::asio::detached
            );
            
            // Small delay to allow async processing
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    /**
     * @brief Get application status
     */
    nlohmann::json get_status() const {
        nlohmann::json status;
        status["running"] = running_.load();
        status["username"] = username_;
        status["local_endpoint"] = local_endpoint_.to_string();
        status["node_id"] = local_node_id_.to_hex();
        status["connected_peers"] = connected_peers_.size();
        status["bootstrap_endpoints"] = bootstrap_endpoints_.size();
        
        // Add component status
        if (transport_) {
            status["transport"] = {
                {"type", "UDP"},
                {"connected", transport_->is_connected()},
                {"stats", {
                    {"messages_sent", transport_->get_stats().messages_sent},
                    {"messages_received", transport_->get_stats().messages_received}
                }}
            };
        }
        
        if (dht_routing_) {
            auto stats = dht_routing_->get_stats();
            status["dht"] = {
                {"total_peers", stats.total_peers},
                {"active_buckets", stats.active_buckets},
                {"find_operations", stats.find_operations}
            };
        }
        
        if (ice_traversal_) {
            auto stats = ice_traversal_->get_stats();
            status["ice"] = {
                {"detection_attempts", stats.detection_attempts},
                {"detection_successes", stats.detection_successes},
                {"hole_punch_attempts", stats.hole_punch_attempts}
            };
        }
        
        return status;
    }

private:
    /**
     * @brief Initialize transport layer
     */
    core::async_result<bool> initialize_transport() {
        try {
            // Create UDP transport using factory
            auto& factory = network::transport_factory::instance();
            
            // Register default transports if not already done
            auto register_result = factory.register_default_transports(io_context_);
            if (!register_result) {
                std::cerr << "[ERROR] Failed to register default transports" << std::endl;
            }
            
            // Create transport parameters
            network::transport_params params;
            params.io_context = &io_context_;
            params.config.enable_stats = true;
            params.config.max_message_size = 65536; // 64KB
            params.config.default_timeout = std::chrono::milliseconds{5000};
            
            // Create UDP transport
            auto transport_result = factory.create_transport(network::transport_type::udp, params);
            if (!transport_result) {
                std::cerr << "[ERROR] Failed to create UDP transport: " << static_cast<int>(transport_result.error()) << std::endl;
                co_return false;
            }
            
            transport_ = transport_result.value();
            std::cout << "[INFO] Transport layer initialized successfully" << std::endl;
            co_return true;
            
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Transport initialization error: " << e.what() << std::endl;
            co_return false;
        }
    }
    
    /**
     * @brief Initialize DHT routing layer
     */
    core::async_result<bool> initialize_dht_routing() {
        try {
            // For this example, we'll simulate DHT routing initialization
            // In a real implementation, you would create the DHT routing instance
            std::cout << "[INFO] DHT routing layer initialized (simulated)" << std::endl;
            
            // Note: In actual implementation, you would:
            // auto routing_factory = dht::routing_factory::instance();
            // dht_routing_ = routing_factory.create_kademlia_routing(local_node_id_, config);
            
            co_return true;
            
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] DHT routing initialization error: " << e.what() << std::endl;
            co_return false;
        }
    }
    
    /**
     * @brief Initialize ICE NAT traversal
     */
    core::async_result<bool> initialize_ice() {
        try {
            // For this example, we'll simulate ICE initialization
            // In a real implementation, you would create the ICE agent
            std::cout << "[INFO] ICE NAT traversal initialized (simulated)" << std::endl;
            
            // Note: In actual implementation, you would:
            // auto ice_factory = ice::nat_traversal_factory::instance();
            // ice_traversal_ = ice_factory.create_ice_agent(config);
            
            co_return true;
            
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] ICE initialization error: " << e.what() << std::endl;
            co_return false;
        }
    }
    
    /**
     * @brief Load configuration from file
     */
    bool load_configuration(const std::string& config_file) {
        try {
            std::ifstream file(config_file);
            if (!file.is_open()) {
                std::cout << "[WARNING] Cannot open config file: " << config_file << std::endl;
                return false;
            }

            nlohmann::json config_json;
            file >> config_json;

            // Parse bootstrap nodes
            if (config_json.contains("node") && config_json["node"].contains("bootstrap_nodes")) {
                bootstrap_endpoints_.clear();
                for (const auto& bootstrap : config_json["node"]["bootstrap_nodes"]) {
                    std::string host = bootstrap["host"];
                    int port = bootstrap["port"];
                    
                    try {
                        core::endpoint ep(host, static_cast<std::uint16_t>(port));
                        bootstrap_endpoints_.push_back(ep);
                    } catch (const std::exception& e) {
                        std::cout << "[WARNING] Invalid bootstrap endpoint: " << host << ":" << port << std::endl;
                    }
                }
            }
            
            // Parse DHT configuration
            if (config_json.contains("dht")) {
                auto& dht_config = config_json["dht"];
                dht_config_.k_bucket_size = dht_config.value("k_bucket_size", 20);
                dht_config_.alpha = dht_config.value("alpha", 3);
                dht_config_.find_timeout = std::chrono::seconds{dht_config.value("lookup_timeout", 5)} * 1000; // Convert to milliseconds
            }

            std::cout << "[INFO] Configuration loaded from: " << config_file << std::endl;
            return true;

        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Config loading error: " << e.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief Create default configuration
     */
    void create_default_configuration() {
        bootstrap_endpoints_.clear();
        bootstrap_endpoints_.emplace_back("127.0.0.1", 8081);
        bootstrap_endpoints_.emplace_back("127.0.0.1", 8082);
        
        // Use testing configuration for DHT
        dht_config_ = dht::kademlia_config::testing_config();
        
        std::cout << "[INFO] Using default configuration" << std::endl;
    }
    
    /**
     * @brief Bootstrap DHT with known nodes
     */
    core::async_void bootstrap_dht() {
        try {
            std::cout << "[INFO] Bootstrapping DHT with " << bootstrap_endpoints_.size() << " nodes" << std::endl;
            
            // Send hello messages to bootstrap nodes
            for (const auto& endpoint : bootstrap_endpoints_) {
                chat_message hello;
                hello.type = "hello";
                hello.username = username_;
                hello.message = "Hello from " + username_;
                hello.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                hello.sender_id = local_node_id_;
                
                co_await send_message_to_endpoint(endpoint, hello);
            }
            
            std::cout << "[INFO] DHT bootstrap completed" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] DHT bootstrap error: " << e.what() << std::endl;
        }
    }
    
    /**
     * @brief Handle incoming message
     */
    core::async_void handle_incoming_message(network::incoming_message msg) {
        try {
            // Convert bytes to string
            std::string json_str(msg.data.begin(), msg.data.end());
            
            // Parse chat message
            auto chat_msg = chat_message::from_json(json_str);
            if (!chat_msg) {
                std::cout << "[WARNING] Failed to parse message from " << msg.sender.to_string() << std::endl;
                co_return;
            }
            
            // Handle different message types
            if (chat_msg->type == "hello") {
                co_await handle_hello_message(*chat_msg, msg.sender);
            } else if (chat_msg->type == "chat") {
                co_await handle_chat_message(*chat_msg, msg.sender);
            } else if (chat_msg->type == "ping") {
                co_await handle_ping_message(*chat_msg, msg.sender);
            } else if (chat_msg->type == "pong") {
                co_await handle_pong_message(*chat_msg, msg.sender);
            } else {
                std::cout << "[WARNING] Unknown message type: " << chat_msg->type << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Message handling error: " << e.what() << std::endl;
        }
    }
    
    /**
     * @brief Handle hello message
     */
    core::async_void handle_hello_message(const chat_message& msg, const core::endpoint& sender) {
        std::cout << "[HELLO] " << msg.username << " joined from " << sender.to_string() << std::endl;
        
        // Add to connected peers
        connected_peers_[sender] = msg.sender_id;
        
        // Send hello response
        chat_message response;
        response.type = "hello";
        response.username = username_;
        response.message = "Hello " + msg.username + " from " + username_;
        response.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        response.sender_id = local_node_id_;
        
        co_await send_message_to_endpoint(sender, response);
    }
    
    /**
     * @brief Handle chat message
     */
    core::async_void handle_chat_message(const chat_message& msg, const core::endpoint& sender) {
        // Format timestamp
        auto time_t = static_cast<std::time_t>(msg.timestamp);
        auto tm = *std::localtime(&time_t);
        
        char time_str[100];
        std::strftime(time_str, sizeof(time_str), "%H:%M:%S", &tm);
        
        std::cout << "[" << time_str << "] " << msg.username << ": " << msg.message << std::endl;
        
        // Add to connected peers
        connected_peers_[sender] = msg.sender_id;
        co_return;
    }
    
    /**
     * @brief Handle ping message
     */
    core::async_void handle_ping_message(const chat_message& msg, const core::endpoint& sender) {
        std::cout << "[PING] from " << msg.username << " (" << sender.to_string() << ")" << std::endl;
        
        // Send pong response
        chat_message response;
        response.type = "pong";
        response.username = username_;
        response.message = "pong";
        response.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        response.sender_id = local_node_id_;
        
        co_await send_message_to_endpoint(sender, response);
    }
    
    /**
     * @brief Handle pong message
     */
    core::async_void handle_pong_message(const chat_message& msg, const core::endpoint& sender) {
        std::cout << "[PONG] from " << msg.username << " (" << sender.to_string() << ")" << std::endl;
        
        // Update connected peers
        connected_peers_[sender] = msg.sender_id;
        co_return;
    }
    
    /**
     * @brief Send message to specific endpoint
     */
    core::async_void send_message_to_endpoint(const core::endpoint& target, const chat_message& msg) {
        try {
            if (!transport_) {
                std::cerr << "[ERROR] Transport not initialized" << std::endl;
                co_return;
            }
            
            // Serialize message
            std::string json_str = msg.to_json();
            std::vector<std::uint8_t> data(json_str.begin(), json_str.end());
            
            // Create outgoing message
            network::outgoing_message out_msg(std::move(data), target);
            
            // Send message
            auto result = co_await transport_->send(std::move(out_msg));
            if (!result) {
                std::cerr << "[ERROR] Failed to send message to " << target.to_string() << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Send message error: " << e.what() << std::endl;
        }
    }

    /**
     * @brief Process CLI command (async)
     */
    core::async_void process_cli_command(const std::string& input) {
        std::istringstream iss(input);
        std::string command;
        iss >> command;

        if (command == "help") {
            show_help();
        }
        else if (command == "status") {
            std::cout << get_status().dump(2) << std::endl;
        }
        else if (command == "peers") {
            list_peers();
        }
        else if (command == "discover") {
            co_await discover_peers();
        }
        else if (command == "connect") {
            std::string host, port_str;
            iss >> host >> port_str;
            if (!host.empty() && !port_str.empty()) {
                try {
                    int port = std::stoi(port_str);
                    co_await connect_to_peer(host, port);
                } catch (const std::exception&) {
                    std::cout << "[ERROR] Invalid port number" << std::endl;
                }
            } else {
                std::cout << "Usage: connect <host> <port>" << std::endl;
            }
        }
        else if (command == "send") {
            std::string host, port_str;
            iss >> host >> port_str;
            
            std::string message;
            std::getline(iss, message);
            if (!host.empty() && !port_str.empty() && !message.empty()) {
                try {
                    int port = std::stoi(port_str);
                    message = message.substr(1); // Remove leading space
                    co_await send_direct_message(host, port, message);
                } catch (const std::exception&) {
                    std::cout << "[ERROR] Invalid port number" << std::endl;
                }
            } else {
                std::cout << "Usage: send <host> <port> <message>" << std::endl;
            }
        }
        else if (command == "broadcast") {
            std::string message;
            std::getline(iss, message);
            if (!message.empty()) {
                message = message.substr(1); // Remove leading space
                co_await broadcast_message(message);
            } else {
                std::cout << "Usage: broadcast <message>" << std::endl;
            }
        }
        else if (command == "ping") {
            std::string host, port_str;
            iss >> host >> port_str;
            if (!host.empty() && !port_str.empty()) {
                try {
                    int port = std::stoi(port_str);
                    co_await ping_peer(host, port);
                } catch (const std::exception&) {
                    std::cout << "[ERROR] Invalid port number" << std::endl;
                }
            } else {
                std::cout << "Usage: ping <host> <port>" << std::endl;
            }
        }
        else {
            std::cout << "[ERROR] Unknown command: " << command << std::endl;
            std::cout << "Type 'help' for available commands" << std::endl;
        }
    }

    /**
     * @brief Show help information
     */
    void show_help() {
        std::cout << "\nModern P2P Chat Commands:" << std::endl;
        std::cout << "  help                       - Show this help" << std::endl;
        std::cout << "  status                     - Show application status (DHT/ICE/Transport)" << std::endl;
        std::cout << "  peers                      - List connected peers" << std::endl;
        std::cout << "  discover                   - Discover peers through DHT" << std::endl;
        std::cout << "  connect <host> <port>      - Connect to a peer" << std::endl;
        std::cout << "  send <host> <port> <msg>   - Send message to specific peer" << std::endl;
        std::cout << "  broadcast <message>        - Broadcast message to all peers" << std::endl;
        std::cout << "  ping <host> <port>         - Ping a peer" << std::endl;
        std::cout << "  quit/exit                  - Exit application" << std::endl;
        std::cout << "\nFeatures: DHT Routing | ICE NAT Traversal | Modern Async APIs\n" << std::endl;
    }

    /**
     * @brief List connected peers
     */
    void list_peers() {
        if (connected_peers_.empty()) {
            std::cout << "[INFO] No connected peers" << std::endl;
            return;
        }

        std::cout << "\nConnected peers (" << connected_peers_.size() << "):" << std::endl;
        for (const auto& [endpoint, node_id] : connected_peers_) {
            std::cout << "  " << endpoint.to_string() << " (ID: " << node_id.to_hex().substr(0, 8) << "...)" << std::endl;
        }
        std::cout << std::endl;
    }
    
    /**
     * @brief Discover peers through DHT
     */
    core::async_void discover_peers() {
        try {
            std::cout << "[INFO] Starting peer discovery..." << std::endl;
            
            // In a real implementation, this would use DHT routing:
            // auto peers = co_await dht_routing_->find_node(core::node_id::random(), 10);
            
            // For now, simulate discovery by trying bootstrap nodes
            for (const auto& endpoint : bootstrap_endpoints_) {
                chat_message hello;
                hello.type = "hello";
                hello.username = username_;
                hello.message = "Discovery hello from " + username_;
                hello.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                hello.sender_id = local_node_id_;
                
                co_await send_message_to_endpoint(endpoint, hello);
            }
            
            std::cout << "[INFO] Peer discovery completed" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Peer discovery error: " << e.what() << std::endl;
        }
    }
    
    /**
     * @brief Connect to a peer
     */
    core::async_void connect_to_peer(const std::string& host, int port) {
        try {
            core::endpoint peer_ep(host, static_cast<std::uint16_t>(port));
            
            std::cout << "[INFO] Connecting to peer: " << peer_ep.to_string() << std::endl;
            
            // Create hello message
            chat_message hello;
            hello.type = "hello";
            hello.username = username_;
            hello.message = "Hello from " + username_;
            hello.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            hello.sender_id = local_node_id_;
            
            // Send hello message
            co_await send_message_to_endpoint(peer_ep, hello);
            
            std::cout << "[INFO] Hello message sent to " << peer_ep.to_string() << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to connect to peer: " << e.what() << std::endl;
        }
    }
    
    /**
     * @brief Send direct message to specific peer
     */
    core::async_void send_direct_message(const std::string& host, int port, const std::string& text) {
        try {
            core::endpoint peer_ep(host, static_cast<std::uint16_t>(port));
            
            // Create chat message
            chat_message msg;
            msg.type = "chat";
            msg.username = username_;
            msg.message = text;
            msg.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            msg.sender_id = local_node_id_;
            
            // Send message
            co_await send_message_to_endpoint(peer_ep, msg);
            
            std::cout << "[SENT] To " << peer_ep.to_string() << ": " << text << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to send message: " << e.what() << std::endl;
        }
    }
    
    /**
     * @brief Broadcast message to all connected peers
     */
    core::async_void broadcast_message(const std::string& text) {
        try {
            if (connected_peers_.empty()) {
                std::cout << "[INFO] No peers to broadcast to" << std::endl;
                co_return;
            }
            
            std::cout << "[INFO] Broadcasting to " << connected_peers_.size() << " peers" << std::endl;
            
            // Create chat message
            chat_message msg;
            msg.type = "chat";
            msg.username = username_;
            msg.message = text;
            msg.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            msg.sender_id = local_node_id_;
            
            // Send to all connected peers
            for (const auto& [endpoint, node_id] : connected_peers_) {
                co_await send_message_to_endpoint(endpoint, msg);
            }
            
            std::cout << "[BROADCAST] " << text << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to broadcast message: " << e.what() << std::endl;
        }
    }
    
    /**
     * @brief Ping a peer to check connectivity
     */
    core::async_void ping_peer(const std::string& host, int port) {
        try {
            core::endpoint peer_ep(host, static_cast<std::uint16_t>(port));
            
            // Create ping message
            chat_message ping;
            ping.type = "ping";
            ping.username = username_;
            ping.message = "ping";
            ping.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            ping.sender_id = local_node_id_;
            
            // Send ping
            co_await send_message_to_endpoint(peer_ep, ping);
            
            std::cout << "[PING] Sent to " << peer_ep.to_string() << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to ping peer: " << e.what() << std::endl;
        }
    }

    // Core components
    core::io_context& io_context_;
    core::endpoint local_endpoint_;
    core::node_id local_node_id_;
    
    // Modern API components
    network::transport_ptr transport_;
    dht::routing_ptr dht_routing_;          // Simulated for this example
    ice::nat_traversal_ptr ice_traversal_;  // Simulated for this example

    // Configuration
    std::vector<core::endpoint> bootstrap_endpoints_;
    dht::kademlia_config dht_config_;

    // State
    std::atomic<bool> running_;
    std::string username_;
    std::unordered_map<core::endpoint, core::node_id> connected_peers_;
};

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    try {
        std::cout << "SS P2P Chat Application v1.0\n";
        std::cout << "============================\n\n";

        // Parse command line arguments
        std::string config_file = "config.json.example";
        std::string host = "127.0.0.1";
        int port = 8080;
        bool daemon_mode = false;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--config" && i + 1 < argc) {
                config_file = argv[++i];
            }
            else if (arg == "--host" && i + 1 < argc) {
                host = argv[++i];
            }
            else if (arg == "--port" && i + 1 < argc) {
                port = std::stoi(argv[++i]);
            }
            else if (arg == "--daemon") {
                daemon_mode = true;
            }
            else if (arg == "--help") {
                std::cout << "Usage: " << argv[0] << " [options]\n";
                std::cout << "Options:\n";
                std::cout << "  --config <file>  Configuration file path\n";
                std::cout << "  --host <host>    Local host address (default: 127.0.0.1)\n";
                std::cout << "  --port <port>    Local port number (default: 8080)\n";
                std::cout << "  --daemon         Run in daemon mode\n";
                std::cout << "  --help           Show this help\n";
                return 0;
            }
        }
        
        // Create local endpoint
        core::endpoint local_endpoint(host, static_cast<std::uint16_t>(port));

        // Create IO context
        boost::asio::io_context io_context;

        // Create chat application
        modern_chat_app app(local_endpoint, io_context);

        // Initialize application (async)
        auto init_future = boost::asio::co_spawn(io_context, 
            [&app, &config_file]() -> core::async_result<bool> {
                co_return co_await app.initialize(config_file);
            },
            boost::asio::use_future
        );
        
        // Start IO context in background thread for initialization
        std::thread init_io_thread([&io_context]() {
            io_context.run();
        });
        
        if (!init_future.get()) {
            std::cerr << "Failed to initialize application" << std::endl;
            io_context.stop();
            if (init_io_thread.joinable()) {
                init_io_thread.join();
            }
            return 1;
        }
        
        // Reset IO context for main operations
        io_context.restart();
        if (init_io_thread.joinable()) {
            init_io_thread.join();
        }
        
        // Start application (async)
        auto start_future = boost::asio::co_spawn(io_context,
            [&app]() -> core::async_result<bool> {
                co_return co_await app.start();
            },
            boost::asio::use_future
        );
        
        // Start IO context again for startup
        std::thread start_io_thread([&io_context]() {
            io_context.run();
        });
        
        if (!start_future.get()) {
            std::cerr << "Failed to start application" << std::endl;
            io_context.stop();
            if (start_io_thread.joinable()) {
                start_io_thread.join();
            }
            return 1;
        }
        
        // Reset IO context for main operations
        io_context.restart();
        if (start_io_thread.joinable()) {
            start_io_thread.join();
        }

        // Run IO context in background thread
        std::thread io_thread([&io_context]() {
            io_context.run();
        });

        // Wait for startup
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        if (daemon_mode) {
            std::cout << "Running in daemon mode. Press Ctrl+C to stop.\n";
            
            // Setup signal handling
            boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
            signals.async_wait([&io_context](const boost::system::error_code&, int) {
                std::cout << "[INFO] Received signal, stopping..." << std::endl;
                io_context.stop();
            });

            // Wait for termination
            if (io_thread.joinable()) {
                io_thread.join();
            }
        } else {
            // Run interactive CLI
            app.run_cli();

            // Stop application gracefully
            std::cout << "[INFO] Stopping application..." << std::endl;
            
            // Give some time for cleanup
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

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