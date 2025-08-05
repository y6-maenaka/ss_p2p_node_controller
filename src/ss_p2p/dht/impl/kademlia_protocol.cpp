#include "../../../../include/ss_p2p/dht/i_routing.hpp"
#include "../../../../include/ss_p2p/dht/i_storage.hpp"
#include "../../../../include/ss_p2p/dht/node_id.hpp"
#include "../../../../include/ss_p2p/dht/kademlia_config.hpp"
#include "../../../../include/ss_p2p/core/result.hpp"
#include "../../../../include/ss_p2p/network/i_transport.hpp"

#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <random>
#include <chrono>
#include <boost/asio.hpp>
#include <json.hpp>

namespace ss::dht::impl {

/**
 * @brief Kademlia RPC message types
 */
enum class message_type : std::uint8_t {
    ping = 1,
    pong = 2,
    find_node = 3,
    find_node_response = 4,
    find_value = 5,
    find_value_response = 6,
    store = 7,
    store_response = 8
};

/**
 * @brief Base class for Kademlia RPC messages
 */
struct kademlia_message {
    /// Message type identifier
    message_type type;
    /// Unique message identifier for request/response matching
    ss::core::message_id msg_id;
    /// Sender's node identifier
    node_id sender_id;
    /// Target node identifier (for routing)
    node_id target_id;
    /// Message timestamp
    std::chrono::steady_clock::time_point timestamp;

    /**
     * @brief Constructor
     * @param msg_type Message type
     * @param sender Sender's node ID
     * @param target Target node ID (default: zero)
     */
    kademlia_message(message_type msg_type, node_id sender, node_id target = node_id{})
        : type(msg_type)
        , msg_id(ss::core::message_id::generate())
        , sender_id(std::move(sender))
        , target_id(std::move(target))
        , timestamp(std::chrono::steady_clock::now()) {}

    /**
     * @brief Virtual destructor
     */
    virtual ~kademlia_message() = default;

    /**
     * @brief Serialize message to JSON
     * @return JSON representation
     */
    virtual nlohmann::json to_json() const {
        return nlohmann::json{
            {"type", static_cast<std::uint8_t>(type)},
            {"msg_id", msg_id.value()},
            {"sender_id", sender_id.to_hex()},
            {"target_id", target_id.to_hex()}
        };
    }

    /**
     * @brief Deserialize message from JSON
     * @param json JSON data
     */
    virtual void from_json(const nlohmann::json& json) {
        type = static_cast<message_type>(json["type"].get<std::uint8_t>());
        msg_id = ss::core::message_id{json["msg_id"].get<std::uint64_t>()};
        sender_id = node_id::from_hex(json["sender_id"].get<std::string>());
        target_id = node_id::from_hex(json["target_id"].get<std::string>());
        timestamp = std::chrono::steady_clock::now();
    }
};

/**
 * @brief PING message for liveness checking
 */
struct ping_message : public kademlia_message {
    ping_message(node_id sender) : kademlia_message(message_type::ping, std::move(sender)) {}
};

/**
 * @brief PONG response to PING
 */
struct pong_message : public kademlia_message {
    pong_message(node_id sender, const ping_message& ping)
        : kademlia_message(message_type::pong, std::move(sender)) {
        msg_id = ping.msg_id; // Use same ID for response matching
    }
};

/**
 * @brief FIND_NODE message for iterative node lookup
 */
struct find_node_message : public kademlia_message {
    /// Target node ID to find
    node_id lookup_target;

    find_node_message(node_id sender, node_id target)
        : kademlia_message(message_type::find_node, std::move(sender))
        , lookup_target(std::move(target)) {}

    nlohmann::json to_json() const override {
        auto json = kademlia_message::to_json();
        json["lookup_target"] = lookup_target.to_hex();
        return json;
    }

    void from_json(const nlohmann::json& json) override {
        kademlia_message::from_json(json);
        lookup_target = node_id::from_hex(json["lookup_target"].get<std::string>());
    }
};

/**
 * @brief FIND_NODE response with closest known nodes
 */
struct find_node_response : public kademlia_message {
    /// List of closest known nodes
    std::vector<peer_info> nodes;

