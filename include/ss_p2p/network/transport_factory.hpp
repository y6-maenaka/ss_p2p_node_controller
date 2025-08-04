#pragma once

#include "i_transport.hpp"
#include "i_protocol.hpp"
#include "../core/types.hpp"
#include "../core/result.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <type_traits>

namespace ss::network {

/**
 * @brief Transport type enumeration
 */
enum class transport_type {
    /// UDP transport (connectionless)
    udp,
    /// TCP transport (connection-oriented)
    tcp,
    /// QUIC transport (connection-oriented with built-in encryption)
    quic,
    /// WebSocket transport
    websocket,
    /// Custom transport type
    custom
};

} // namespace ss::network

// Hash specialization must be in std namespace
namespace std {
    template<>
    struct hash<ss::network::transport_type> {
        std::size_t operator()(ss::network::transport_type type) const noexcept {
            return std::hash<std::underlying_type_t<ss::network::transport_type>>{}(
                static_cast<std::underlying_type_t<ss::network::transport_type>>(type));
        }
    };
} // namespace std

namespace ss::network {

/**
 * @brief Convert transport_type to string representation
 * @param type Transport type to convert
 * @return String name of the transport type
 */
std::string to_string(transport_type type) noexcept;

/**
 * @brief Parse transport type from string
 * @param type_str String representation of transport type
 * @return Parsed transport type or error
 */
ss::core::result<transport_type, std::string> transport_type_from_string(const std::string& type_str) noexcept;

/**
 * @brief Factory error codes
 */
enum class factory_error {
    /// No error occurred
    none = 0,
    /// Unsupported transport type
    unsupported_transport,
    /// Invalid configuration
    invalid_config,
    /// Missing required dependency
    missing_dependency,
    /// Initialization failed
    initialization_failed,
    /// Transport already registered
    already_registered,
    /// Transport not found
    not_found,
    /// Out of memory
    out_of_memory,
    /// Unknown error
    unknown
};

/**
 * @brief Convert factory_error to string representation
 * @param error Error code to convert
 * @return String description of the error
 */
std::string to_string(factory_error error) noexcept;

/**
 * @brief Transport creation parameters
 */
struct transport_params {
    /// IO context for async operations
    ss::core::io_context* io_context = nullptr;
    /// Transport configuration
    transport_config config;
    /// Custom properties for transport-specific parameters
    std::unordered_map<std::string, std::string> properties;

    /**
     * @brief Set a property value
     * @param key Property key
     * @param value Property value
     */
    void set_property(const std::string& key, const std::string& value) {
        properties[key] = value;
    }

    /**
     * @brief Get a property value
     * @param key Property key
     * @param default_value Default value if key not found
     * @return Property value or default
     */
    std::string get_property(const std::string& key, const std::string& default_value = "") const {
        auto it = properties.find(key);
        return it != properties.end() ? it->second : default_value;
    }

    /**
     * @brief Check if property exists
     * @param key Property key
     * @return true if property exists, false otherwise
     */
    bool has_property(const std::string& key) const {
        return properties.find(key) != properties.end();
    }
};

/**
 * @brief Protocol creation parameters
 */
struct protocol_params {
    /// Protocol configuration
    protocol_config config;
    /// Custom properties for protocol-specific parameters
    std::unordered_map<std::string, std::string> properties;

    /**
     * @brief Set a property value
     * @param key Property key
     * @param value Property value
     */
    void set_property(const std::string& key, const std::string& value) {
        properties[key] = value;
    }

    /**
     * @brief Get a property value
     * @param key Property key
     * @param default_value Default value if key not found
     * @return Property value or default
     */
    std::string get_property(const std::string& key, const std::string& default_value = "") const {
        auto it = properties.find(key);
        return it != properties.end() ? it->second : default_value;
    }

    /**
     * @brief Check if property exists
     * @param key Property key
     * @return true if property exists, false otherwise
     */
    bool has_property(const std::string& key) const {
        return properties.find(key) != properties.end();
    }
};

/**
 * @brief Transport factory function type
 */
using transport_factory_func = std::function<ss::core::result<transport_ptr, factory_error>(const transport_params&)>;

/**
 * @brief Protocol factory function type
 */
using protocol_factory_func = std::function<ss::core::result<protocol_ptr, factory_error>(const protocol_params&)>;

/**
 * @brief Transport information structure
 */
struct transport_info {
    /// Transport type
    transport_type type;
    /// Human-readable name
    std::string name;
    /// Description of the transport
    std::string description;
    /// Supported features
    std::vector<std::string> features;
    /// Whether transport is connection-based
    bool connection_based = false;
    /// Whether transport supports encryption
    bool supports_encryption = false;
    /// Whether transport supports fragmentation
    bool supports_fragmentation = true;
    /// Maximum message size (0 = unlimited)
    std::uint32_t max_message_size = 0;
};

/**
 * @brief Protocol information structure
 */
struct protocol_info {
    /// Protocol name
    std::string name;
    /// Protocol description
    std::string description;
    /// Supported versions
    std::vector<protocol_version> supported_versions;
    /// Supported features
    std::vector<std::string> features;
    /// Whether protocol supports compression
    bool supports_compression = false;
    /// Whether protocol supports encryption
    bool supports_encryption = false;
    /// Whether protocol supports fragmentation
    bool supports_fragmentation = true;
};

/**
 * @brief Factory for creating transport and protocol instances
 * 
 * Provides a centralized mechanism for creating different types of transport
 * and protocol implementations with proper configuration and dependency injection.
 */
class transport_factory {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to factory instance
     */
    static transport_factory& instance();

