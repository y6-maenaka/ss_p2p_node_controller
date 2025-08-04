# SS P2P Node Controller - API Reference

## Overview

The SS P2P Node Controller provides a comprehensive, modern C++20 API for building distributed peer-to-peer applications. The library is designed with clean architecture principles, featuring modular components that can be used independently or together to create robust P2P networks.

## Table of Contents

1. [Core API](#core-api)
2. [Network API](#network-api)
3. [DHT API](#dht-api)
4. [ICE API](#ice-api)
5. [Security API](#security-api)
6. [Application API](#application-api)
7. [Integration Guide](#integration-guide)
8. [Code Examples](#code-examples)

---

## Core API

The Core API provides fundamental types, interfaces, and utilities used throughout the library.

### Types and Concepts

#### Core Types

```cpp
namespace ss::core {
    // 160-bit node identifier
    class node_id {
    public:
        static constexpr size_t SIZE = 20;
        using storage_type = std::array<std::uint8_t, SIZE>;
        
        constexpr node_id() noexcept;
        explicit constexpr node_id(const storage_type& data) noexcept;
        
        // XOR distance calculation for DHT
        constexpr node_id distance(const node_id& other) const noexcept;
        
        // String representation
        std::string to_hex() const;
        static node_id from_hex(const std::string& hex_str);
        static node_id random();
        
        // Comparison operators
        constexpr bool operator==(const node_id& other) const noexcept;
        constexpr bool operator<(const node_id& other) const noexcept;
    };
    
    // Network endpoint with strong typing
    class endpoint {
    public:
        using native_type = boost::asio::ip::udp::endpoint;
        
        endpoint(const std::string& addr, std::uint16_t port);
        explicit endpoint(const native_type& ep) noexcept;
        
        std::string address() const;
        std::uint16_t port() const noexcept;
        bool is_valid() const noexcept;
        std::string to_string() const;
        
        const native_type& native() const noexcept;
    };
    
    // Message identifier for request/response tracking
    class message_id {
    public:
        using value_type = std::uint64_t;
        
        constexpr message_id() noexcept;
        explicit constexpr message_id(value_type value) noexcept;
        
        constexpr value_type value() const noexcept;
        static message_id generate();
    };
    
    // Async operation types
    template<typename T>
    using async_result = boost::asio::awaitable<T>;
    using async_void = boost::asio::awaitable<void>;
}
```

#### Result Monad

```cpp
namespace ss::core {
    // Rust-like Result type for error handling
    template<typename T, typename E>
    class result {
    public:
        using value_type = T;
        using error_type = E;
        
        // Construction
        template<typename U> constexpr result(U&& value);
        template<typename F> constexpr result(F&& error);
        
        // Status checking
        constexpr bool is_ok() const noexcept;
        constexpr bool is_err() const noexcept;
        constexpr explicit operator bool() const noexcept;
        
        // Value access
        constexpr T& value() &;
        constexpr const T& value() const&;
        constexpr T&& value() &&;
        
        constexpr E& error() &;
        constexpr const E& error() const&;
        constexpr E&& error() &&;
        
        // Monadic operations
        template<typename F> auto map(F&& func) const& -> result<...>;
        template<typename F> auto and_then(F&& func) const& -> result<...>;
        template<typename F> auto or_else(F&& func) const& -> result<...>;
        
        // Factory methods
        template<typename... Args>
        static constexpr result ok(Args&&... args);
        template<typename... Args>
        static constexpr result err(Args&&... args);
    };
}
```

#### Core Interfaces

```cpp
namespace ss::core {
    // Lifecycle management interface
    class i_lifecycle {
    public:
        virtual ~i_lifecycle() = default;
        virtual async_void start() = 0;
        virtual async_void stop() = 0;
        virtual bool is_running() const noexcept = 0;
    };
    
    // Component interface with health and monitoring
    class i_component : public i_lifecycle {
    public:
        virtual ~i_component() = default;
        virtual std::string name() const noexcept = 0;
        virtual std::string version() const noexcept;
        virtual bool is_healthy() const noexcept;
        virtual std::string status() const;
        
        virtual async_void initialize();
        virtual async_void cleanup();
    };
    
    // Configurable objects
    template<typename ConfigType>
    class i_configurable {
    public:
        virtual ~i_configurable() = default;
        virtual result<void, std::string> configure(const ConfigType& config) = 0;
        virtual ConfigType get_config() const = 0;
        virtual result<void, std::string> validate_config(const ConfigType& config) const = 0;
    };
}
```

#### C++20 Concepts

```cpp
namespace ss::core {
    // Serialization concept
    template<typename T>
    concept Serializable = requires(const T& t, const std::vector<std::uint8_t>& data) {
        { t.serialize() } -> std::convertible_to<std::vector<std::uint8_t>>;
        { T::deserialize(data) } -> std::same_as<T>;
    };
    
    // Network endpoint concept
    template<typename T>
    concept NetworkEndpoint = requires(const T& ep) {
        { ep.address() } -> std::convertible_to<std::string>;
        { ep.port() } -> std::convertible_to<std::uint16_t>;
        { ep.to_string() } -> std::convertible_to<std::string>;
        { ep.is_valid() } -> std::convertible_to<bool>;
    } && Hashable<T> && Comparable<T>;
    
    // Component concept
    template<typename T>
    concept Component = LifecycleManaged<T> && requires(const T& comp) {
        { comp.name() } -> std::convertible_to<std::string>;
    } && std::move_constructible<T>;
}
```

### Usage Examples

```cpp
// Using Result monad for error handling
auto load_config(const std::string& path) -> result<Config, std::string> {
    return read_file(path)
        .and_then([](const std::string& content) {
            return parse_json(content);
        })
        .map([](const nlohmann::json& json) {
            return Config::from_json(json);
        });
}

// Async component lifecycle
auto start_component(i_component& comp) -> async_void {
    co_await comp.initialize();
    co_await comp.start();
    
    if (!comp.is_healthy()) {
        throw std::runtime_error("Component failed health check");
    }
}

// Working with node IDs
auto generate_network_id() {
    auto id = node_id::random();
    auto hex = id.to_hex();
    
    // Calculate distance for DHT operations
    auto target = node_id::from_hex("abcd1234...");
    auto distance = id.distance(target);
    
    return std::make_pair(id, distance);
}
```

---

## Network API

The Network API provides abstraction over different transport protocols and message handling.

### Transport Interface

```cpp
namespace ss::network {
    // Transport error codes
    enum class transport_error {
        none = 0,
        network_failure,
        connection_refused,
        timeout,
        host_unreachable,
        message_too_large,
        not_connected,
        // ... more error codes
    };
    
    // Transport statistics
    struct transport_stats {
        std::uint64_t bytes_sent = 0;
        std::uint64_t bytes_received = 0;
        std::uint64_t messages_sent = 0;
        std::uint64_t messages_received = 0;
        std::uint64_t send_errors = 0;
        std::uint64_t receive_errors = 0;
        std::uint64_t avg_send_latency_us = 0;
        std::uint64_t avg_receive_latency_us = 0;
    };
    
    // Message structures
    struct incoming_message {
        std::vector<std::uint8_t> data;
        ss::core::endpoint sender;
        std::chrono::steady_clock::time_point received_at;
        std::string metadata;
    };
    
    struct outgoing_message {
        std::vector<std::uint8_t> data;
        ss::core::endpoint target;
        std::uint32_t priority = 0;
        std::optional<std::chrono::milliseconds> timeout;
        std::string metadata;
    };
    
    // Message handler types
    using message_handler = std::function<ss::core::async_void(incoming_message)>;
    using send_completion_handler = std::function<void(ss::core::result<void, transport_error>)>;
}
```

#### Transport Interface

```cpp
namespace ss::network {
    class i_transport : public ss::core::i_component {
    public:
        virtual ~i_transport() = default;
        
        // Connection management
        virtual ss::core::async_result<ss::core::result<void, transport_error>>
        bind(const ss::core::endpoint& local_endpoint) = 0;
        
        virtual ss::core::async_result<ss::core::result<ss::core::endpoint, transport_error>>
        connect(const ss::core::endpoint& remote_endpoint) = 0;
        
        // Message operations
        virtual ss::core::async_result<ss::core::result<void, transport_error>>
        send(outgoing_message message) = 0;
        
        virtual void send_async(outgoing_message message, send_completion_handler handler) = 0;
        
        virtual ss::core::async_result<ss::core::result<incoming_message, transport_error>>
        receive(std::optional<std::chrono::milliseconds> timeout = std::nullopt) = 0;
        
        // Message handling
        virtual void set_message_handler(message_handler handler) = 0;
        virtual void clear_message_handler() = 0;
        
        // Status and configuration
        virtual ss::core::endpoint local_endpoint() const noexcept = 0;
        virtual ss::core::endpoint remote_endpoint() const noexcept = 0;
        virtual bool is_connection_based() const noexcept = 0;
        virtual bool is_connected() const noexcept = 0;
        
        virtual transport_stats get_stats() const noexcept = 0;
        virtual void reset_stats() noexcept = 0;
        
        // Flow control
        virtual bool can_send() const noexcept = 0;
        virtual std::uint32_t send_queue_size() const noexcept = 0;
        virtual std::uint32_t receive_queue_size() const noexcept = 0;
        
        // Lifecycle
        virtual ss::core::async_void disconnect() = 0;
        virtual ss::core::async_void shutdown(std::chrono::milliseconds timeout = std::chrono::milliseconds{5000}) = 0;
    };
    
    using transport_ptr = std::shared_ptr<i_transport>;
}
```

### Protocol Interface

```cpp
namespace ss::network {
    // Protocol configuration
    struct protocol_config {
        protocol_version version{1, 0, 0};
        bool enable_compression = false;
        std::uint8_t compression_level = 6;
        bool enable_encryption = false;
        bool enable_authentication = false;
        std::uint32_t max_message_size = 1024 * 1024;  // 1MB
        bool enable_fragmentation = true;
        std::uint32_t fragment_size = 65536;  // 64KB
    };
    
    // Message metadata
    struct message_metadata {
        std::uint32_t type_id = 0;
        std::uint64_t sequence_number = 0;
        std::chrono::steady_clock::time_point created_at;
        std::uint32_t flags = 0;
        std::uint32_t checksum = 0;
        std::unordered_map<std::string, std::string> attributes;
    };
    
    class i_protocol : public ss::core::i_component {
    public:
        virtual ~i_protocol() = default;
        
        // Message encoding/decoding
        virtual ss::core::result<encoded_message, protocol_error> 
        encode(const std::vector<std::uint8_t>& payload, 
               std::uint32_t type_id,
               std::optional<message_metadata> metadata = std::nullopt) = 0;
        
        virtual ss::core::result<decoded_message, protocol_error>
        decode(const std::vector<std::uint8_t>& data) = 0;
        
        // Async variants
        virtual ss::core::async_result<ss::core::result<encoded_message, protocol_error>>
        encode_async(const std::vector<std::uint8_t>& payload,
                     std::uint32_t type_id,
                     std::optional<message_metadata> metadata = std::nullopt) = 0;
        
        virtual ss::core::async_result<ss::core::result<decoded_message, protocol_error>>
        decode_async(const std::vector<std::uint8_t>& data) = 0;
        
        // Protocol capabilities
        virtual bool supports_fragmentation() const noexcept = 0;
        virtual bool supports_compression() const noexcept = 0;
        virtual bool supports_encryption() const noexcept = 0;
        virtual bool supports_ordering() const noexcept = 0;
        virtual bool supports_acknowledgments() const noexcept = 0;
        
        // Message validation
        virtual bool validate_format(const std::vector<std::uint8_t>& data) const noexcept = 0;
        
        // Protocol info
        virtual protocol_version get_version() const noexcept = 0;
        virtual bool supports_version(const protocol_version& version) const noexcept = 0;
        virtual std::uint32_t max_message_size() const noexcept = 0;
        virtual std::uint32_t overhead_size() const noexcept = 0;
    };
}
```

### Usage Examples

```cpp
// Setting up a UDP transport
auto setup_transport(boost::asio::io_context& io_context) -> ss::core::async_result<transport_ptr> {
    auto transport = transport_factory::create(transport_factory::transport_type::udp, io_context);
    
    // Configure transport
    transport_config config;
    config.max_concurrent_ops = 1000;
    config.send_buffer_size = 65536;
    config.default_timeout = std::chrono::milliseconds{5000};
    
    auto result = transport->set_config(config);
    if (!result.is_ok()) {
        throw std::runtime_error("Failed to configure transport");
    }
    
    // Bind to local endpoint
    auto bind_result = co_await transport->bind(endpoint{"127.0.0.1", 8080});
    if (!bind_result.is_ok()) {
        throw std::runtime_error("Failed to bind transport");
    }
    
    co_return transport;
}

// Sending messages with error handling
auto send_reliable_message(transport_ptr transport, const outgoing_message& msg) -> ss::core::async_result<bool> {
    constexpr int max_retries = 3;
    
    for (int attempt = 0; attempt < max_retries; ++attempt) {
        auto result = co_await transport->send(msg);
        
        if (result.is_ok()) {
            co_return true;
        }
        
        // Handle specific errors
        auto error = result.error();
        if (error == transport_error::message_too_large) {
            // Cannot retry - message is fundamentally too large
            co_return false;
        }
        
        if (error == transport_error::timeout && attempt < max_retries - 1) {
            // Wait before retry
            co_await boost::asio::steady_timer(co_await boost::asio::this_coro::executor)
                .async_wait(std::chrono::milliseconds{100 * (attempt + 1)});
            continue;
        }
    }
    
    co_return false;
}

// Message handling with protocol processing
class message_processor {
private:
    protocol_ptr protocol_;
    
public:
    auto handle_incoming_message(const incoming_message& msg) -> ss::core::async_void {
        // Decode message using protocol
        auto decode_result = co_await protocol_->decode_async(msg.data);
        
        if (!decode_result.is_ok()) {
            // Log protocol error
            return;
        }
        
        auto decoded = decode_result.value();
        
        // Process based on message type
        switch (decoded.metadata.type_id) {
            case 1: // Ping message
                co_await handle_ping(decoded, msg.sender);
                break;
            case 2: // DHT query
                co_await handle_dht_query(decoded, msg.sender);
                break;
            // ... more message types
        }
    }
};
```

---

## DHT API

The DHT API provides distributed hash table functionality based on the Kademlia algorithm.

### Core Structures

```cpp
namespace ss::dht {
    // Peer information in routing table
    struct peer_info {
        ss::core::node_id id;
        ss::core::endpoint endpoint;
        std::chrono::steady_clock::time_point last_seen;
        std::uint32_t failure_count = 0;
        std::chrono::microseconds rtt{0};
        bool ping_pending = false;
        
        bool is_alive(std::chrono::milliseconds max_age) const noexcept;
        void update_liveness(std::chrono::microseconds new_rtt = {}) noexcept;
        void record_failure() noexcept;
    };
    
    // Find operation context
    struct find_context {
        ss::core::node_id target;
        std::uint32_t max_results;
        std::unordered_set<ss::core::node_id> queried_nodes;
        std::vector<peer_info> results;
        std::chrono::steady_clock::time_point start_time;
        std::uint32_t alpha;  // Concurrency parameter
        
        void add_results(const std::vector<peer_info>& peers);
        void mark_queried(const ss::core::node_id& node_id);
        std::chrono::milliseconds elapsed() const noexcept;
    };
    
    // Routing statistics
    struct routing_stats {
        std::uint32_t total_peers = 0;
        std::uint32_t active_buckets = 0;
        std::uint64_t find_operations = 0;
        std::uint64_t successful_finds = 0;
        std::uint64_t avg_find_time_us = 0;
        std::uint64_t peers_added = 0;
        std::uint64_t peers_removed = 0;
        std::uint64_t failed_pings = 0;
        std::chrono::steady_clock::time_point last_update{};
    };
}
```

### Routing Interface

```cpp
namespace ss::dht {
    class i_routing : public ss::core::i_component {
    public:
        virtual ~i_routing() = default;
        
        // Peer management
        virtual ss::core::async_result<ss::core::result<void, std::string>>
        add_peer(peer_info peer) = 0;
        
        virtual ss::core::async_result<ss::core::result<bool, std::string>>
        remove_peer(const ss::core::node_id& node_id) = 0;
        
        // Peer lookup operations
        virtual ss::core::async_result<ss::core::result<std::vector<peer_info>, std::string>>
        find_closest_peers(const ss::core::node_id& target, std::uint32_t count = 0) = 0;
        
        virtual ss::core::async_result<ss::core::result<std::vector<peer_info>, std::string>>
        find_node(const ss::core::node_id& target, std::uint32_t max_results = 20) = 0;
        
        virtual ss::core::async_result<ss::core::result<peer_info, std::string>>
        get_peer(const ss::core::node_id& node_id) = 0;
        
        // Synchronous queries
        virtual bool has_peer(const ss::core::node_id& node_id) const noexcept = 0;
        virtual std::vector<peer_info> get_all_peers() const = 0;
        virtual std::vector<peer_info> get_bucket_peers(std::uint32_t bucket_index) const = 0;
        
        // Liveness management
        virtual ss::core::async_result<ss::core::result<void, std::string>>
        update_peer_liveness(const ss::core::node_id& node_id, 
                            std::chrono::microseconds rtt = {}) = 0;
        
        virtual ss::core::async_result<ss::core::result<bool, std::string>>
        record_peer_failure(const ss::core::node_id& node_id) = 0;
        
        // Maintenance operations
        virtual ss::core::async_void perform_maintenance() = 0;
        
        // Information and statistics
        virtual routing_stats get_stats() const noexcept = 0;
        virtual void reset_stats() noexcept = 0;
        virtual const ss::core::node_id& local_node_id() const noexcept = 0;
        virtual std::uint32_t bucket_index(const ss::core::node_id& node_id) const noexcept = 0;
        virtual std::uint32_t peer_count() const noexcept = 0;
        virtual std::uint32_t active_bucket_count() const noexcept = 0;
        
        // Persistence
        virtual std::vector<std::uint8_t> export_state() const = 0;
        virtual ss::core::async_result<ss::core::result<void, std::string>>
        import_state(const std::vector<std::uint8_t>& data) = 0;
    };
    
    using routing_ptr = std::shared_ptr<i_routing>;
}
```

### Usage Examples

```cpp
// Setting up a Kademlia routing table
auto setup_routing(boost::asio::io_context& io_context) -> routing_ptr {
    auto routing = std::make_shared<kademlia_routing>(io_context);
    
    // Set local node ID
    auto local_id = core::node_id::random();
    routing->set_local_node_id(local_id);
    
    return routing;
}

// Performing a DHT lookup
auto dht_lookup(routing_ptr routing, const core::node_id& target) -> ss::core::async_result<std::vector<peer_info>> {
    // Find closest nodes to target
    auto result = co_await routing->find_node(target, 20);
    
    if (!result.is_ok()) {
        throw std::runtime_error("DHT lookup failed: " + result.error());
    }
    
    auto peers = result.value();
    
    // Sort by distance to target
    std::sort(peers.begin(), peers.end(), 
        [&target](const auto& a, const auto& b) {
            return target.distance(a.id) < target.distance(b.id);
        });
    
    co_return peers;
}

// Adding peers with error handling
auto bootstrap_from_peers(routing_ptr routing, const std::vector<core::endpoint>& bootstrap_endpoints) -> ss::core::async_void {
    for (const auto& endpoint : bootstrap_endpoints) {
        try {
            // Create peer info (ID will be discovered through ping)
            peer_info peer{core::node_id{}, endpoint, std::chrono::steady_clock::now()};
            
            auto result = co_await routing->add_peer(peer);
            if (result.is_ok()) {
                std::cout << "Added bootstrap peer: " << endpoint.to_string() << std::endl;
            } else {
                std::cerr << "Failed to add peer " << endpoint.to_string() << ": " << result.error() << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception adding peer " << endpoint.to_string() << ": " << e.what() << std::endl;
        }
    }
}

// Periodic maintenance
auto maintain_routing_table(routing_ptr routing) -> ss::core::async_void {
    boost::asio::steady_timer timer(co_await boost::asio::this_coro::executor);
    
    while (routing->is_running()) {
        // Perform maintenance (refresh buckets, ping stale peers)
        co_await routing->perform_maintenance();
        
        // Print statistics
        auto stats = routing->get_stats();
        std::cout << "Routing table stats: " << stats.total_peers << " peers, " 
                  << stats.active_buckets << " active buckets" << std::endl;
        
        // Wait for next maintenance cycle
        timer.expires_after(std::chrono::minutes{1});
        co_await timer.async_wait();
    }
}
```

---

## ICE API

The ICE (Interactive Connectivity Establishment) API provides NAT traversal capabilities for peer-to-peer connectivity.

### Core Structures

```cpp
namespace ss::ice {
    // NAT type classification
    enum class nat_type {
        open_internet,      // Direct connection
        full_cone,          // Full cone NAT
        restricted_cone,    // Restricted cone NAT
        port_restricted_cone, // Port restricted cone NAT
        symmetric,          // Symmetric NAT (difficult to traverse)
        unknown,            // Could not determine
        blocked             // Network blocked
    };
    
    // NAT detection result
    struct nat_detection_result {
        nat_type type = nat_type::unknown;
        std::optional<ss::core::endpoint> external_endpoint;
        ss::core::endpoint local_endpoint;
        std::chrono::milliseconds detection_latency{0};
        std::string metadata;
        
        bool is_traversable() const noexcept;
    };
    
    // Hole punching configuration
    struct hole_punch_config {
        std::uint32_t max_attempts = 10;
        std::chrono::milliseconds attempt_interval{100};
        std::chrono::milliseconds total_timeout{5000};
        bool use_port_prediction = true;
        std::uint32_t port_prediction_range = 5;
        bool enable_parallel_punch = true;
        std::uint32_t parallel_threads = 3;
    };
    
    // Hole punching result
    struct hole_punch_result {
        bool success = false;
        ss::core::endpoint local_endpoint;
        ss::core::endpoint remote_endpoint;
        std::uint32_t attempts_made = 0;
        std::chrono::milliseconds elapsed_time{0};
        std::string punch_method;
    };
    
    // Callback types
    using nat_detection_handler = std::function<void(ss::core::result<nat_detection_result, nat_error>)>;
    using hole_punch_handler = std::function<void(ss::core::result<hole_punch_result, nat_error>)>;
    using traversal_progress_handler = std::function<void(const std::string& progress_info)>;
}
```

### NAT Traversal Interface

```cpp
namespace ss::ice {
    class i_nat_traversal : public ss::core::i_component {
    public:
        virtual ~i_nat_traversal() = default;
        
        // NAT detection
        virtual ss::core::async_result<ss::core::result<nat_detection_result, nat_error>>
        detect_nat(const ss::core::endpoint& local_endpoint,
                  const std::vector<ss::core::endpoint>& detection_servers) = 0;
        
        virtual void detect_nat_async(const ss::core::endpoint& local_endpoint,
                                     const std::vector<ss::core::endpoint>& detection_servers,
                                     nat_detection_handler handler) = 0;
        
        // Hole punching
        virtual ss::core::async_result<ss::core::result<hole_punch_result, nat_error>>
        punch_hole(const ss::core::endpoint& local_endpoint,
                  const ss::core::endpoint& target_endpoint,
                  const hole_punch_config& config = {}) = 0;
        
        virtual void punch_hole_async(const ss::core::endpoint& local_endpoint,
                                     const ss::core::endpoint& target_endpoint,
                                     hole_punch_handler handler,
                                     const hole_punch_config& config = {}) = 0;
        
        // Connection establishment
        virtual ss::core::async_result<ss::core::result<hole_punch_result, nat_error>>
        establish_connection(const ss::core::endpoint& local_endpoint,
                            const ss::core::peer_id& remote_peer_id,
                            const std::vector<ss::core::endpoint>& signaling_servers) = 0;
        
        // Caching and prediction
        virtual std::optional<nat_detection_result> 
        get_cached_nat_info(const ss::core::endpoint& local_endpoint) const noexcept = 0;
        
        virtual void clear_nat_cache(const std::optional<ss::core::endpoint>& local_endpoint = std::nullopt) = 0;
        
        virtual std::vector<std::uint16_t> 
        predict_external_ports(const ss::core::endpoint& local_endpoint,
                              const nat_detection_result& nat_result) const = 0;
        
        // Connectivity analysis
        virtual bool can_connect_directly(const nat_detection_result& local_nat,
                                         const nat_detection_result& remote_nat) const noexcept = 0;
        
        // Progress monitoring
        virtual void set_progress_handler(traversal_progress_handler handler) = 0;
        virtual void clear_progress_handler() = 0;
        
        // Statistics
        virtual struct nat_traversal_stats {
            std::uint64_t detection_attempts = 0;
            std::uint64_t detection_successes = 0;
            std::uint64_t hole_punch_attempts = 0;
            std::uint64_t hole_punch_successes = 0;
            std::uint64_t connection_establishments = 0;
            std::chrono::milliseconds avg_detection_time{0};
            std::chrono::milliseconds avg_hole_punch_time{0};
            std::uint32_t cache_hits = 0;
            std::uint32_t cache_misses = 0;
        } get_stats() const noexcept = 0;
        
        virtual void reset_stats() noexcept = 0;
    };
    
    using nat_traversal_ptr = std::shared_ptr<i_nat_traversal>;
}
```

### Usage Examples

```cpp
// NAT detection and analysis
auto detect_network_type(nat_traversal_ptr nat_traversal, const core::endpoint& local_ep) -> ss::core::async_result<nat_detection_result> {
    // List of STUN servers for detection
    std::vector<core::endpoint> stun_servers = {
        {"stun.l.google.com", 19302},
        {"stun1.l.google.com", 19302},
        {"stun2.l.google.com", 19302}
    };
    
    auto result = co_await nat_traversal->detect_nat(local_ep, stun_servers);
    
    if (!result.is_ok()) {
        throw std::runtime_error("NAT detection failed: " + to_string(result.error()));
    }
    
    auto detection = result.value();
    
    std::cout << "NAT type: " << to_string(detection.type) << std::endl;
    if (detection.external_endpoint) {
        std::cout << "External endpoint: " << detection.external_endpoint->to_string() << std::endl;
    }
    std::cout << "Detection latency: " << detection.detection_latency.count() << "ms" << std::endl;
    std::cout << "Traversable: " << (detection.is_traversable() ? "Yes" : "No") << std::endl;
    
    co_return detection;
}

// Establishing P2P connection through NAT
auto establish_p2p_connection(nat_traversal_ptr nat_traversal,
                             const core::endpoint& local_ep,
                             const core::peer_id& remote_peer) -> ss::core::async_result<hole_punch_result> {
    
    // Configure hole punching parameters
    hole_punch_config config;
    config.max_attempts = 15;
    config.attempt_interval = std::chrono::milliseconds{150};
    config.total_timeout = std::chrono::milliseconds{10000};
    config.use_port_prediction = true;
    config.enable_parallel_punch = true;
    
    // Set progress handler
    nat_traversal->set_progress_handler([](const std::string& progress) {
        std::cout << "NAT traversal progress: " << progress << std::endl;
    });
    
    // List of signaling relay servers
    std::vector<core::endpoint> signaling_servers = {
        {"relay1.example.com", 8080},
        {"relay2.example.com", 8080}
    };
    
    auto result = co_await nat_traversal->establish_connection(local_ep, remote_peer, signaling_servers);
    
    if (!result.is_ok()) {
        std::cerr << "Connection establishment failed: " << to_string(result.error()) << std::endl;
        co_return hole_punch_result{};
    }
    
    auto connection = result.value();
    
    if (connection.success) {
        std::cout << "P2P connection established!" << std::endl;
        std::cout << "Local endpoint: " << connection.local_endpoint.to_string() << std::endl;
        std::cout << "Remote endpoint: " << connection.remote_endpoint.to_string() << std::endl;
        std::cout << "Method: " << connection.punch_method << std::endl;
        std::cout << "Attempts: " << connection.attempts_made << std::endl;
        std::cout << "Time taken: " << connection.elapsed_time.count() << "ms" << std::endl;
    } else {
        std::cout << "Failed to establish P2P connection after " << connection.attempts_made << " attempts" << std::endl;
    }
    
    co_return connection;
}

// Analyzing connectivity between two peers
auto analyze_connectivity(nat_traversal_ptr nat_traversal, 
                         const nat_detection_result& local_nat,
                         const nat_detection_result& remote_nat) {
    bool can_connect = nat_traversal->can_connect_directly(local_nat, remote_nat);
    
    std::cout << "Connectivity Analysis:" << std::endl;
    std::cout << "Local NAT:  " << to_string(local_nat.type) << std::endl;
    std::cout << "Remote NAT: " << to_string(remote_nat.type) << std::endl;
    std::cout << "Direct connection possible: " << (can_connect ? "Yes" : "No") << std::endl;
    
    if (!can_connect) {
        std::cout << "Recommended approach: Use relay server" << std::endl;
    } else {
        // Predict port mappings for better hole punching
        if (local_nat.external_endpoint) {
            auto predicted_ports = nat_traversal->predict_external_ports(
                local_nat.local_endpoint, local_nat);
            
            std::cout << "Predicted external ports: ";
            for (auto port : predicted_ports) {
                std::cout << port << " ";
            }
            std::cout << std::endl;
        }
    }
}
```

---

## Security API

The Security API provides comprehensive cryptographic operations including encryption, digital signatures, and key management.

### Core Structures

```cpp
namespace ss::security {
    // Cryptographic error codes
    enum class crypto_error {
        none = 0,
        invalid_key,
        invalid_input,
        encryption_failed,
        decryption_failed,
        verification_failed,
        key_generation_failed,
        signature_failed,
        hash_failed,
        random_failed,
        insufficient_entropy,
        unsupported_algorithm,
        timeout,
        unknown
    };
    
    // Algorithm enumerations
    enum class symmetric_algorithm {
        aes_256_gcm,
        aes_192_gcm,
        aes_128_gcm,
        chacha20_poly1305
    };
    
    enum class asymmetric_algorithm {
        rsa_oaep,
        ecies_p256,
        x25519_chacha20_poly1305
    };
    
    enum class signature_algorithm {
        ed25519,
        ecdsa_p256_sha256,
        rsa_pss_sha256
    };
    
    enum class hash_algorithm {
        sha256,
        sha384,
        sha512,
        sha3_256,
        sha3_512,
        blake2b_256,
        blake2b_512
    };
    
    // Secure memory container
    class secure_bytes {
    public:
        using value_type = std::uint8_t;
        using size_type = std::size_t;
        
        explicit secure_bytes(size_type size);
        explicit secure_bytes(const std::vector<std::uint8_t>& data);
        explicit secure_bytes(std::vector<std::uint8_t>&& data);
        
        // Automatically zeroes memory on destruction
        ~secure_bytes();
        
        value_type* data() noexcept;
        const value_type* data() const noexcept;
        size_type size() const noexcept;
        bool empty() const noexcept;
        
        void resize(size_type new_size);
        void clear() noexcept;
        
        std::vector<std::uint8_t> to_vector() const;
    };
    
    // Cryptographic key representation
    class crypto_key {
    public:
        crypto_key(key_type type, std::string algorithm, secure_bytes data);
        
        key_type type() const noexcept;
        const std::string& algorithm() const noexcept;
        const secure_bytes& data() const noexcept;
        std::size_t size() const noexcept;
        
        bool is_valid() const noexcept;
        std::string fingerprint() const;
    };
    
    // Key pair for asymmetric operations
    struct key_pair {
        crypto_key public_key;
        crypto_key private_key;
    };
    
    // Encrypted data structure
    struct encrypted_data {
        std::vector<std::uint8_t> ciphertext;
        std::vector<std::uint8_t> tag;  // Authentication tag
        std::vector<std::uint8_t> iv;   // Initialization vector
        std::vector<std::uint8_t> aad;  // Additional authenticated data
        std::string algorithm;
    };
    
    // Digital signature structure
    struct signature_data {
        std::vector<std::uint8_t> signature;
        std::string algorithm;
        std::string signer_fingerprint;
        std::chrono::system_clock::time_point timestamp;
    };
}
```

### Crypto Interface

```cpp
namespace ss::security {
    class i_crypto : public ss::core::i_component {
    public:
        virtual ~i_crypto() = default;
        
        // === Symmetric Encryption ===
        
        virtual ss::core::result<crypto_key, crypto_error>
        generate_symmetric_key(symmetric_algorithm algorithm) = 0;
        
        virtual ss::core::result<encrypted_data, crypto_error>
        symmetric_encrypt(
            const std::vector<std::uint8_t>& plaintext,
            const crypto_key& key,
            const std::vector<std::uint8_t>& aad = {}) = 0;
        
        virtual ss::core::result<std::vector<std::uint8_t>, crypto_error>
        symmetric_decrypt(
            const encrypted_data& encrypted,
            const crypto_key& key) = 0;
        
        // === Asymmetric Encryption ===
        
        virtual ss::core::result<key_pair, crypto_error>
        generate_key_pair(asymmetric_algorithm algorithm) = 0;
        
        virtual ss::core::result<std::vector<std::uint8_t>, crypto_error>
        asymmetric_encrypt(
            const std::vector<std::uint8_t>& plaintext,
            const crypto_key& public_key) = 0;
        
        virtual ss::core::result<std::vector<std::uint8_t>, crypto_error>
        asymmetric_decrypt(
            const std::vector<std::uint8_t>& ciphertext,
            const crypto_key& private_key) = 0;
        
        // === Digital Signatures ===
        
        virtual ss::core::result<key_pair, crypto_error>
        generate_signature_key_pair(signature_algorithm algorithm) = 0;
        
        virtual ss::core::result<signature_data, crypto_error>
        sign(
            const std::vector<std::uint8_t>& data,
            const crypto_key& private_key) = 0;
        
        virtual ss::core::result<bool, crypto_error>
        verify(
            const std::vector<std::uint8_t>& data,
            const signature_data& signature,
            const crypto_key& public_key) = 0;
        
        // === Cryptographic Hashing ===
        
        virtual ss::core::result<std::vector<std::uint8_t>, crypto_error>
        hash(
            const std::vector<std::uint8_t>& data,
            hash_algorithm algorithm) = 0;
        
        virtual ss::core::result<std::vector<std::uint8_t>, crypto_error>
        hmac(
            const std::vector<std::uint8_t>& data,
            const crypto_key& key,
            hash_algorithm algorithm) = 0;
        
        // === Key Derivation ===
        
        virtual ss::core::result<crypto_key, crypto_error>
        derive_key_pbkdf2(
            const std::string& password,
            const std::vector<std::uint8_t>& salt,
            std::uint32_t iterations,
            std::size_t key_length,
            hash_algorithm hash_alg = hash_algorithm::sha256) = 0;
        
        virtual ss::core::result<crypto_key, crypto_error>
        derive_key_hkdf(
            const crypto_key& input_key_material,
            const std::vector<std::uint8_t>& salt,
            const std::vector<std::uint8_t>& info,
            std::size_t key_length,
            hash_algorithm hash_alg = hash_algorithm::sha256) = 0;
        
        // === Random Number Generation ===
        
        virtual ss::core::result<std::vector<std::uint8_t>, crypto_error>
        random_bytes(std::size_t length) = 0;
        
        // === Utility Functions ===
        
        virtual bool is_algorithm_supported(const std::string& algorithm) const noexcept = 0;
        virtual std::size_t get_key_size(const std::string& algorithm) const noexcept = 0;
        virtual std::uint32_t get_security_level(const std::string& algorithm) const noexcept = 0;
        
        virtual bool constant_time_compare(
            const std::vector<std::uint8_t>& a,
            const std::vector<std::uint8_t>& b) const noexcept = 0;
    };
    
    using crypto_ptr = std::shared_ptr<i_crypto>;
}
```

### Usage Examples

```cpp
// Secure message encryption and decryption
auto secure_message_exchange(crypto_ptr crypto) -> ss::core::async_result<void> {
    // Generate symmetric key for message encryption
    auto key_result = crypto->generate_symmetric_key(symmetric_algorithm::aes_256_gcm);
    if (!key_result.is_ok()) {
        throw std::runtime_error("Failed to generate encryption key");
    }
    auto encryption_key = key_result.value();
    
    // Message to encrypt
    std::string message = "This is a confidential P2P message";
    std::vector<std::uint8_t> plaintext(message.begin(), message.end());
    
    // Additional authenticated data (optional)
    std::string aad_str = "P2P-MSG-v1.0";
    std::vector<std::uint8_t> aad(aad_str.begin(), aad_str.end());
    
    // Encrypt the message
    auto encrypt_result = crypto->symmetric_encrypt(plaintext, encryption_key, aad);
    if (!encrypt_result.is_ok()) {
        throw std::runtime_error("Encryption failed: " + to_string(encrypt_result.error()));
    }
    auto encrypted = encrypt_result.value();
    
    std::cout << "Message encrypted successfully" << std::endl;
    std::cout << "Ciphertext size: " << encrypted.ciphertext.size() << " bytes" << std::endl;
    std::cout << "Authentication tag size: " << encrypted.tag.size() << " bytes" << std::endl;
    
    // Decrypt the message
    auto decrypt_result = crypto->symmetric_decrypt(encrypted, encryption_key);
    if (!decrypt_result.is_ok()) {
        throw std::runtime_error("Decryption failed: " + to_string(decrypt_result.error()));
    }
    auto decrypted = decrypt_result.value();
    
    std::string recovered_message(decrypted.begin(), decrypted.end());
    std::cout << "Decrypted message: " << recovered_message << std::endl;
    
    // Verify message integrity
    if (message == recovered_message) {
        std::cout << "Message integrity verified!" << std::endl;
    } else {
        throw std::runtime_error("Message integrity check failed!");
    }
}

// Digital signatures for message authentication
auto message_signing_example(crypto_ptr crypto) -> ss::core::async_result<void> {
    // Generate signature key pair
    auto keypair_result = crypto->generate_signature_key_pair(signature_algorithm::ed25519);
    if (!keypair_result.is_ok()) {
        throw std::runtime_error("Failed to generate signature keys");
    }
    auto keypair = keypair_result.value();
    
    // Message to sign
    std::string message = "Important P2P network announcement";
    std::vector<std::uint8_t> message_data(message.begin(), message.end());
    
    // Sign the message
    auto sign_result = crypto->sign(message_data, keypair.private_key);
    if (!sign_result.is_ok()) {
        throw std::runtime_error("Signing failed: " + to_string(sign_result.error()));
    }
    auto signature = sign_result.value();
    
    std::cout << "Message signed successfully" << std::endl;
    std::cout << "Signature size: " << signature.signature.size() << " bytes" << std::endl;
    std::cout << "Signer fingerprint: " << signature.signer_fingerprint << std::endl;
    
    // Verify the signature
    auto verify_result = crypto->verify(message_data, signature, keypair.public_key);
    if (!verify_result.is_ok()) {
        throw std::runtime_error("Verification failed: " + to_string(verify_result.error()));
    }
    
    bool is_valid = verify_result.value();
    if (is_valid) {
        std::cout << "Signature verification successful!" << std::endl;
    } else {
        std::cout << "Signature verification failed!" << std::endl;
    }
}

// Key derivation for secure session keys
auto derive_session_keys(crypto_ptr crypto, const std::string& shared_secret) -> ss::core::async_result<std::pair<crypto_key, crypto_key>> {
    // Generate random salt
    auto salt_result = crypto->random_bytes(32);
    if (!salt_result.is_ok()) {
        throw std::runtime_error("Failed to generate salt");
    }
    auto salt = salt_result.value();
    
    // Create master key from shared secret
    auto master_key_result = crypto->derive_key_pbkdf2(shared_secret, salt, 100000, 32);
    if (!master_key_result.is_ok()) {
        throw std::runtime_error("Failed to derive master key");
    }
    auto master_key = master_key_result.value();
    
    // Derive encryption key
    std::string enc_info = "P2P-ENCRYPTION-KEY";
    std::vector<std::uint8_t> enc_info_bytes(enc_info.begin(), enc_info.end());
    
    auto enc_key_result = crypto->derive_key_hkdf(master_key, salt, enc_info_bytes, 32);
    if (!enc_key_result.is_ok()) {
        throw std::runtime_error("Failed to derive encryption key");
    }
    auto encryption_key = enc_key_result.value();
    
    // Derive MAC key
    std::string mac_info = "P2P-MAC-KEY";
    std::vector<std::uint8_t> mac_info_bytes(mac_info.begin(), mac_info.end());
    
    auto mac_key_result = crypto->derive_key_hkdf(master_key, salt, mac_info_bytes, 32);
    if (!mac_key_result.is_ok()) {
        throw std::runtime_error("Failed to derive MAC key");
    }
    auto mac_key = mac_key_result.value();
    
    std::cout << "Session keys derived successfully" << std::endl;
    std::cout << "Encryption key fingerprint: " << encryption_key.fingerprint() << std::endl;
    std::cout << "MAC key fingerprint: " << mac_key.fingerprint() << std::endl;
    
    co_return std::make_pair(encryption_key, mac_key);
}
```

---

## Application API

The Application API provides high-level interfaces for building P2P applications, including node management, message bus, and chat services.

### P2P Node Interface

```cpp
namespace ss::application {
    // Node configuration
    struct node_config {
        core::endpoint local_endpoint;
        std::vector<core::endpoint> bootstrap_nodes;
        std::optional<core::node_id> node_id;
        std::size_t max_connections = 1000;
        std::chrono::milliseconds connection_timeout{5000};
        std::chrono::milliseconds dht_refresh_interval{60000};
        bool enable_nat_traversal = true;
        bool enable_encryption = true;
        std::chrono::milliseconds ping_timeout{2000};
        std::chrono::milliseconds find_node_timeout{5000};
        core::i_logger::level log_level = core::i_logger::level::info;
        nlohmann::json app_config;
        
        core::result<void, std::string> validate() const;
        static core::result<node_config, std::string> from_json(const nlohmann::json& config_json);
        nlohmann::json to_json() const;
    };
    
    // Node statistics
    struct node_stats {
        std::size_t active_connections = 0;
        std::size_t routing_table_size = 0;
        std::size_t messages_sent = 0;
        std::size_t messages_received = 0;
        std::size_t messages_failed = 0;
        std::size_t bytes_sent = 0;
        std::size_t bytes_received = 0;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::milliseconds uptime{0};
        std::chrono::milliseconds avg_ping_latency{0};
        std::size_t nat_success_count = 0;
        std::size_t nat_failure_count = 0;
        std::size_t dht_lookups_performed = 0;
        std::size_t dht_lookups_successful = 0;
        
        nlohmann::json to_json() const;
        void update_uptime();
    };
    
    // Peer information
    struct peer_info {
        core::node_id id;
        core::endpoint endpoint;
        enum class status {
            unknown, connecting, connected, disconnected, failed
        } connection_status = status::unknown;
        std::chrono::steady_clock::time_point last_seen;
        std::chrono::milliseconds latency{0};
        std::unordered_set<std::string> capabilities;
        double quality_score = 0.0;
        
        bool is_alive(std::chrono::milliseconds timeout = std::chrono::milliseconds{30000}) const;
        nlohmann::json to_json() const;
    };
    
    // Callback types
    using message_handler = std::function<core::async_void(const ss::message&, const core::endpoint&)>;
    using peer_event_handler = std::function<void(const peer_info&, const std::string& event)>;
}
```

#### P2P Node Interface

```cpp
namespace ss::application {
    class i_p2p_node : public core::i_component,
                       public core::i_configurable<node_config>,
                       public core::i_observable<peer_info> {
    public:
        virtual ~i_p2p_node() = default;
        
        // === Lifecycle Management ===
        
        virtual core::async_void initialize(const node_config& config) = 0;
        virtual core::async_void start() override = 0;
        virtual core::async_void stop() override = 0;
        virtual core::async_void restart() = 0;
        
        // === Message Communication ===
        
        virtual core::async_result<bool> send_message(
            const ss::message& message,
            const core::endpoint& target_endpoint,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{5000}
        ) = 0;
        
        virtual core::async_result<bool> send_message(
            const ss::message& message,
            const core::node_id& target_id,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{10000}
        ) = 0;
        
        virtual core::async_result<std::size_t> broadcast_message(
            const ss::message& message,
            std::size_t max_peers = 0
        ) = 0;
        
        virtual void register_message_handler(
            const ss::message::app_id& app_id,
            message_handler handler
        ) = 0;
        
        virtual void unregister_message_handler(const ss::message::app_id& app_id) = 0;
        
        // === Peer Management ===
        
        virtual std::vector<peer_info> get_peers() const = 0;
        virtual std::optional<peer_info> get_peer(const core::node_id& peer_id) const = 0;
        virtual std::optional<peer_info> get_peer(const core::endpoint& endpoint) const = 0;
        
        virtual core::async_result<bool> connect_to_peer(
            const core::endpoint& endpoint,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{10000}
        ) = 0;
        
        virtual core::async_void disconnect_from_peer(const core::node_id& peer_id) = 0;
        
        virtual core::async_result<std::vector<peer_info>> find_peers(
            const core::node_id& target_id,
            std::size_t max_peers = 20
        ) = 0;
        
        virtual std::size_t register_peer_event_handler(peer_event_handler handler) = 0;
        virtual void unregister_peer_event_handler(std::size_t handler_id) = 0;
        
        // === Node Information ===
        
        virtual core::node_id get_node_id() const noexcept = 0;
        virtual core::endpoint get_local_endpoint() const noexcept = 0;
        virtual node_stats get_stats() const = 0;
        virtual node_config get_config() const override = 0;
        
        // === Network Operations ===
        
        virtual core::async_void perform_maintenance() = 0;
        
        virtual core::async_result<bool> join_network(
            const std::vector<core::endpoint>& bootstrap_nodes,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{30000}
        ) = 0;
        
        virtual core::async_void leave_network() = 0;
        
        virtual core::async_result<std::chrono::milliseconds> ping_peer(
            const core::endpoint& endpoint,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{5000}
        ) = 0;
        
        // === Advanced Features ===
        
        virtual void set_nat_traversal_enabled(bool enabled) = 0;
        virtual bool is_nat_traversal_enabled() const noexcept = 0;
        virtual void set_encryption_enabled(bool enabled) = 0;
        virtual bool is_encryption_enabled() const noexcept = 0;
        virtual void set_max_connections(std::size_t max_connections) = 0;
        virtual std::size_t get_max_connections() const noexcept = 0;
        
        // === Error Handling ===
        
        virtual std::string get_last_error() const = 0;
        virtual void clear_error() = 0;
    };
    
    // Factory function
    std::unique_ptr<i_p2p_node> create_p2p_node(
        boost::asio::io_context& io_context,
        const node_config& config = {}
    );
}
```

### Node Builder Pattern

```cpp
namespace ss::application {
    class node_builder {
    public:
        explicit node_builder(boost::asio::io_context& io_context);
        
        node_builder& with_local_endpoint(const core::endpoint& endpoint);
        node_builder& with_bootstrap_node(const core::endpoint& endpoint);
        node_builder& with_node_id(const core::node_id& id);
        node_builder& with_max_connections(std::size_t max_connections);
        node_builder& with_nat_traversal(bool enabled);
        node_builder& with_encryption(bool enabled);
        node_builder& with_log_level(core::i_logger::level level);
        node_builder& with_timeouts(
            std::chrono::milliseconds connection_timeout,
            std::chrono::milliseconds ping_timeout
        );
        node_builder& with_config_file(const std::string& config_path);
        
        std::unique_ptr<i_p2p_node> build();
    };
}
```

### Usage Examples

```cpp
// Creating and configuring a P2P node
auto create_configured_node(boost::asio::io_context& io_context) -> std::unique_ptr<i_p2p_node> {
    return node_builder(io_context)
        .with_local_endpoint(core::endpoint{"0.0.0.0", 8080})
        .with_bootstrap_node(core::endpoint{"bootstrap.example.com", 8080})
        .with_max_connections(500)
        .with_nat_traversal(true)
        .with_encryption(true)
        .with_log_level(core::i_logger::level::info)
        .with_timeouts(std::chrono::milliseconds{5000}, std::chrono::milliseconds{2000})
        .build();
}

// Complete P2P application lifecycle
auto run_p2p_application() -> ss::core::async_result<void> {
    boost::asio::io_context io_context;
    
    // Create and configure node
    auto node = create_configured_node(io_context);
    
    // Register message handlers
    node->register_message_handler("chat", [](const auto& message, const auto& sender) -> core::async_void {
        std::cout << "Received chat message from " << sender.to_string() << ": " 
                  << std::string(message.payload.begin(), message.payload.end()) << std::endl;
    });
    
    // Register peer event handler
    auto handler_id = node->register_peer_event_handler([](const auto& peer, const auto& event) {
        std::cout << "Peer event: " << event << " for peer " << peer.id.to_hex() << std::endl;
    });
    
    try {
        // Initialize and start node
        co_await node->initialize(node->get_config());
        co_await node->start();
        
        std::cout << "P2P node started with ID: " << node->get_node_id().to_hex() << std::endl;
        std::cout << "Local endpoint: " << node->get_local_endpoint().to_string() << std::endl;
        
        // Join the network
        std::vector<core::endpoint> bootstrap_nodes = {
            {"bootstrap1.example.com", 8080},
            {"bootstrap2.example.com", 8080}
        };
        
        bool joined = co_await node->join_network(bootstrap_nodes);
        if (!joined) {
            throw std::runtime_error("Failed to join P2P network");
        }
        
        std::cout << "Successfully joined P2P network" << std::endl;
        
        // Wait for peers to connect
        co_await boost::asio::steady_timer(io_context).async_wait(std::chrono::seconds{5});
        
        // Send a broadcast message
        ss::message broadcast_msg;
        broadcast_msg.app_id = "chat";
        broadcast_msg.payload = std::vector<std::uint8_t>{'H', 'e', 'l', 'l', 'o', ' ', 'P', '2', 'P', '!'};
        
        auto sent_count = co_await node->broadcast_message(broadcast_msg);
        std::cout << "Broadcast message sent to " << sent_count << " peers" << std::endl;
        
        // Print statistics
        auto stats = node->get_stats();
        std::cout << "Node statistics:" << std::endl;
        std::cout << stats.to_json().dump(2) << std::endl;
        
        // Run for a while
        co_await boost::asio::steady_timer(io_context).async_wait(std::chrono::minutes{5});
        
        // Graceful shutdown
        std::cout << "Shutting down P2P node..." << std::endl;
        co_await node->leave_network();
        co_await node->stop();
        
    } catch (const std::exception& e) {
        std::cerr << "P2P application error: " << e.what() << std::endl;
        
        // Cleanup
        node->unregister_peer_event_handler(handler_id);
        node->unregister_message_handler("chat");
        
        if (node->is_running()) {
            co_await node->stop();
        }
        
        throw;
    }
}

// Peer discovery and communication
auto discover_and_communicate(std::unique_ptr<i_p2p_node>& node) -> ss::core::async_result<void> {
    // Find peers near a specific target
    auto target_id = core::node_id::random();
    auto peers = co_await node->find_peers(target_id, 10);
    
    std::cout << "Found " << peers.size() << " peers near target " << target_id.to_hex() << std::endl;
    
    for (const auto& peer : peers) {
        std::cout << "Peer: " << peer.id.to_hex() << " at " << peer.endpoint.to_string() 
                  << " (quality: " << peer.quality_score << ")" << std::endl;
        
        // Test connectivity
        auto latency = co_await node->ping_peer(peer.endpoint);
        if (latency.count() > 0) {
            std::cout << "  Ping: " << latency.count() << "ms" << std::endl;
            
            // Send a direct message
            ss::message direct_msg;
            direct_msg.app_id = "direct";
            direct_msg.payload = std::vector<std::uint8_t>{'H', 'i', '!'};
            
            bool sent = co_await node->send_message(direct_msg, peer.endpoint);
            if (sent) {
                std::cout << "  Direct message sent successfully" << std::endl;
            } else {
                std::cout << "  Failed to send direct message" << std::endl;
            }
        } else {
            std::cout << "  Peer unreachable" << std::endl;
        }
    }
}
```

---

## Integration Guide

### Library Integration

#### CMake Integration

```cmake
# Find the SS P2P library
find_package(ss_p2p REQUIRED)

# Create your target
add_executable(my_p2p_app main.cpp)

# Link with SS P2P library
target_link_libraries(my_p2p_app 
    ss_p2p::ss_p2p
    Boost::system
    Boost::thread
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Set C++20 standard
target_compile_features(my_p2p_app PRIVATE cxx_std_20)
```

#### Manual Integration

```cpp
// Include the main header
#include <ss_p2p/application/i_p2p_node.hpp>

// Or include specific modules
#include <ss_p2p/network/i_transport.hpp>
#include <ss_p2p/dht/i_routing.hpp>
#include <ss_p2p/ice/i_nat_traversal.hpp>
#include <ss_p2p/security/i_crypto.hpp>
```

### Standalone Application

```cpp
#include <ss_p2p/application/i_p2p_node.hpp>
#include <boost/asio.hpp>
#include <iostream>

int main() {
    try {
        boost::asio::io_context io_context;
        
        // Create P2P node
        auto node = ss::application::node_builder(io_context)
            .with_local_endpoint(ss::core::endpoint{"0.0.0.0", 8080})
            .with_nat_traversal(true)
            .with_encryption(true)
            .build();
        
        // Start node
        boost::asio::co_spawn(io_context, [&]() -> ss::core::async_void {
            co_await node->start();
            std::cout << "Node started with ID: " << node->get_node_id().to_hex() << std::endl;
        }, boost::asio::detached);
        
        // Run event loop
        io_context.run();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

### systemd Service Configuration

Create `/etc/systemd/system/my-p2p-app.service`:

```ini
[Unit]
Description=My P2P Application
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=p2p
Group=p2p
WorkingDirectory=/opt/my-p2p-app
ExecStart=/opt/my-p2p-app/bin/my_p2p_app --config /etc/my-p2p-app/config.json --daemon
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

# Security settings
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/my-p2p-app /var/log/my-p2p-app

[Install]
WantedBy=multi-user.target
```

Enable and start the service:

```bash
sudo systemctl enable my-p2p-app
sudo systemctl start my-p2p-app
sudo systemctl status my-p2p-app
```

---

## Code Examples

### Basic P2P Chat Application

```cpp
#include <ss_p2p/application/i_p2p_node.hpp>
#include <ss_p2p/application/i_message_bus.hpp>
#include <ss_p2p/application/i_chat.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <string>

class simple_chat {
private:
    boost::asio::io_context& io_context_;
    std::unique_ptr<ss::application::i_p2p_node> node_;
    std::unique_ptr<ss::application::i_message_bus> message_bus_;
    std::unique_ptr<ss::application::i_chat> chat_service_;
    
public:
    explicit simple_chat(boost::asio::io_context& io_context) 
        : io_context_(io_context) {}
    
    ss::core::async_void initialize() {
        // Create P2P node
        node_ = ss::application::node_builder(io_context_)
            .with_local_endpoint(ss::core::endpoint{"0.0.0.0", 0})  // Random port
            .with_nat_traversal(true)
            .with_encryption(true)
            .build();
        
        // Create message bus for pub/sub messaging
        ss::application::message_bus_config bus_config;
        bus_config.max_queue_size = 10000;
        bus_config.enable_persistence = false;
        
        message_bus_ = ss::application::create_message_bus(io_context_, bus_config);
        
        // Create chat service
        ss::application::chat_config chat_config;
        chat_config.max_message_length = 4096;
        chat_config.enable_encryption = true;
        
        chat_service_ = ss::application::create_chat_service(io_context_, chat_config);
        
        // Initialize components
        co_await node_->initialize(node_->get_config());
        co_await message_bus_->initialize(bus_config);
        co_await chat_service_->initialize(chat_config, node_, message_bus_);
        
        // Setup event handlers
        setup_handlers();
    }
    
    ss::core::async_void start() {
        co_await node_->start();
        co_await message_bus_->start();
        co_await chat_service_->start();
        
        std::cout << "Chat application started!" << std::endl;
        std::cout << "Node ID: " << node_->get_node_id().to_hex() << std::endl;
        std::cout << "Endpoint: " << node_->get_local_endpoint().to_string() << std::endl;
    }
    
    ss::core::async_void join_network(const std::vector<ss::core::endpoint>& bootstrap_nodes) {
        bool success = co_await node_->join_network(bootstrap_nodes);
        if (success) {
            std::cout << "Successfully joined P2P network!" << std::endl;
        } else {
            throw std::runtime_error("Failed to join network");
        }
    }
    
    ss::core::async_void register_user(const std::string& nickname) {
        auto user = co_await chat_service_->register_user(nickname);
        if (!user.nickname.empty()) {
            std::cout << "Registered as: " << user.nickname << std::endl;
        } else {
            throw std::runtime_error("Failed to register user");
        }
    }
    
    ss::core::async_void create_room(const std::string& room_name) {
        auto room_id = co_await chat_service_->create_room(room_name);
        if (!room_id.empty()) {
            std::cout << "Created room: " << room_id << std::endl;
        } else {
            throw std::runtime_error("Failed to create room");
        }
    }
    
    ss::core::async_void send_message(const std::string& room_id, const std::string& message) {
        auto msg_id = co_await chat_service_->send_message(room_id, message);
        if (msg_id.value() != 0) {
            std::cout << "Message sent to room " << room_id << std::endl;
        } else {
            std::cout << "Failed to send message" << std::endl;
        }
    }
    
private:
    void setup_handlers() {
        // Handle incoming chat messages
        chat_service_->set_message_received_handler([](const auto& message) {
            std::cout << "[" << message.sender_nickname << "] " << message.content << std::endl;
        });
        
        // Handle user join/leave events
        chat_service_->set_user_joined_handler([](const auto& user, const auto& room_id) {
            std::cout << "*** " << user.nickname << " joined " << room_id << " ***" << std::endl;
        });
        
        chat_service_->set_user_left_handler([](const auto& user_id, const auto& room_id) {
            std::cout << "*** User left " << room_id << " ***" << std::endl;
        });
    }
};

int main() {
    try {
        boost::asio::io_context io_context;
        simple_chat app(io_context);
        
        // Initialize and start
        boost::asio::co_spawn(io_context, [&]() -> ss::core::async_void {
            co_await app.initialize();
            co_await app.start();
            
            // Join network
            std::vector<ss::core::endpoint> bootstrap_nodes = {
                {"bootstrap.example.com", 8080}
            };
            co_await app.join_network(bootstrap_nodes);
            
            // Register user
            co_await app.register_user("Alice");
            
            // Create a room
            co_await app.create_room("General Chat");
            
            // Send a message
            co_await app.send_message("general", "Hello, P2P world!");
            
        }, boost::asio::detached);
        
        io_context.run();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

### File Sharing Application

```cpp
#include <ss_p2p/application/i_p2p_node.hpp>
#include <boost/asio.hpp>
#include <filesystem>
#include <fstream>

class p2p_file_sharing {
private:
    boost::asio::io_context& io_context_;
    std::unique_ptr<ss::application::i_p2p_node> node_;
    std::filesystem::path shared_directory_;
    
public:
    explicit p2p_file_sharing(boost::asio::io_context& io_context, 
                             const std::filesystem::path& shared_dir)
        : io_context_(io_context), shared_directory_(shared_dir) {}
    
    ss::core::async_void initialize() {
        // Create P2P node
        node_ = ss::application::node_builder(io_context_)
            .with_local_endpoint(ss::core::endpoint{"0.0.0.0", 9090})
            .with_nat_traversal(true)
            .with_encryption(true)
            .build();
        
        // Register message handlers
        node_->register_message_handler("file_request", 
            [this](const auto& msg, const auto& sender) -> ss::core::async_void {
                co_await handle_file_request(msg, sender);
            });
        
        node_->register_message_handler("file_response",
            [this](const auto& msg, const auto& sender) -> ss::core::async_void {
                co_await handle_file_response(msg, sender);
            });
        
        node_->register_message_handler("file_list_request",
            [this](const auto& msg, const auto& sender) -> ss::core::async_void {
                co_await handle_file_list_request(msg, sender);
            });
        
        co_await node_->initialize(node_->get_config());
        
        // Create shared directory if it doesn't exist
        std::filesystem::create_directories(shared_directory_);
    }
    
    ss::core::async_void start() {
        co_await node_->start();
        std::cout << "File sharing node started!" << std::endl;
        std::cout << "Shared directory: " << shared_directory_ << std::endl;
    }
    
    ss::core::async_void request_file(const std::string& filename, 
                                      const ss::core::endpoint& peer) {
        ss::message request;
        request.app_id = "file_request";
        request.payload.assign(filename.begin(), filename.end());
        
        bool sent = co_await node_->send_message(request, peer);
        if (sent) {
            std::cout << "File request sent for: " << filename << std::endl;
        } else {
            std::cout << "Failed to send file request" << std::endl;
        }
    }
    
    ss::core::async_void list_files() {
        std::cout << "Available files:" << std::endl;
        for (const auto& entry : std::filesystem::directory_iterator(shared_directory_)) {
            if (entry.is_regular_file()) {
                std::cout << "  " << entry.path().filename().string() 
                          << " (" << entry.file_size() << " bytes)" << std::endl;
            }
        }
    }
    
private:
    ss::core::async_void handle_file_request(const ss::message& msg, 
                                            const ss::core::endpoint& sender) {
        std::string filename(msg.payload.begin(), msg.payload.end());
        auto file_path = shared_directory_ / filename;
        
        if (std::filesystem::exists(file_path) && std::filesystem::is_regular_file(file_path)) {
            // Read file content
            std::ifstream file(file_path, std::ios::binary);
            std::vector<std::uint8_t> file_data(std::istreambuf_iterator<char>(file), {});
            
            // Prepare response
            ss::message response;
            response.app_id = "file_response";
            response.payload = std::move(file_data);
            
            // Add filename as metadata (in real implementation, use proper protocol)
            response.payload.insert(response.payload.begin(), filename.begin(), filename.end());
            response.payload.insert(response.payload.begin() + filename.length(), '\0');
            
            bool sent = co_await node_->send_message(response, sender);
            if (sent) {
                std::cout << "Sent file: " << filename << " to " << sender.to_string() << std::endl;
            }
        } else {
            std::cout << "File not found: " << filename << std::endl;
        }
    }
    
    ss::core::async_void handle_file_response(const ss::message& msg, 
                                             const ss::core::endpoint& sender) {
        if (msg.payload.empty()) return;
        
        // Extract filename (simplified protocol)
        auto null_pos = std::find(msg.payload.begin(), msg.payload.end(), '\0');
        if (null_pos == msg.payload.end()) return;
        
        std::string filename(msg.payload.begin(), null_pos);
        std::vector<std::uint8_t> file_data(null_pos + 1, msg.payload.end());
        
        // Save file
        auto save_path = shared_directory_ / ("received_" + filename);
        std::ofstream outfile(save_path, std::ios::binary);
        outfile.write(reinterpret_cast<const char*>(file_data.data()), file_data.size());
        outfile.close();
        
        std::cout << "Received file: " << filename << " (" << file_data.size() 
                  << " bytes) from " << sender.to_string() << std::endl;
        std::cout << "Saved as: " << save_path << std::endl;
    }
    
    ss::core::async_void handle_file_list_request(const ss::message& msg, 
                                                  const ss::core::endpoint& sender) {
        // Build file list
        std::string file_list;
        for (const auto& entry : std::filesystem::directory_iterator(shared_directory_)) {
            if (entry.is_regular_file()) {
                file_list += entry.path().filename().string() + ":" + 
                            std::to_string(entry.file_size()) + "\n";
            }
        }
        
        ss::message response;
        response.app_id = "file_list_response";
        response.payload.assign(file_list.begin(), file_list.end());
        
        co_await node_->send_message(response, sender);
    }
};

int main() {
    try {
        boost::asio::io_context io_context;
        p2p_file_sharing app(io_context, "./shared_files");
        
        boost::asio::co_spawn(io_context, [&]() -> ss::core::async_void {
            co_await app.initialize();
            co_await app.start();
            
            // List available files
            co_await app.list_files();
            
            // Example: request a file from another peer
            // app.request_file("example.txt", ss::core::endpoint{"peer.example.com", 9090});
            
        }, boost::asio::detached);
        
        io_context.run();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

### Error Handling Patterns

```cpp
// Using Result monad for comprehensive error handling
auto robust_p2p_operation() -> ss::core::async_result<void> {
    auto node = create_p2p_node();
    
    // Chain operations with proper error handling
    auto start_result = co_await node->start()
        .and_then([&]() -> ss::core::async_result<bool> {
            return node->join_network(bootstrap_nodes);
        })
        .map([&](bool joined) -> std::string {
            if (!joined) {
                throw std::runtime_error("Failed to join network");
            }
            return "Network joined successfully";
        })
        .or_else([](const auto& error) -> ss::core::result<std::string, std::string> {
            std::cerr << "Network operation failed: " << error << std::endl;
            return ss::core::result<std::string, std::string>::err("Network operation failed");
        });
    
    if (start_result.is_err()) {
        std::cerr << "P2P operation failed: " << start_result.error() << std::endl;
        co_return;
    }
    
    std::cout << start_result.value() << std::endl;
}

// Exception-safe RAII pattern
class p2p_node_guard {
private:
    std::unique_ptr<ss::application::i_p2p_node> node_;
    
public:
    explicit p2p_node_guard(std::unique_ptr<ss::application::i_p2p_node> node)
        : node_(std::move(node)) {}
    
    ~p2p_node_guard() {
        if (node_ && node_->is_running()) {
            // Synchronous shutdown in destructor (not recommended for production)
            // In real code, ensure async shutdown is handled properly
            try {
                boost::asio::io_context temp_context;
                boost::asio::co_spawn(temp_context, [&]() -> ss::core::async_void {
                    co_await node_->stop();
                }, boost::asio::detached);
                temp_context.run();
            } catch (...) {
                // Log error but don't throw from destructor
            }
        }
    }
    
    ss::application::i_p2p_node* operator->() { return node_.get(); }
    ss::application::i_p2p_node& operator*() { return *node_; }
    
    // Move-only
    p2p_node_guard(const p2p_node_guard&) = delete;
    p2p_node_guard& operator=(const p2p_node_guard&) = delete;
    p2p_node_guard(p2p_node_guard&&) = default;
    p2p_node_guard& operator=(p2p_node_guard&&) = default;
};
```

This comprehensive API reference provides developers with all the necessary information to integrate and use the SS P2P Node Controller library effectively. The modular design allows for flexible usage patterns, from simple peer-to-peer applications to complex distributed systems with advanced features like NAT traversal, encryption, and DHT-based routing.