    find_node_response(node_id sender, const find_node_message& request)
        : kademlia_message(message_type::find_node_response, std::move(sender)) {
        msg_id = request.msg_id;
    }

    nlohmann::json to_json() const override {
        auto json = kademlia_message::to_json();
        nlohmann::json node_array = nlohmann::json::array();
        
        for (const auto& node : nodes) {
            node_array.push_back({
                {"id", node.id.to_hex()},
                {"endpoint", node.endpoint.to_string()},
                {"last_seen", std::chrono::duration_cast<std::chrono::seconds>(
                    node.last_seen.time_since_epoch()).count()}
            });
        }
        
        json["nodes"] = node_array;
        return json;
    }

    void from_json(const nlohmann::json& json) override {
        kademlia_message::from_json(json);
        nodes.clear();
        
        for (const auto& node_json : json["nodes"]) {
            auto node_id = node_id::from_hex(node_json["id"].get<std::string>());
            auto endpoint_str = node_json["endpoint"].get<std::string>();
            
            // Parse endpoint string (format: "ip:port")
            auto colon_pos = endpoint_str.find(':');
            if (colon_pos != std::string::npos) {
                auto ip = endpoint_str.substr(0, colon_pos);
                auto port = static_cast<std::uint16_t>(
                    std::stoul(endpoint_str.substr(colon_pos + 1)));
                
                peer_info peer(node_id, ss::core::endpoint{ip, port});
                
                // Restore last_seen timestamp if available
                if (node_json.contains("last_seen")) {
                    auto timestamp = std::chrono::seconds{node_json["last_seen"].get<std::int64_t>()};
                    peer.last_seen = std::chrono::steady_clock::time_point{timestamp};
                }
                
                nodes.push_back(std::move(peer));
            }
        }
    }
};

/**
 * @brief FIND_VALUE message for value lookup
 */
struct find_value_message : public kademlia_message {
    /// Key to find
    dht_key key;

    find_value_message(node_id sender, dht_key lookup_key)
        : kademlia_message(message_type::find_value, std::move(sender))
        , key(std::move(lookup_key)) {}

    nlohmann::json to_json() const override {
        auto json = kademlia_message::to_json();
        json["key"] = key.to_hex();
        return json;
    }

    void from_json(const nlohmann::json& json) override {
        kademlia_message::from_json(json);
        key = dht_key::from_hex(json["key"].get<std::string>());
    }
};

/**
 * @brief FIND_VALUE response with value or closest nodes
 */
struct find_value_response : public kademlia_message {
    /// Found value (if any)
    std::optional<dht_value> value;
    /// Closest nodes if value not found
    std::vector<peer_info> nodes;

    find_value_response(node_id sender, const find_value_message& request)
        : kademlia_message(message_type::find_value_response, std::move(sender)) {
        msg_id = request.msg_id;
    }

    nlohmann::json to_json() const override {
        auto json = kademlia_message::to_json();
        
        if (value) {
            json["has_value"] = true;
            json["value"] = {
                {"data", nlohmann::json::binary_t(value->data)},
                {"metadata", value->metadata},
                {"ttl", value->ttl.count()},
                {"publisher", value->publisher.to_hex()},
                {"sequence", value->sequence_number}
            };
        } else {
            json["has_value"] = false;
            nlohmann::json node_array = nlohmann::json::array();
            
            for (const auto& node : nodes) {
                node_array.push_back({
                    {"id", node.id.to_hex()},
                    {"endpoint", node.endpoint.to_string()}
                });
            }
            
            json["nodes"] = node_array;
        }
        
        return json;
    }

