#include <ss_p2p/kademlia/k_observer.hpp>
#include <ss_p2p/kademlia/k_message.hpp>

#include <random>
#include <sstream>
#include <iostream>

namespace ss::kademlia {

// k_observer implementation

k_observer::k_observer(boost::asio::io_context& io_ctx, const std::string& t_name)
    : base_observer(io_ctx, t_name) {
}

// ping implementation

ping::ping(boost::asio::io_context& io_ctx, 
           boost::asio::ip::udp::endpoint ep,
           on_pong_handler pong_handler, 
           on_timeout_handler timeout_handler)
    : k_observer(io_ctx, "ping")
    , is_pong_arrived_(false)
    , timer_(std::make_unique<boost::asio::steady_timer>(io_ctx))
    , dest_ep_(std::move(ep))
    , pong_handler_(std::move(pong_handler))
    , timeout_handler_(std::move(timeout_handler))
    , io_ctx_(io_ctx) {
}

int ping::income_message(message& msg, boost::asio::ip::udp::endpoint& ep) {
    // Check if this is a pong response for our ping
    try {
        auto json_data = nlohmann::json::parse(msg.get_body());
        
        // Verify it's a pong message
        if (json_data.contains("type") && json_data["type"] == "pong") {
            // Verify it's from our target endpoint
            if (ep == dest_ep_) {
                is_pong_arrived_ = true;
                
                // Cancel timeout timer
                if (timer_) {
                    timer_->cancel();
                }
                
                // Call success handler
                if (pong_handler_) {
                    pong_handler_(ep);
                }
                
                return 0; // Success
            }
        }
    } catch (const std::exception&) {
        // Invalid message format - ignore
    }
    
    return -1; // Not handled
}

void ping::timeout(const boost::system::error_code& ec) {
    if (ec == boost::asio::error::operation_aborted) {
        return; // Timer was cancelled (pong arrived)
    }
    
    if (!is_pong_arrived_) {
        // Call timeout handler
        if (timeout_handler_) {
            timeout_handler_(dest_ep_);
        }
    }
}

void ping::init() {
    // Set up timeout timer
    if (timer_) {
        timer_->expires_after(std::chrono::seconds{DEFAULT_PING_RESPONSE_TIMEOUT_s});
        timer_->async_wait([this](const boost::system::error_code& ec) {
            timeout(ec);
            });
    }
}

void ping::print() const {
    std::cout << "ping_observer{target=" << dest_ep_.address().to_string() 
              << ":" << dest_ep_.port()
              << ", pong_arrived=" << is_pong_arrived_ << "}\n";
}

// find_node implementation

find_node::find_node(boost::asio::io_context& io_ctx, on_response_handler response_handler)
    : k_observer(io_ctx, "find_node")
    , response_handler_(std::move(response_handler)) {
}

void find_node::init() {
    // Set up any initialization needed for find_node observer
    // In the legacy system, this might set up timers or other state
}

int find_node::income_message(message& msg, boost::asio::ip::udp::endpoint& ep) {
    // Check if this is a find_node response
    try {
        auto json_data = nlohmann::json::parse(msg.get_body());
        
        // Verify it's a find_node response
        if (json_data.contains("type") && json_data["type"] == "find_node_response") {
            // Extract nodes from response
            if (json_data.contains("nodes")) {
                for (const auto& node_json : json_data["nodes"]) {
                    try {
                        // Parse node endpoint
                        std::string endpoint_str = node_json["endpoint"];
                        auto colon_pos = endpoint_str.find(':');
                        
                        if (colon_pos != std::string::npos) {
                            auto ip = endpoint_str.substr(0, colon_pos);
                            auto port_str = endpoint_str.substr(colon_pos + 1);
                            auto port = static_cast<std::uint16_t>(std::stoul(port_str));
                            
                            boost::asio::ip::address addr = boost::asio::ip::make_address(ip);
                            boost::asio::ip::udp::endpoint node_ep{addr, port};
                            
                            // Call response handler for each found node
                            if (response_handler_) {
                                response_handler_(node_ep);
                            }
                        }
                    } catch (const std::exception&) {
                        // Skip invalid node entries
                        continue;
                    }
                }
            }
            
            return 0; // Success
        }
    } catch (const std::exception&) {
        // Invalid message format - ignore
    }
    
    return -1; // Not handled
}

void find_node::print() const {
    std::cout << "find_node_observer{}\n";
}

// Utility functions

std::string generate_h_id() {
    // Generate a unique handler ID using random UUID-like string
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::ostringstream oss;
    oss << std::hex;
    
    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) {
            oss << '-';
        }
        oss << dis(gen);
    }
    
    return oss.str();
}

} // namespace ss::kademlia