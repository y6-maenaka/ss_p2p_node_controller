#include "../../../include/ss_p2p/network/i_transport.hpp"
#include "../../../include/ss_p2p/network/i_protocol.hpp"
#include "../../../include/ss_p2p/network/message_buffer.hpp"
#include "../../../include/ss_p2p/network/transport_factory.hpp"

namespace ss::network {

std::string to_string(transport_error error) noexcept {
    switch (error) {
        case transport_error::none:
            return "No error";
        case transport_error::network_failure:
            return "Network I/O operation failed";
        case transport_error::connection_refused:
            return "Connection was refused by remote host";
        case transport_error::timeout:
            return "Operation timed out";
        case transport_error::host_unreachable:
            return "Host is unreachable";
        case transport_error::port_unreachable:
            return "Port is unreachable";
        case transport_error::connection_reset:
            return "Connection was reset by peer";
        case transport_error::connection_aborted:
            return "Connection was aborted locally";
        case transport_error::no_route_to_host:
            return "No route to host";
        case transport_error::address_in_use:
            return "Address already in use";
        case transport_error::address_not_available:
            return "Address not available";
        case transport_error::message_too_large:
            return "Message too large for transport";
        case transport_error::not_connected:
            return "Transport is not connected";
        case transport_error::already_connected:
            return "Transport is already connected";
        case transport_error::invalid_argument:
            return "Invalid argument provided";
        case transport_error::permission_denied:
            return "Permission denied";
        case transport_error::out_of_memory:
            return "Out of memory";
        case transport_error::shutdown_in_progress:
            return "Transport is shutting down";
        case transport_error::unknown:
        default:
            return "Unknown error";
    }
}

std::string to_string(protocol_error error) noexcept {
    switch (error) {
        case protocol_error::none:
            return "No error";
        case protocol_error::invalid_format:
            return "Invalid message format";
        case protocol_error::unsupported_version:
            return "Unsupported protocol version";
        case protocol_error::message_too_large:
            return "Message too large for protocol";
        case protocol_error::message_too_small:
            return "Message too small for protocol";
        case protocol_error::invalid_message_type:
            return "Invalid message type";
        case protocol_error::missing_required_fields:
            return "Missing required fields";
        case protocol_error::invalid_field_value:
            return "Invalid field value";
        case protocol_error::checksum_failure:
            return "Checksum verification failed";
        case protocol_error::compression_error:
            return "Compression/decompression error";
        case protocol_error::encryption_error:
            return "Encryption/decryption error";
        case protocol_error::protocol_state_error:
            return "Protocol state violation";
        case protocol_error::unknown:
        default:
            return "Unknown error";
    }
}

std::string to_string(buffer_error error) noexcept {
    switch (error) {
        case buffer_error::none:
            return "No error";
        case buffer_error::buffer_full:
            return "Buffer is full";
        case buffer_error::buffer_empty:
            return "Buffer is empty";
        case buffer_error::invalid_size:
            return "Invalid size requested";
        case buffer_error::out_of_memory:
            return "Out of memory";
        case buffer_error::corruption:
            return "Buffer is corrupted";
        case buffer_error::would_block:
            return "Operation would block";
        case buffer_error::timeout:
            return "Operation timed out";
        case buffer_error::invalid_alignment:
            return "Invalid alignment";
        case buffer_error::unknown:
        default:
            return "Unknown error";
    }
}

std::string to_string(factory_error error) noexcept {
    switch (error) {
        case factory_error::none:
            return "No error";
        case factory_error::unsupported_transport:
            return "Unsupported transport type";
        case factory_error::invalid_config:
            return "Invalid configuration";
        case factory_error::missing_dependency:
            return "Missing required dependency";
        case factory_error::initialization_failed:
            return "Initialization failed";
        case factory_error::already_registered:
            return "Transport already registered";
        case factory_error::not_found:
            return "Transport not found";
        case factory_error::out_of_memory:
            return "Out of memory";
        case factory_error::unknown:
        default:
            return "Unknown error";
    }
}

std::string to_string(transport_type type) noexcept {
    switch (type) {
        case transport_type::udp:
            return "udp";
        case transport_type::tcp:
            return "tcp";
        case transport_type::quic:
            return "quic";
        case transport_type::websocket:
            return "websocket";
        case transport_type::custom:
            return "custom";
        default:
            return "unknown";
    }
}

ss::core::result<transport_type, std::string> transport_type_from_string(const std::string& type_str) noexcept {
    if (type_str == "udp") {
        return ss::core::result<transport_type, std::string>::ok(transport_type::udp);
    } else if (type_str == "tcp") {
        return ss::core::result<transport_type, std::string>::ok(transport_type::tcp);
    } else if (type_str == "quic") {
        return ss::core::result<transport_type, std::string>::ok(transport_type::quic);
    } else if (type_str == "websocket") {
        return ss::core::result<transport_type, std::string>::ok(transport_type::websocket);
    } else if (type_str == "custom") {
        return ss::core::result<transport_type, std::string>::ok(transport_type::custom);
    } else {
        return ss::core::result<transport_type, std::string>::err("Unknown transport type: " + type_str);
    }
}

} // namespace ss::network