    void from_json(const nlohmann::json& json) override {
        kademlia_message::from_json(json);
        
        if (json["has_value"].get<bool>()) {
            auto value_json = json["value"];
            auto data = value_json["data"].get<nlohmann::json::binary_t>();
            auto ttl = std::chrono::seconds{value_json["ttl"].get<std::int64_t>()};
            auto publisher = node_id::from_hex(value_json["publisher"].get<std::string>());
            
            dht_value val{std::vector<std::uint8_t>(data.begin(), data.end()), ttl, publisher};
            val.metadata = value_json["metadata"].get<std::string>();
            val.sequence_number = value_json["sequence"].get<std::uint64_t>();
            
            value = std::move(val);
        } else {
            // Parse nodes (similar to find_node_response)
            nodes.clear();
            for (const auto& node_json : json["nodes"]) {
                auto node_id = node_id::from_hex(node_json["id"].get<std::string>());
                auto endpoint_str = node_json["endpoint"].get<std::string>();
                
                auto colon_pos = endpoint_str.find(':');
                if (colon_pos != std::string::npos) {
                    auto ip = endpoint_str.substr(0, colon_pos);
                    auto port = static_cast<std::uint16_t>(
                        std::stoul(endpoint_str.substr(colon_pos + 1)));
                    
                    nodes.emplace_back(node_id, ss::core::endpoint{ip, port});
                }
            }
        }
    }
};

/**
 * @brief STORE message for storing key-value pairs
 */
struct store_message : public kademlia_message {
    /// Key to store
    dht_key key;
    /// Value to store
    dht_value value;

    store_message(node_id sender, dht_key store_key, dht_value store_value)
        : kademlia_message(message_type::store, std::move(sender))
        , key(std::move(store_key))
        , value(std::move(store_value)) {}

    nlohmann::json to_json() const override {
        auto json = kademlia_message::to_json();
        json["key"] = key.to_hex();
        json["value"] = {
            {"data", nlohmann::json::binary_t(value.data)},
            {"metadata", value.metadata},
            {"ttl", value.ttl.count()},
            {"publisher", value.publisher.to_hex()},
            {"sequence", value.sequence_number}
        };
        return json;
    }

    void from_json(const nlohmann::json& json) override {
        kademlia_message::from_json(json);
        key = dht_key::from_hex(json["key"].get<std::string>());
        
        auto value_json = json["value"];
        auto data = value_json["data"].get<nlohmann::json::binary_t>();
        auto ttl = std::chrono::seconds{value_json["ttl"].get<std::int64_t>()};
        auto publisher = node_id::from_hex(value_json["publisher"].get<std::string>());
        
        value = dht_value{std::vector<std::uint8_t>(data.begin(), data.end()), ttl, publisher};
        value.metadata = value_json["metadata"].get<std::string>();
        value.sequence_number = value_json["sequence"].get<std::uint64_t>();
    }
};

/**
 * @brief STORE response confirming storage
 */
struct store_response : public kademlia_message {
    /// Success status
    bool success;
    /// Error message if unsuccessful
    std::string error_message;

    store_response(node_id sender, const store_message& request, bool succeeded)
        : kademlia_message(message_type::store_response, std::move(sender))
        , success(succeeded) {
        msg_id = request.msg_id;
    }

    nlohmann::json to_json() const override {
        auto json = kademlia_message::to_json();
        json["success"] = success;
        json["error"] = error_message;
        return json;
    }

    void from_json(const nlohmann::json& json) override {
        kademlia_message::from_json(json);
        success = json["success"].get<bool>();
        error_message = json["error"].get<std::string>();
    }
};

/**
 * @brief Pending RPC request tracking
 */
struct pending_request {
    /// Request message ID
    ss::core::message_id msg_id;
    /// Target endpoint
    ss::core::endpoint target;
    /// Request timestamp
    std::chrono::steady_clock::time_point sent_at;
    /// Timeout duration
    std::chrono::milliseconds timeout;
    /// Response callback
    std::function<void(std::unique_ptr<kademlia_message>)> callback;
    /// Timeout timer
    std::unique_ptr<boost::asio::steady_timer> timer;

