#include "ss_p2p/core/types.hpp"

#include <random>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <atomic>
#include <boost/asio.hpp>

namespace ss::core {

// node_id implementation

std::string node_id::to_hex() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (const auto& byte : data_) {
        oss << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return oss.str();
}

node_id node_id::from_hex(const std::string& hex_str) {
    if (hex_str.length() != SIZE * 2) {
        throw std::invalid_argument("Invalid hex string length. Expected " + 
                                   std::to_string(SIZE * 2) + " characters, got " + 
                                   std::to_string(hex_str.length()));
    }

    node_id result;
    for (size_t i = 0; i < SIZE; ++i) {
        const std::string byte_str = hex_str.substr(i * 2, 2);
        try {
            result.data_[i] = static_cast<std::uint8_t>(std::stoul(byte_str, nullptr, 16));
        } catch (const std::exception& e) {
            throw std::invalid_argument("Invalid hex character in string: " + hex_str);
        }
    }
    return result;
}

node_id node_id::random() {
    // Thread-local random number generator for better performance
    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());
    thread_local std::uniform_int_distribution<std::uint8_t> dis(0, 255);

    node_id result;
    for (auto& byte : result.data_) {
        byte = dis(gen);
    }
    return result;
}

// message_id implementation

message_id message_id::generate() {
    // Atomic counter for unique message IDs
    static std::atomic<value_type> counter{1};
    return message_id{counter.fetch_add(1, std::memory_order_relaxed)};
}

// endpoint implementation

endpoint::endpoint(const std::string& addr, std::uint16_t port) {
    try {
        boost::asio::ip::address address = boost::asio::ip::make_address(addr);
        endpoint_ = native_type(address, port);
    } catch (const std::exception& e) {
        throw std::invalid_argument("Invalid address: " + addr + " - " + e.what());
    }
}

std::string endpoint::to_string() const {
    if (!is_valid()) {
        return "invalid_endpoint";
    }
    
    std::ostringstream oss;
    if (endpoint_.address().is_v6()) {
        oss << "[" << endpoint_.address().to_string() << "]:" << endpoint_.port();
    } else {
        oss << endpoint_.address().to_string() << ":" << endpoint_.port();
    }
    return oss.str();
}

} // namespace ss::core