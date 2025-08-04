#include <iostream>
#include <chrono>
#include <memory>
#include <ss_p2p/message.hpp>

// Minimal test to validate basic message functionality
int main() {
    std::cout << "Testing minimal message functionality...\n";
    
    try {
        // Test 1: Basic message creation
        std::cout << "Test 1: Basic message creation\n";
        
        ss::message msg(ss::message_type::ping, ss::message_priority::normal);
        
        std::cout << "  Message ID: " << msg.id() << "\n";
        std::cout << "  Message type: " << static_cast<int>(msg.type()) << "\n";
        std::cout << "  Message priority: " << static_cast<int>(msg.priority()) << "\n";
        std::cout << "  Message size: " << msg.total_size() << " bytes\n";
        std::cout << "  Message is valid: " << (msg.is_valid() ? "Yes" : "No") << "\n";
        
        // Test 2: Message with payload
        std::cout << "\nTest 2: Message with JSON payload\n";
        
        nlohmann::json test_data = {
            {"test", "Hello, World!"},
            {"number", 42},
            {"array", {1, 2, 3}}
        };
        
        ss::message_payload payload(test_data);
        ss::message msg_with_payload(ss::message_type::user_data, std::move(payload));
        
        std::cout << "  Message with payload ID: " << msg_with_payload.id() << "\n";
        std::cout << "  Message type: " << static_cast<int>(msg_with_payload.type()) << "\n";
        std::cout << "  Message size: " << msg_with_payload.total_size() << " bytes\n";
        
        // Try to get payload back
        auto recovered_json = msg_with_payload.payload().get<nlohmann::json>();
        if (recovered_json) {
            std::cout << "  Payload recovery successful\n";
            std::cout << "  Payload content: " << recovered_json->dump() << "\n";
        } else {
            std::cout << "  Payload recovery failed\n";
        }
        
        // Test 3: Serialization
        std::cout << "\nTest 3: Message serialization\n";
        
        auto serialized = msg_with_payload.serialize();
        std::cout << "  Serialized size: " << serialized.size() << " bytes\n";
        
        auto deserialized = ss::message::deserialize(serialized);
        if (deserialized) {
            std::cout << "  Deserialization successful\n";
            std::cout << "  Deserialized ID: " << deserialized->id() << "\n";
            std::cout << "  Deserialized type: " << static_cast<int>(deserialized->type()) << "\n";
            
            auto payload_check = deserialized->payload().get<nlohmann::json>();
            if (payload_check) {
                std::cout << "  Deserialized payload: " << payload_check->dump() << "\n";
            }
        } else {
            std::cout << "  Deserialization failed\n";
            return 1;
        }
        
        // Test 4: Message utility functions
        std::cout << "\nTest 4: Message utilities\n";
        
        auto ping_msg = ss::message_utils::create_ping_message();
        auto pong_msg = ss::message_utils::create_pong_message();
        auto find_node_msg = ss::message_utils::create_find_node_message("target123");
        
        std::cout << "  Created ping message: ID=" << ping_msg->id() << "\n";
        std::cout << "  Created pong message: ID=" << pong_msg->id() << "\n";
        std::cout << "  Created find_node message: ID=" << find_node_msg->id() << "\n";
        
        std::cout << "  Is ping system message: " << ss::message_utils::is_system_message(ping_msg->type()) << "\n";
        std::cout << "  Does ping require response: " << ss::message_utils::requires_response(ping_msg->type()) << "\n";
        std::cout << "  Does pong require response: " << ss::message_utils::requires_response(pong_msg->type()) << "\n";
        
        std::cout << "\nAll basic tests passed successfully!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}