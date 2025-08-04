#pragma once

#include <concepts>
#include <type_traits>
#include <string>
#include <vector>
#include <memory>
#include <boost/asio.hpp>

namespace ss::core {

// Forward declarations
template<typename T, typename E>
class result;

/**
 * @brief Concept for types that can be serialized to/from byte sequences
 */
template<typename T>
concept Serializable = requires(const T& t, const std::vector<std::uint8_t>& data) {
    // Must be able to serialize to bytes
    { t.serialize() } -> std::convertible_to<std::vector<std::uint8_t>>;
    
    // Must be able to deserialize from bytes
    { T::deserialize(data) } -> std::same_as<T>;
} || requires(const T& t, std::string& str, const std::string& input) {
    // Alternative: string-based serialization
    { t.to_string() } -> std::convertible_to<std::string>;
    { T::from_string(input) } -> std::same_as<T>;
};

/**
 * @brief Concept for types that can be converted to/from JSON
 */
template<typename T>
concept JsonSerializable = requires(const T& t, const std::string& json_str) {
    { t.to_json() } -> std::convertible_to<std::string>;
    { T::from_json(json_str) } -> std::same_as<T>;
};

/**
 * @brief Concept for hashable types (can be used in hash maps)
 */
template<typename T>
concept Hashable = requires(const T& t) {
    { std::hash<T>{}(t) } -> std::convertible_to<std::size_t>;
} && std::equality_comparable<T>;

/**
 * @brief Concept for comparable types (can be ordered)
 */
template<typename T>
concept Comparable = std::totally_ordered<T>;

/**
 * @brief Concept for network endpoint types
 */
template<typename T>
concept NetworkEndpoint = requires(const T& ep) {
    // Must have address and port
    { ep.address() } -> std::convertible_to<std::string>;
    { ep.port() } -> std::convertible_to<std::uint16_t>;
    
    // Must be convertible to string
    { ep.to_string() } -> std::convertible_to<std::string>;
    
    // Must be equality comparable
    { ep == ep } -> std::convertible_to<bool>;
    { ep != ep } -> std::convertible_to<bool>;
    
    // Must have validity check
    { ep.is_valid() } -> std::convertible_to<bool>;
} && Hashable<T> && Comparable<T>;

/**
 * @brief Concept for message types in the P2P system
 */
template<typename T>
concept Message = requires(const T& msg) {
    // Must have message type identifier
    { msg.type() } -> std::convertible_to<std::string>;
    
    // Must be serializable
    requires Serializable<T>;
    
    // Must have timestamp
    { msg.timestamp() } -> std::convertible_to<std::uint64_t>;
} && std::move_constructible<T> && std::copy_constructible<T>;

/**
 * @brief Concept for identifier types (node_id, peer_id, message_id, etc.)
 */
template<typename T>
concept Identifier = requires(const T& id) {
    // Must be hashable and comparable
    requires Hashable<T>;
    requires Comparable<T>;
    
    // Must be copyable and movable
    requires std::copy_constructible<T>;
    requires std::move_constructible<T>;
    requires std::is_copy_assignable_v<T>;
    requires std::is_move_assignable_v<T>;
    
    // Must have string representation
    { id.to_string() } -> std::convertible_to<std::string>;
} || requires(const T& id) {
    // Alternative: simpler identifier
    requires std::integral<T> || std::is_enum_v<T>;
    requires Hashable<T>;
    requires Comparable<T>;
};

/**
 * @brief Concept for DHT node types
 */
template<typename T>
concept DHTNode = requires(const T& node) {
    // Must have node identifier
    typename T::id_type;
    { node.id() } -> std::convertible_to<typename T::id_type>;
    requires Identifier<typename T::id_type>;
    
    // Must have endpoint
    typename T::endpoint_type;
    { node.endpoint() } -> std::convertible_to<typename T::endpoint_type>;
    requires NetworkEndpoint<typename T::endpoint_type>;
    
    // Must track last seen time
    { node.last_seen() } -> std::convertible_to<std::uint64_t>;
    
    // Must be updatable
    { node.update_last_seen() } -> std::same_as<void>;
} && std::move_constructible<T> && std::copy_constructible<T>;

/**
 * @brief Concept for distance calculation in DHT
 */
template<typename T>
concept DistanceCalculable = requires(const T& a, const T& b) {
    { a.distance(b) } -> std::same_as<T>;
    requires Comparable<T>;
    requires Identifier<T>;
};

/**
 * @brief Concept for async operations that return results
 */
template<typename T>
concept AsyncResult = requires {
    typename T::value_type;
    typename T::error_type;
} && requires(T&& t) {
    // Must be awaitable
    { t } -> std::convertible_to<boost::asio::awaitable<typename T::value_type>>;
};

/**
 * @brief Concept for result types
 */
template<typename T>
concept ResultType = requires(const T& r) {
    // Must have value and error types
    typename T::value_type;
    typename T::error_type;
    
    // Must be checkable
    { r.is_ok() } -> std::convertible_to<bool>;
    { r.is_err() } -> std::convertible_to<bool>;
    
    // Must be convertible to bool
    { static_cast<bool>(r) } -> std::same_as<bool>;
} && requires(T&& r) {
    // Must support monadic operations
    { std::move(r).map(std::declval<std::function<int(typename T::value_type)>>()) };
    { std::move(r).and_then(std::declval<std::function<T(typename T::value_type)>>()) };
};

/**
 * @brief Concept for lifecycle manageable components
 */
template<typename T>
concept LifecycleManaged = requires(T& obj) {
    // Must support async start/stop
    { obj.start() } -> std::convertible_to<boost::asio::awaitable<void>>;
    { obj.stop() } -> std::convertible_to<boost::asio::awaitable<void>>;
    
    // Must track running state
    { obj.is_running() } -> std::convertible_to<bool>;
};

/**
 * @brief Concept for component types in the system
 */
template<typename T>
concept Component = LifecycleManaged<T> && requires(const T& comp) {
    // Must have component name
    { comp.name() } -> std::convertible_to<std::string>;
    
    // Must be move-constructible
    requires std::move_constructible<T>;
};

/**
 * @brief Concept for observer pattern participants
 */
template<typename T>
concept Observable = requires(T& obj) {
    // Event type
    typename T::event_type;
    
    // Must support adding/removing observers
    { obj.add_observer(std::declval<std::function<void(const typename T::event_type&)>>()) } -> std::convertible_to<std::size_t>;
    { obj.remove_observer(std::declval<std::size_t>()) } -> std::convertible_to<bool>;
    
    // Must support notifying observers
    { obj.notify(std::declval<typename T::event_type>()) } -> std::same_as<void>;
};

/**
 * @brief Concept for socket-like types
 */
template<typename T>
concept Socket = requires(T& sock) {
    // Must have endpoint type
    typename T::endpoint_type;
    requires NetworkEndpoint<typename T::endpoint_type>;
    
    // Must support async operations
    { sock.async_send_to(std::declval<const std::vector<std::uint8_t>&>(), 
                        std::declval<typename T::endpoint_type>(), 
                        std::declval<std::function<void(std::error_code, std::size_t)>>()) };
    
    { sock.async_receive_from(std::declval<std::vector<std::uint8_t>&>(), 
                             std::declval<typename T::endpoint_type&>(), 
                             std::declval<std::function<void(std::error_code, std::size_t)>>()) };
    
    // Must support binding
    { sock.bind(std::declval<typename T::endpoint_type>()) } -> std::convertible_to<std::error_code>;
    
    // Must be closeable
    { sock.close() } -> std::same_as<void>;
    
    // Must track open state
    { sock.is_open() } -> std::convertible_to<bool>;
};

/**
 * @brief Concept for message handlers
 */
template<typename T, typename MessageType>
concept MessageHandler = requires(T& handler, const MessageType& msg) {
    // Must be able to handle message
    { handler.handle(msg) } -> std::convertible_to<boost::asio::awaitable<void>>;
    
    // Must specify supported message types
    { handler.can_handle(msg) } -> std::convertible_to<bool>;
} && Message<MessageType>;

/**
 * @brief Concept for routing table types
 */
template<typename T>
concept RoutingTable = requires(T& table) {
    // Node and distance types
    typename T::node_type;
    typename T::distance_type;
    requires DHTNode<typename T::node_type>;
    requires DistanceCalculable<typename T::distance_type>;
    
    // Must support finding nodes
    { table.find_closest(std::declval<typename T::distance_type>(), std::declval<std::size_t>()) } 
        -> std::convertible_to<std::vector<typename T::node_type>>;
    
    // Must support adding/removing nodes
    { table.add_node(std::declval<typename T::node_type>()) } -> std::convertible_to<bool>;
    { table.remove_node(std::declval<typename T::distance_type>()) } -> std::convertible_to<bool>;
    
    // Must support updating nodes
    { table.update_node(std::declval<typename T::node_type>()) } -> std::convertible_to<bool>;
    
    // Must provide size information
    { table.size() } -> std::convertible_to<std::size_t>;
    { table.empty() } -> std::convertible_to<bool>;
};

/**
 * @brief Concept for timeout manageable operations
 */
template<typename T>
concept TimeoutManaged = requires(T& obj) {
    // Must support timeout operations
    { obj.set_timeout(std::declval<std::chrono::milliseconds>()) } -> std::same_as<void>;
    { obj.get_timeout() } -> std::convertible_to<std::chrono::milliseconds>;
    { obj.is_timed_out() } -> std::convertible_to<bool>;
};

/**
 * @brief Concept for cryptographic key types
 */
template<typename T>
concept CryptoKey = requires(const T& key) {
    // Must have key data
    { key.data() } -> std::convertible_to<const std::uint8_t*>;
    { key.size() } -> std::convertible_to<std::size_t>;
    
    // Must be serializable
    requires Serializable<T>;
    
    // Must support key derivation or generation
    { T::generate() } -> std::same_as<T>;
} && std::move_constructible<T> && std::copy_constructible<T>;

/**
 * @brief Concept for thread-safe types
 */
template<typename T>
concept ThreadSafe = requires {
    // Must be documented as thread-safe or have obvious synchronization
    requires std::is_trivially_copyable_v<T> || requires(T& obj) {
        // Has mutex or atomic operations
        typename T::mutex_type;
    };
};

} // namespace ss::core