    /**
     * @brief Register a transport factory function
     * @param type Transport type
     * @param info Transport information
     * @param factory Factory function
     * @return Result indicating success or error
     */
    ss::core::result<void, factory_error> 
    register_transport(transport_type type, 
                      const transport_info& info,
                      transport_factory_func factory);

    /**
     * @brief Register a protocol factory function
     * @param name Protocol name
     * @param info Protocol information
     * @param factory Factory function
     * @return Result indicating success or error
     */
    ss::core::result<void, factory_error>
    register_protocol(const std::string& name,
                     const protocol_info& info,
                     protocol_factory_func factory);

    /**
     * @brief Unregister a transport type
     * @param type Transport type to unregister
     * @return Result indicating success or error
     */
    ss::core::result<void, factory_error> unregister_transport(transport_type type);

    /**
     * @brief Unregister a protocol
     * @param name Protocol name to unregister
     * @return Result indicating success or error
     */
    ss::core::result<void, factory_error> unregister_protocol(const std::string& name);

    /**
     * @brief Create a transport instance
     * @param type Transport type
     * @param params Creation parameters
     * @return Transport instance or error
     */
    ss::core::result<transport_ptr, factory_error>
    create_transport(transport_type type, const transport_params& params);

    /**
     * @brief Create a protocol instance
     * @param name Protocol name
     * @param params Creation parameters
     * @return Protocol instance or error
     */
    ss::core::result<protocol_ptr, factory_error>
    create_protocol(const std::string& name, const protocol_params& params);

    /**
     * @brief Create transport from configuration string
     * @param config_str Configuration string (e.g., "udp://0.0.0.0:8080")
     * @param io_context IO context for async operations
     * @return Transport instance or error
     */
    ss::core::result<transport_ptr, factory_error>
    create_transport_from_string(const std::string& config_str, ss::core::io_context& io_context);

    /**
     * @brief Create protocol from configuration string
     * @param config_str Configuration string (e.g., "json-rpc:1.0")
     * @return Protocol instance or error
     */
    ss::core::result<protocol_ptr, factory_error>
    create_protocol_from_string(const std::string& config_str);

    /**
     * @brief Get information about a registered transport
     * @param type Transport type
     * @return Transport information or error
     */
    ss::core::result<transport_info, factory_error> get_transport_info(transport_type type) const;

    /**
     * @brief Get information about a registered protocol
     * @param name Protocol name
     * @return Protocol information or error
     */
    ss::core::result<protocol_info, factory_error> get_protocol_info(const std::string& name) const;

    /**
     * @brief Get list of all registered transport types
     * @return Vector of registered transport types
     */
    std::vector<transport_type> get_registered_transports() const;

    /**
     * @brief Get list of all registered protocol names
     * @return Vector of registered protocol names
     */
    std::vector<std::string> get_registered_protocols() const;

    /**
     * @brief Check if transport type is registered
     * @param type Transport type to check
     * @return true if registered, false otherwise
     */
    bool is_transport_registered(transport_type type) const;

    /**
     * @brief Check if protocol is registered
     * @param name Protocol name to check
     * @return true if registered, false otherwise
     */
    bool is_protocol_registered(const std::string& name) const;

    /**
     * @brief Register default transport types
     * @param io_context IO context for transport creation
     * @return Result indicating success or error
     */
    ss::core::result<void, factory_error> register_default_transports(ss::core::io_context& io_context);

    /**
     * @brief Register default protocol types
     * @return Result indicating success or error
     */
    ss::core::result<void, factory_error> register_default_protocols();

    /**
     * @brief Clear all registered factories
     */
    void clear();

private:
    /**
     * @brief Transport registration entry
     */
    struct transport_entry {
        transport_info info;
        transport_factory_func factory;
    };

    /**
     * @brief Protocol registration entry
     */
    struct protocol_entry {
        protocol_info info;
        protocol_factory_func factory;
    };

    mutable std::mutex mutex_;
    std::unordered_map<transport_type, transport_entry> transport_registry_;
    std::unordered_map<std::string, protocol_entry> protocol_registry_;

    /**
     * @brief Private constructor for singleton
     */
    transport_factory() = default;