    pending_request(ss::core::message_id id, 
                   ss::core::endpoint ep,
                   std::chrono::milliseconds timeout_ms,
                   std::function<void(std::unique_ptr<kademlia_message>)> cb,
                   boost::asio::io_context& io_context)
        : msg_id(id)
        , target(std::move(ep))
        , sent_at(std::chrono::steady_clock::now())
        , timeout(timeout_ms)
        , callback(std::move(cb))
        , timer(std::make_unique<boost::asio::steady_timer>(io_context)) {}
};

/**
 * @brief Kademlia protocol implementation
 * 
 * Handles RPC communication, message serialization/deserialization,
 * and implements the core Kademlia protocol operations.
 */
class kademlia_protocol {
private:
    /// Local node identifier
    node_id local_node_id_;
    /// Configuration parameters
    kademlia_config config_;
    /// Transport layer for network communication
    std::shared_ptr<ss::network::i_transport> transport_;
    /// Routing table for peer management
    std::shared_ptr<i_routing> routing_table_;
    /// Storage layer for key-value pairs
    std::shared_ptr<i_storage> storage_;
    /// IO context for async operations
    boost::asio::io_context& io_context_;
    /// Pending RPC requests
    std::unordered_map<ss::core::message_id, std::unique_ptr<pending_request>> pending_requests_;
    /// Mutex for pending requests
    std::mutex pending_requests_mutex_;
    /// Protocol running state
    std::atomic<bool> running_{false};

public:
    /**
     * @brief Constructor
     * @param local_id Local node identifier
     * @param config Configuration parameters
     * @param transport Transport layer
     * @param routing Routing table
     * @param storage Storage layer
     * @param io_context IO context
     */
    kademlia_protocol(node_id local_id,
                     kademlia_config config,
                     std::shared_ptr<ss::network::i_transport> transport,
                     std::shared_ptr<i_routing> routing,
                     std::shared_ptr<i_storage> storage,
                     boost::asio::io_context& io_context)
        : local_node_id_(std::move(local_id))
        , config_(std::move(config))
        , transport_(std::move(transport))
        , routing_table_(std::move(routing))
        , storage_(std::move(storage))
        , io_context_(io_context) {}

    /**
     * @brief Start the protocol
     */
    boost::asio::awaitable<ss::core::result<void, std::string>> start() {
        if (running_.exchange(true)) {
            co_return ss::core::result<void, std::string>::ok(); // Already running
        }

        // Set up message handler
        transport_->set_message_handler(
            [this](ss::network::incoming_message msg) -> ss::core::async_void {
                co_await handle_incoming_message(std::move(msg));
            });

        co_return ss::core::result<void, std::string>::ok();
    }

    /**
     * @brief Stop the protocol
     */
    boost::asio::awaitable<void> stop() {
        if (!running_.exchange(false)) {
            co_return; // Already stopped
        }

        transport_->clear_message_handler();

        // Cancel all pending requests
        std::lock_guard lock(pending_requests_mutex_);
        for (auto& [id, request] : pending_requests_) {
            if (request->timer) {
                request->timer->cancel();
            }
            if (request->callback) {
                request->callback(nullptr); // Signal timeout/cancellation
            }
        }
        pending_requests_.clear();

        co_return;
    }

    /**
     * @brief Send PING to check node liveness
     * @param target Target endpoint
     * @return Awaitable result with success/failure
     */
    boost::asio::awaitable<ss::core::result<bool, std::string>> ping(const ss::core::endpoint& target) {
        auto ping_msg = std::make_unique<ping_message>(local_node_id_);
        [[maybe_unused]] auto msg_id = ping_msg->msg_id;
        
        auto result = co_await send_request(
            std::move(ping_msg), target, config_.rpc_timeout);
        
        if (!result) {
            co_return ss::core::result<bool, std::string>::err(result.error());
        }

        auto response = std::move(result).value();
        if (!response || response->type != message_type::pong) {
            co_return ss::core::result<bool, std::string>::err("Invalid response");
        }

        co_return ss::core::result<bool, std::string>::ok(true);
    }

