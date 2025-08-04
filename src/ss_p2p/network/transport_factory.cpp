#include "../../../include/ss_p2p/network/transport_factory.hpp"
#include "../../../include/ss_p2p/network/impl/udp_transport.hpp"
#include "../../../include/ss_p2p/network/impl/tcp_transport.hpp"

#include <regex>
#include <sstream>

namespace ss::network {

transport_factory& transport_factory::instance() {
    static transport_factory factory;
    return factory;
}

ss::core::result<void, factory_error> 
transport_factory::register_transport(transport_type type, 
                                     const transport_info& info,
                                     transport_factory_func factory) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = transport_registry_.find(type);
    if (it != transport_registry_.end()) {
        return ss::core::result<void, factory_error>::err(factory_error::already_registered);
    }
    
    transport_registry_[type] = transport_entry{info, std::move(factory)};
    return ss::core::result<void, factory_error>::ok();
}

ss::core::result<void, factory_error>
transport_factory::register_protocol(const std::string& name,
                                    const protocol_info& info,
                                    protocol_factory_func factory) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = protocol_registry_.find(name);
    if (it != protocol_registry_.end()) {
        return ss::core::result<void, factory_error>::err(factory_error::already_registered);
    }
    
    protocol_registry_[name] = protocol_entry{info, std::move(factory)};
    return ss::core::result<void, factory_error>::ok();
}

ss::core::result<void, factory_error> transport_factory::unregister_transport(transport_type type) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = transport_registry_.find(type);
    if (it == transport_registry_.end()) {
        return ss::core::result<void, factory_error>::err(factory_error::not_found);
    }
    
    transport_registry_.erase(it);
    return ss::core::result<void, factory_error>::ok();
}

ss::core::result<void, factory_error> transport_factory::unregister_protocol(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = protocol_registry_.find(name);
    if (it == protocol_registry_.end()) {
        return ss::core::result<void, factory_error>::err(factory_error::not_found);
    }
    
    protocol_registry_.erase(it);
    return ss::core::result<void, factory_error>::ok();
}

ss::core::result<transport_ptr, factory_error>
transport_factory::create_transport(transport_type type, const transport_params& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = transport_registry_.find(type);
    if (it == transport_registry_.end()) {
        return ss::core::result<transport_ptr, factory_error>::err(factory_error::unsupported_transport);
    }
    
    return it->second.factory(params);
}

ss::core::result<protocol_ptr, factory_error>
transport_factory::create_protocol(const std::string& name, const protocol_params& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = protocol_registry_.find(name);
    if (it == protocol_registry_.end()) {
        return ss::core::result<protocol_ptr, factory_error>::err(factory_error::not_found);
    }
    
    return it->second.factory(params);
}

ss::core::result<transport_ptr, factory_error>
transport_factory::create_transport_from_string(const std::string& config_str, ss::core::io_context& io_context) {
    auto url_result = parse_transport_url(config_str);
    if (!url_result.is_ok()) {
        return ss::core::result<transport_ptr, factory_error>::err(url_result.error());
    }
    
    auto components = url_result.value();
    
    transport_params params;
    params.io_context = &io_context;
    
    // Set configuration from URL parameters
    for (const auto& [key, value] : components.parameters) {
        params.set_property(key, value);
    }
    
    return create_transport(components.type, params);
}

ss::core::result<protocol_ptr, factory_error>
transport_factory::create_protocol_from_string(const std::string& config_str) {
    auto spec_result = parse_protocol_spec(config_str);
    if (!spec_result.is_ok()) {
        return ss::core::result<protocol_ptr, factory_error>::err(spec_result.error());
    }
    
    auto spec = spec_result.value();
    
    protocol_params params;
    params.config.version = spec.version;
    
    // Set configuration from spec parameters
    for (const auto& [key, value] : spec.parameters) {
        params.set_property(key, value);
    }
    
    return create_protocol(spec.name, params);
}