    /**
     * @brief Parse transport URL
     * @param url URL string (e.g., "udp://0.0.0.0:8080")
     * @return Parsed components or error
     */
    struct url_components {
        transport_type type;
        std::string host;
        std::uint16_t port;
        std::unordered_map<std::string, std::string> parameters;
    };

    ss::core::result<url_components, factory_error> parse_transport_url(const std::string& url) const;

    /**
     * @brief Parse protocol specification
     * @param spec Protocol specification (e.g., "json-rpc:1.0")
     * @return Parsed components or error
     */
    struct protocol_spec {
        std::string name;
        protocol_version version;
        std::unordered_map<std::string, std::string> parameters;
    };

    ss::core::result<protocol_spec, factory_error> parse_protocol_spec(const std::string& spec) const;
};

/**
 * @brief RAII helper for automatic transport registration
 * 
 * Allows automatic registration and cleanup of transport types
 * using RAII principles.
 */
template<transport_type Type>
class transport_registrar {
public:
    /**
     * @brief Constructor with automatic registration
     * @param info Transport information
     * @param factory Factory function
     */
    transport_registrar(const transport_info& info, transport_factory_func factory)
        : registered_(false) {
        auto result = transport_factory::instance().register_transport(Type, info, std::move(factory));
        registered_ = result.is_ok();
    }

    /**
     * @brief Destructor with automatic cleanup
     */
    ~transport_registrar() {
        if (registered_) {
            transport_factory::instance().unregister_transport(Type);
        }
    }

    /**
     * @brief Copy constructor (deleted)
     */
    transport_registrar(const transport_registrar&) = delete;

    /**
     * @brief Copy assignment (deleted)
     */
    transport_registrar& operator=(const transport_registrar&) = delete;

    /**
     * @brief Move constructor
     */
    transport_registrar(transport_registrar&& other) noexcept
        : registered_(other.registered_) {
        other.registered_ = false;
    }

    /**
     * @brief Move assignment
     */
    transport_registrar& operator=(transport_registrar&& other) noexcept {
        if (this != &other) {
            if (registered_) {
                transport_factory::instance().unregister_transport(Type);
            }
            registered_ = other.registered_;
            other.registered_ = false;
        }
        return *this;
    }

    /**
     * @brief Check if registration was successful
     * @return true if registered, false otherwise
     */
    bool is_registered() const noexcept { return registered_; }

private:
    bool registered_;
};

/**
 * @brief RAII helper for automatic protocol registration
 */
class protocol_registrar {
public:
    /**
     * @brief Constructor with automatic registration
     * @param name Protocol name
     * @param info Protocol information
     * @param factory Factory function
     */
    protocol_registrar(const std::string& name, 
                      const protocol_info& info, 
                      protocol_factory_func factory)
        : name_(name), registered_(false) {
        auto result = transport_factory::instance().register_protocol(name, info, std::move(factory));
        registered_ = result.is_ok();
    }

    /**
     * @brief Destructor with automatic cleanup
     */
    ~protocol_registrar() {
        if (registered_) {
            transport_factory::instance().unregister_protocol(name_);
        }
    }

    /**
     * @brief Copy constructor (deleted)
     */
    protocol_registrar(const protocol_registrar&) = delete;

    /**
     * @brief Copy assignment (deleted)
     */
    protocol_registrar& operator=(const protocol_registrar&) = delete;

    /**
     * @brief Move constructor
     */
    protocol_registrar(protocol_registrar&& other) noexcept
        : name_(std::move(other.name_)), registered_(other.registered_) {
        other.registered_ = false;
    }

    /**
     * @brief Move assignment
     */
    protocol_registrar& operator=(protocol_registrar&& other) noexcept {
        if (this != &other) {
            if (registered_) {
                transport_factory::instance().unregister_protocol(name_);
            }
            name_ = std::move(other.name_);
            registered_ = other.registered_;
            other.registered_ = false;
        }
        return *this;
    }

    /**
     * @brief Check if registration was successful
     * @return true if registered, false otherwise
     */
    bool is_registered() const noexcept { return registered_; }

private:
    std::string name_;
    bool registered_;
};

/**
 * @brief Convenience function to create UDP transport
 * @param io_context IO context for async operations
 * @param config Transport configuration
 * @return UDP transport instance or error
 */
ss::core::result<transport_ptr, factory_error>
create_udp_transport(ss::core::io_context& io_context, 
                    const transport_config& config = {});

/**
 * @brief Convenience function to create TCP transport
 * @param io_context IO context for async operations
 * @param config Transport configuration
 * @return TCP transport instance or error
 */
ss::core::result<transport_ptr, factory_error>
create_tcp_transport(ss::core::io_context& io_context,
                    const transport_config& config = {});

/**
 * @brief Convenience function to create default protocol
 * @param config Protocol configuration
 * @return Protocol instance or error
 */
ss::core::result<protocol_ptr, factory_error>
create_default_protocol(const protocol_config& config = {});

} // namespace ss::network