    /**
     * @brief Send FIND_NODE request
     * @param target Target endpoint
     * @param lookup_target Node ID to find
     * @return Awaitable result with closest nodes
     */
    boost::asio::awaitable<ss::core::result<std::vector<peer_info>, std::string>>
    find_node(const ss::core::endpoint& target, const node_id& lookup_target) {
        
        auto find_msg = std::make_unique<find_node_message>(local_node_id_, lookup_target);
        
        auto result = co_await send_request(
            std::move(find_msg), target, config_.rpc_timeout);
        
        if (!result) {
            co_return ss::core::result<std::vector<peer_info>, std::string>::err(result.error());
        }

        auto response = std::move(result).value();
        if (!response || response->type != message_type::find_node_response) {
            co_return ss::core::result<std::vector<peer_info>, std::string>::err("Invalid response");
        }

        auto* find_response = static_cast<find_node_response*>(response.get());
        co_return ss::core::result<std::vector<peer_info>, std::string>::ok(
            std::move(find_response->nodes));
    }

    /**
     * @brief Send FIND_VALUE request
     * @param target Target endpoint
     * @param key Key to find
     * @return Awaitable result with value or closest nodes
     */
    boost::asio::awaitable<ss::core::result<find_value_response, std::string>>
    find_value(const ss::core::endpoint& target, const dht_key& key) {
        
        auto find_msg = std::make_unique<find_value_message>(local_node_id_, key);
        
        auto result = co_await send_request(
            std::move(find_msg), target, config_.rpc_timeout);
        
        if (!result) {
            co_return ss::core::result<find_value_response, std::string>::err(result.error());
        }

        auto response = std::move(result).value();
        if (!response || response->type != message_type::find_value_response) {
            co_return ss::core::result<find_value_response, std::string>::err("Invalid response");
        }

        auto* value_response = static_cast<find_value_response*>(response.get());
        find_value_response result_response(local_node_id_, find_value_message{local_node_id_, key});
        result_response.value = std::move(value_response->value);
        result_response.nodes = std::move(value_response->nodes);
        
        co_return ss::core::result<find_value_response, std::string>::ok(std::move(result_response));
    }

    /**
     * @brief Send STORE request
     * @param target Target endpoint
     * @param key Key to store
     * @param value Value to store
     * @return Awaitable result with success/failure
     */
    boost::asio::awaitable<ss::core::result<bool, std::string>>
    store(const ss::core::endpoint& target, const dht_key& key, const dht_value& value) {
        
        auto store_msg = std::make_unique<store_message>(local_node_id_, key, value);
        
        auto result = co_await send_request(
            std::move(store_msg), target, config_.rpc_timeout);
        
        if (!result) {
            co_return ss::core::result<bool, std::string>::err(result.error());
        }

        auto response = std::move(result).value();
        if (!response || response->type != message_type::store_response) {
            co_return ss::core::result<bool, std::string>::err("Invalid response");
        }

        auto* store_resp = static_cast<store_response*>(response.get());
        if (!store_resp->success) {
            co_return ss::core::result<bool, std::string>::err(store_resp->error_message);
        }

        co_return ss::core::result<bool, std::string>::ok(true);
    }

private:
    /**
     * @brief Handle incoming message
     * @param msg Incoming message
     */
    boost::asio::awaitable<void> handle_incoming_message(ss::network::incoming_message msg) {
        try {
            // Parse JSON message
            auto json_data = nlohmann::json::parse(msg.data.begin(), msg.data.end());
            auto msg_type = static_cast<message_type>(json_data["type"].get<std::uint8_t>());
            
            // Create appropriate message object
            std::unique_ptr<kademlia_message> kademlia_msg;
            
            switch (msg_type) {
                case message_type::ping:
                    kademlia_msg = std::make_unique<ping_message>(local_node_id_);
                    break;
                case message_type::find_node:
                    kademlia_msg = std::make_unique<find_node_message>(local_node_id_, node_id{});
                    break;
                case message_type::find_value:
                    kademlia_msg = std::make_unique<find_value_message>(local_node_id_, dht_key{});
                    break;
                case message_type::store:
                    kademlia_msg = std::make_unique<store_message>(
                        local_node_id_, dht_key{}, dht_value{{}, std::chrono::seconds{0}, node_id{}});
                    break;
                default:
                    // Handle responses
                    co_await handle_response(json_data, msg.sender);
                    co_return;
            }
            
            if (kademlia_msg) {
                kademlia_msg->from_json(json_data);
                co_await handle_request(std::move(kademlia_msg), msg.sender);
            }
            
        } catch (const std::exception&) {
            // Invalid message format - ignore
        }
    }

