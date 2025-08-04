#include <iostream>
#include <chrono>
#include <memory>
#include <boost/asio.hpp>
#include <ss_p2p/message.hpp>
#include <ss_p2p/message_pool.hpp>

// Simple test to validate basic functionality
int main() {
    std::cout << "Testing new message system...\n";
    
    try {
        // Test 1: Basic message creation and serialization
        std::cout << "Test 1: Message creation and serialization\n";
        
        nlohmann::json test_data = {
            {"type", "test"},
            {"payload", "Hello, World!"},
            {"timestamp", 123456789}
        };
        
        ss::message_payload payload(test_data);
        ss::message msg(ss::message_type::user_data, std::move(payload));
        
        std::cout << "  Message ID: " << msg.id() << "\n";
        std::cout << "  Message type: " << static_cast<int>(msg.type()) << "\n";
        std::cout << "  Message size: " << msg.total_size() << " bytes\n";
        
        // Test serialization
        auto serialized = msg.serialize();
        std::cout << "  Serialized size: " << serialized.size() << " bytes\n";
        
        // Test deserialization
        auto deserialized = ss::message::deserialize(serialized);
        if (deserialized) {
            std::cout << "  Deserialization successful\n";
            std::cout << "  Deserialized ID: " << deserialized->id() << "\n";
            std::cout << "  Deserialized type: " << static_cast<int>(deserialized->type()) << "\n";
        } else {
            std::cout << "  Deserialization failed\n";
            return 1;
        }
        
        // Test 2: Message pool basic operations
        std::cout << "\nTest 2: Message pool operations\n";
        
        boost::asio::io_context io_context;
        
        // Mock peer resolver
        auto peer_resolver = [](const ss::endpoint_t& ep) -> ss::peer_ptr {
            return nullptr; // Simple mock for testing
        };
        
        auto pool = std::make_unique<ss::message_pool>(io_context, peer_resolver);
        pool->start();
        
        // Create test endpoint
        boost::asio::ip::address_v4 addr = boost::asio::ip::make_address_v4("127.0.0.1");
        ss::endpoint_t test_endpoint(addr, 8080);
        
        // Store a message
        auto test_msg = std::make_shared<ss::message>(ss::message_type::ping);
        pool->store_message(test_msg, test_endpoint);
        std::cout << "  Message stored\n";
        
        // Retrieve the message
        auto retrieved = pool->retrieve_message(test_endpoint, std::chrono::milliseconds{100});
        if (retrieved) {
            std::cout << "  Message retrieved successfully\n";
            std::cout << "  Retrieved message ID: " << (*retrieved)->id() << "\n";
            std::cout << "  Retrieved message type: " << static_cast<int>((*retrieved)->type()) << "\n";
        } else {
            std::cout << "  Message retrieval failed\n";
            return 1;
        }
        
        // Test 3: Pool statistics
        std::cout << "\nTest 3: Pool statistics\n";
        auto stats = pool->get_statistics();
        std::cout << "  Active buffers: " << stats.active_buffers << "\n";
        std::cout << "  Messages stored: " << stats.messages_stored << "\n";
        std::cout << "  Messages retrieved: " << stats.messages_retrieved << "\n";
        std::cout << "  Cache hits: " << stats.buffer_cache_hits << "\n";
        std::cout << "  Cache misses: " << stats.buffer_cache_misses << "\n";
        
        // Test 4: Memory pool
        std::cout << "\nTest 4: Memory pool operations\n";
        auto& memory_pool = pool->get_memory_pool();
        
        // Allocate some memory
        auto buffer1 = memory_pool.allocate(1024);
        auto buffer2 = memory_pool.allocate(2048);
        
        std::cout << "  Allocated two buffers\n";
        
        const auto& memory_stats = memory_pool.get_stats();
        std::cout << "  Total allocations: " << memory_stats.total_allocations.load() << "\n";
        std::cout << "  Active allocations: " << memory_stats.active_allocations.load() << "\n";
        std::cout << "  Pool hits: " << memory_stats.pool_hits.load() << "\n";
        std::cout << "  Pool misses: " << memory_stats.pool_misses.load() << "\n";
        
        // Deallocate
        memory_pool.deallocate(std::move(buffer1), 1024);
        memory_pool.deallocate(std::move(buffer2), 2048);
        
        std::cout << "  Deallocated buffers\n";
        
        const auto& final_stats = memory_pool.get_stats();
        std::cout << "  Active allocations after deallocation: " << final_stats.active_allocations.load() << "\n";
        
        pool->stop();
        std::cout << "\nAll tests passed successfully!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}