ss::core::result<transport_info, factory_error> transport_factory::get_transport_info(transport_type type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = transport_registry_.find(type);
    if (it == transport_registry_.end()) {
        return ss::core::result<transport_info, factory_error>::err(factory_error::not_found);
    }
    
    return ss::core::result<transport_info, factory_error>::ok(it->second.info);
}

ss::core::result<protocol_info, factory_error> transport_factory::get_protocol_info(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = protocol_registry_.find(name);
    if (it == protocol_registry_.end()) {
        return ss::core::result<protocol_info, factory_error>::err(factory_error::not_found);
    }
    
    return ss::core::result<protocol_info, factory_error>::ok(it->second.info);
}

std::vector<transport_type> transport_factory::get_registered_transports() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<transport_type> types;
    types.reserve(transport_registry_.size());
    
    for (const auto& [type, entry] : transport_registry_) {
        types.push_back(type);
    }
    
    return types;
}

std::vector<std::string> transport_factory::get_registered_protocols() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> names;
    names.reserve(protocol_registry_.size());
    
    for (const auto& [name, entry] : protocol_registry_) {
        names.push_back(name);
    }
    
    return names;
}

bool transport_factory::is_transport_registered(transport_type type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return transport_registry_.find(type) != transport_registry_.end();
}

bool transport_factory::is_protocol_registered(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return protocol_registry_.find(name) != protocol_registry_.end();
}

ss::core::result<void, factory_error> transport_factory::register_default_transports(ss::core::io_context& /*io_context*/) {
    // Register UDP transport
    transport_info udp_info;
    udp_info.type = transport_type::udp;
    udp_info.name = "UDP";
    udp_info.description = "User Datagram Protocol transport";
    udp_info.features = {"connectionless", "fast", "unreliable"};
    udp_info.connection_based = false;
    udp_info.supports_encryption = false;
    udp_info.supports_fragmentation = true;
    udp_info.max_message_size = 65507;  // UDP max payload
    
    auto udp_factory = [](const transport_params& params) -> ss::core::result<transport_ptr, factory_error> {
        try {
            auto transport = std::make_shared<impl::udp_transport>(*params.io_context, params.config);
            return ss::core::result<transport_ptr, factory_error>::ok(transport);
        } catch (const std::exception&) {
            return ss::core::result<transport_ptr, factory_error>::err(factory_error::initialization_failed);
        }
    };
    
    auto udp_result = register_transport(transport_type::udp, udp_info, udp_factory);
    if (!udp_result.is_ok()) {
        return udp_result;
    }
    
    // Register TCP transport
    transport_info tcp_info;
    tcp_info.type = transport_type::tcp;
    tcp_info.name = "TCP";
    tcp_info.description = "Transmission Control Protocol transport";
    tcp_info.features = {"connection-oriented", "reliable", "ordered"};
    tcp_info.connection_based = true;
    tcp_info.supports_encryption = false;
    tcp_info.supports_fragmentation = true;
    tcp_info.max_message_size = 0;  // No limit
    
    auto tcp_factory = [](const transport_params& params) -> ss::core::result<transport_ptr, factory_error> {
        try {
            auto transport = std::make_shared<impl::tcp_transport>(*params.io_context, params.config);
            return ss::core::result<transport_ptr, factory_error>::ok(transport);
        } catch (const std::exception&) {
            return ss::core::result<transport_ptr, factory_error>::err(factory_error::initialization_failed);
        }
    };
    
    auto tcp_result = register_transport(transport_type::tcp, tcp_info, tcp_factory);
    if (!tcp_result.is_ok()) {
        return tcp_result;
    }
    
    return ss::core::result<void, factory_error>::ok();
}

ss::core::result<void, factory_error> transport_factory::register_default_protocols() {
    // TODO: Implement default protocol registration when protocols are implemented
    return ss::core::result<void, factory_error>::ok();
}

void transport_factory::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    transport_registry_.clear();
    protocol_registry_.clear();
}