    /**
     * @brief Handle incoming request
     * @param request Request message
     * @param sender Sender endpoint
     */
    boost::asio::awaitable<void> handle_request(std::unique_ptr<kademlia_message> request, 
                                               const ss::core::endpoint& sender) {
        
        // Update routing table with sender information
        peer_info sender_peer(request->sender_id, sender);
        co_await routing_table_->add_peer(std::move(sender_peer));
        
        std::unique_ptr<kademlia_message> response;
        
        switch (request->type) {
            case message_type::ping:
                response = std::make_unique<pong_message>(
                    local_node_id_, *static_cast<ping_message*>(request.get()));
                break;
                
            case message_type::find_node: {
                auto* find_req = static_cast<find_node_message*>(request.get());
                auto find_resp = std::make_unique<find_node_response>(local_node_id_, *find_req);
                
                auto closest_result = co_await routing_table_->find_closest_peers(
                    find_req->lookup_target, config_.k_bucket_size);
                    
                if (closest_result) {
                    find_resp->nodes = closest_result.value();
                }
                
                response = std::move(find_resp);
                break;
            }
            
            case message_type::find_value: {
                auto* find_req = static_cast<find_value_message*>(request.get());
                auto find_resp = std::make_unique<find_value_response>(local_node_id_, *find_req);
                
                // Try to find value in local storage
                auto value_result = co_await storage_->retrieve(find_req->key);
                
                if (value_result) {
                    find_resp->value = value_result.value();
                } else {
                    // Return closest nodes
                    auto closest_result = co_await routing_table_->find_closest_peers(
                        find_req->key, config_.k_bucket_size);
                        
                    if (closest_result) {
                        find_resp->nodes = closest_result.value();
                    }
                }
                
                response = std::move(find_resp);
                break;
            }
            
            case message_type::store: {
                auto* store_req = static_cast<store_message*>(request.get());
                auto store_resp = std::make_unique<store_response>(
                    local_node_id_, *store_req, true);
                
                // Store value in local storage
                auto store_result = co_await storage_->store(store_req->key, store_req->value);
                
                if (!store_result) {
                    store_resp->success = false;
                    store_resp->error_message = store_result.error();
                }
                
                response = std::move(store_resp);
                break;
            }
            
            default:
                co_return; // Unknown request type
        }
        
        if (response) {
            co_await send_response(std::move(response), sender);
        }
    }

    /**
     * @brief Handle incoming response
     * @param json_data Response JSON data
     * @param sender Sender endpoint
     */
    boost::asio::awaitable<void> handle_response(const nlohmann::json& json_data,
                                                const ss::core::endpoint& sender) {
        
        auto msg_id = ss::core::message_id{json_data["msg_id"].get<std::uint64_t>()};
        
        std::unique_ptr<pending_request> request;
        {
            std::lock_guard lock(pending_requests_mutex_);
            auto it = pending_requests_.find(msg_id);
            if (it != pending_requests_.end()) {
                request = std::move(it->second);
                pending_requests_.erase(it);
            }
        }
        
        if (!request) {
            co_return; // No matching request found
        }
        
        // Cancel timeout timer
        if (request->timer) {
            request->timer->cancel();
        }
        
        // Create response message
        std::unique_ptr<kademlia_message> response;
        auto response_type = static_cast<message_type>(json_data["type"].get<std::uint8_t>());
        
        switch (response_type) {
            case message_type::pong:
                response = std::make_unique<pong_message>(local_node_id_, 
                    ping_message{local_node_id_});
                break;
            case message_type::find_node_response:
                response = std::make_unique<find_node_response>(local_node_id_,
                    find_node_message{local_node_id_, node_id{}});
                break;
            case message_type::find_value_response:
                response = std::make_unique<find_value_response>(local_node_id_,
                    find_value_message{local_node_id_, dht_key{}});
                break;
            case message_type::store_response:
                response = std::make_unique<store_response>(local_node_id_,
                    store_message{local_node_id_, dht_key{}, 
                        dht_value{{}, std::chrono::seconds{0}, node_id{}}}, true);
                break;
            default:
                co_return;
        }
        
        if (response) {
            response->from_json(json_data);
            
            // Call the response callback
            if (request->callback) {
                request->callback(std::move(response));
            }
        }
    }