ss::core::result<transport_factory::url_components, factory_error> 
transport_factory::parse_transport_url(const std::string& url) const {
    // Parse URL format: protocol://host:port?param1=value1&param2=value2
    std::regex url_regex(R"(^(\w+)://([^:/?]+):(\d+)(?:\?(.*))?$)");
    std::smatch match;
    
    if (!std::regex_match(url, match, url_regex)) {
        return ss::core::result<url_components, factory_error>::err(factory_error::invalid_config);
    }
    
    url_components components;
    
    // Parse transport type
    auto type_result = transport_type_from_string(match[1].str());
    if (!type_result.is_ok()) {
        return ss::core::result<url_components, factory_error>::err(factory_error::unsupported_transport);
    }
    components.type = type_result.value();
    
    // Parse host and port
    components.host = match[2].str();
    try {
        auto port_val = std::stoul(match[3].str());
        if (port_val > 65535) {
            return ss::core::result<url_components, factory_error>::err(factory_error::invalid_config);
        }
        components.port = static_cast<std::uint16_t>(port_val);
    } catch (const std::exception&) {
        return ss::core::result<url_components, factory_error>::err(factory_error::invalid_config);
    }
    
    // Parse query parameters
    if (match[4].matched) {
        std::string query = match[4].str();
        std::regex param_regex(R"(([^&=]+)=([^&=]*))");
        std::sregex_iterator iter(query.begin(), query.end(), param_regex);
        std::sregex_iterator end;
        
        for (; iter != end; ++iter) {
            std::smatch param_match = *iter;
            components.parameters[param_match[1].str()] = param_match[2].str();
        }
    }
    
    return ss::core::result<url_components, factory_error>::ok(components);
}

ss::core::result<transport_factory::protocol_spec, factory_error> 
transport_factory::parse_protocol_spec(const std::string& spec) const {
    // Parse format: protocol_name:version?param1=value1&param2=value2
    std::regex spec_regex(R"(^([^:?]+)(?::([^?]+))?(?:\?(.*))?$)");
    std::smatch match;
    
    if (!std::regex_match(spec, match, spec_regex)) {
        return ss::core::result<protocol_spec, factory_error>::err(factory_error::invalid_config);
    }
    
    protocol_spec components;
    components.name = match[1].str();
    
    // Parse version
    if (match[2].matched) {
        auto version_result = protocol_version::from_string(match[2].str());
        if (!version_result.is_ok()) {
            return ss::core::result<protocol_spec, factory_error>::err(factory_error::invalid_config);
        }
        components.version = version_result.value();
    } else {
        components.version = protocol_version{1, 0, 0};  // Default version
    }
    
    // Parse query parameters
    if (match[3].matched) {
        std::string query = match[3].str();
        std::regex param_regex(R"(([^&=]+)=([^&=]*))");
        std::sregex_iterator iter(query.begin(), query.end(), param_regex);
        std::sregex_iterator end;
        
        for (; iter != end; ++iter) {
            std::smatch param_match = *iter;
            components.parameters[param_match[1].str()] = param_match[2].str();
        }
    }
    
    return ss::core::result<protocol_spec, factory_error>::ok(components);
}

// Convenience functions

ss::core::result<transport_ptr, factory_error>
create_udp_transport(ss::core::io_context& io_context, const transport_config& config) {
    transport_params params;
    params.io_context = &io_context;
    params.config = config;
    
    return transport_factory::instance().create_transport(transport_type::udp, params);
}

ss::core::result<transport_ptr, factory_error>
create_tcp_transport(ss::core::io_context& io_context, const transport_config& config) {
    transport_params params;
    params.io_context = &io_context;
    params.config = config;
    
    return transport_factory::instance().create_transport(transport_type::tcp, params);
}

ss::core::result<protocol_ptr, factory_error>
create_default_protocol(const protocol_config& config) {
    protocol_params params;
    params.config = config;
    
    // TODO: Create default protocol when implemented
    return ss::core::result<protocol_ptr, factory_error>::err(factory_error::not_found);
}

} // namespace ss::network