    /**
     * @brief Send RPC request with timeout
     * @param request Request message
     * @param target Target endpoint
     * @param timeout Request timeout
     * @return Awaitable result with response message
     */
    boost::asio::awaitable<ss::core::result<std::unique_ptr<kademlia_message>, std::string>>
    send_request(std::unique_ptr<kademlia_message> request,
                const ss::core::endpoint& target,
                std::chrono::milliseconds timeout) {
        
        auto msg_id = request->msg_id;
        
        // Serialize request to JSON
        auto json_data = request->to_json();
        auto json_str = json_data.dump();
        std::vector<std::uint8_t> data(json_str.begin(), json_str.end());
        
        // Create pending request
        auto pending = std::make_unique<pending_request>(
            msg_id, target, timeout,
            [](std::unique_ptr<kademlia_message>) {}, // Placeholder callback
            io_context_);
        
        // Set up response handling
        std::unique_ptr<kademlia_message> response_msg;
        bool response_received = false;
        
        pending->callback = [&response_msg, &response_received]
            (std::unique_ptr<kademlia_message> response) {
            response_msg = std::move(response);
            response_received = true;
        };
        
        // Set up timeout timer
        pending->timer->expires_after(timeout);
        pending->timer->async_wait([&response_received](boost::system::error_code ec) {
            if (!ec) {
                response_received = true; // Timeout occurred
            }
        });
        
        // Store pending request
        {
            std::lock_guard lock(pending_requests_mutex_);
            pending_requests_[msg_id] = std::move(pending);
        }
        
        // Send request
        ss::network::outgoing_message outgoing(std::move(data), target);
        auto send_result = co_await transport_->send(std::move(outgoing));
        
        if (!send_result) {
            // Remove pending request on send failure
            std::lock_guard lock(pending_requests_mutex_);
            pending_requests_.erase(msg_id);
            co_return ss::core::result<std::unique_ptr<kademlia_message>, std::string>::err(
                "Send failed");
        }
        
        // Wait for response or timeout
        while (!response_received) {
            co_await boost::asio::steady_timer(io_context_, std::chrono::milliseconds{10}).async_wait(
                boost::asio::use_awaitable);
        }
        
        // Clean up pending request
        {
            std::lock_guard lock(pending_requests_mutex_);
            pending_requests_.erase(msg_id);
        }
        
        if (!response_msg) {
            co_return ss::core::result<std::unique_ptr<kademlia_message>, std::string>::err(
                "Request timeout");
        }
        
        co_return ss::core::result<std::unique_ptr<kademlia_message>, std::string>::ok(
            std::move(response_msg));
    }

    /**
     * @brief Send response message
     * @param response Response message
     * @param target Target endpoint
     */
    boost::asio::awaitable<void> send_response(std::unique_ptr<kademlia_message> response,
                                              const ss::core::endpoint& target) {
        
        auto json_data = response->to_json();
        auto json_str = json_data.dump();
        std::vector<std::uint8_t> data(json_str.begin(), json_str.end());
        
        ss::network::outgoing_message outgoing(std::move(data), target);
        co_await transport_->send(std::move(outgoing));
    }
};

} // namespace ss::dht::impl