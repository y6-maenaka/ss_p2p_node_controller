# Migration Guide: From Legacy to New Architecture

This guide helps you migrate from the legacy SS P2P Node Controller API to the new modular architecture.

## Overview of Changes

The new architecture introduces:
- **Modular design** with clear separation of concerns
- **Interface-based programming** for better testability
- **C++20 features** (concepts, coroutines, improved type safety)
- **Built-in security** with end-to-end encryption
- **Improved error handling** with Result<T,E> monad

## API Mapping

### Node Initialization

**Legacy API:**
```cpp
ss::node_controller n_controller(self_endpoint, io_context);
n_controller.start(boot_endpoints);
```

**New API:**
```cpp
// Using builder pattern
auto node = ss_p2p::application::p2p_node_builder()
    .with_endpoint(self_endpoint)
    .with_bootstrap_nodes(boot_endpoints)
    .with_io_context(io_context)
    .build();

co_await node->start();
```

### Message Sending

**Legacy API:**
```cpp
auto peer = n_controller.get_peer(peer_endpoint);
peer.send(message_str);
```

**New API:**
```cpp
// Type-safe message sending
std::vector<std::uint8_t> message_data = /* your data */;
auto result = co_await node->send_message(peer_id, message_data);
if (result.is_error()) {
    // Handle error
}
```

### Message Receiving

**Legacy API:**
```cpp
auto &message_hub = n_controller.get_message_hub();
message_hub.start([](ss::message msg, ss::peer peer){
    // Process message
});
```

**New API:**
```cpp
// Register message handler
node->on_message([](const ss_p2p::core::peer_id& from, 
                   std::span<const std::uint8_t> data) {
    // Process message with automatic decryption
});
```

## Module-by-Module Migration

### 1. Core Types

**node_id:**
- Legacy: Custom 160-bit implementation
- New: `ss_p2p::core::node_id` (std::array<uint8_t, 20>)

**endpoint:**
- Legacy: `boost::asio::ip::udp::endpoint`
- New: `ss_p2p::core::endpoint` (strong typing)

**Error handling:**
- Legacy: Exceptions and boolean returns
- New: `ss_p2p::core::result<T, E>` monad

### 2. Network Layer

**Transport:**
- Legacy: Direct UDP socket manipulation
- New: Abstract `i_transport` interface

```cpp
// Old
udp::socket socket(io_context);
socket.async_send_to(...);

// New
auto transport = transport_factory::create("udp", io_context);
co_await transport->send(message);
```

### 3. DHT/Kademlia

**Routing table:**
- Legacy: `k_routing_table` with direct access
- New: `i_routing_table` interface with async operations

```cpp
// Old
auto nodes = dht_manager.get_k_closest_nodes(target);

// New
auto nodes = co_await routing_table->find_closest(target, k);
```

### 4. ICE/NAT Traversal

**STUN/TURN:**
- Legacy: Custom implementation
- New: RFC-compliant with distributed architecture

```cpp
// Old
auto sr_obj = ice_agent->get_stun_server().binding_request(eps);

// New
auto candidates = co_await nat_traversal->gather_candidates();
auto connection = co_await nat_traversal->establish_connection(peer_id);
```

### 5. Security

**Encryption:**
- Legacy: No built-in encryption
- New: Automatic end-to-end encryption

```cpp
// New - automatic encryption
auto secure_channel = co_await node->create_secure_channel(peer_id);
co_await secure_channel->send(sensitive_data);
```

## Step-by-Step Migration Process

### Phase 1: Update Build System

1. Update CMakeLists.txt to use new requirements:
```cmake
find_package(OpenSSL 3.0 REQUIRED)
find_package(Boost 1.75 REQUIRED COMPONENTS system coroutine)
```

2. Enable C++20:
```cmake
set(CMAKE_CXX_STANDARD 20)
```

### Phase 2: Update Includes

Replace legacy includes:
```cpp
// Old
#include <ss_p2p/node_controller.hpp>
#include <ss_p2p/peer.hpp>

// New
#include <ss_p2p/application/i_p2p_node.hpp>
#include <ss_p2p/core/types.hpp>
```

### Phase 3: Update Initialization Code

1. Replace node_controller with p2p_node_builder
2. Add error handling for initialization
3. Use coroutines for async operations

### Phase 4: Update Message Handling

1. Convert string messages to byte arrays
2. Add message type definitions
3. Implement proper serialization

### Phase 5: Add Security Configuration

```cpp
ss_p2p::security::security_config security;
security.enable_encryption = true;
security.cipher_suite = "AES-256-GCM";

builder.with_security(security);
```

### Phase 6: Update Error Handling

Replace exception handling with Result<T,E>:
```cpp
// Old
try {
    peer.send(message);
} catch (const std::exception& e) {
    // Handle error
}

// New
auto result = co_await node->send_message(peer_id, data);
if (result.is_error()) {
    log_error("Send failed: {}", result.error().message());
}
```

## Compatibility Layer

For gradual migration, the legacy API is preserved in the `legacy/` headers. You can use both APIs during transition:

```cpp
// Create new node with legacy compatibility
auto modern_node = /* new API */;
auto legacy_wrapper = std::make_unique<ss::node_controller>(modern_node);
```

## Common Issues and Solutions

### Issue 1: Compilation Errors with Boost.Asio

**Problem:** `ip::address::from_string` not found
**Solution:** Use `ip::make_address` (Boost 1.66+)

### Issue 2: using namespace in Headers

**Problem:** Namespace pollution
**Solution:** Use namespace aliases:
```cpp
namespace asio = boost::asio;
namespace ip = boost::asio::ip;
```

### Issue 3: Synchronous to Asynchronous

**Problem:** Existing code is synchronous
**Solution:** Use `asio::co_spawn` or `std::future`:
```cpp
// Adapter for synchronous code
auto future = asio::co_spawn(io_context, 
    async_operation(), asio::use_future);
auto result = future.get();
```

## Testing Migration

1. Start with unit tests - migrate test code first
2. Use the compatibility layer for integration tests
3. Gradually replace legacy components
4. Monitor performance and functionality

## Performance Considerations

The new architecture provides:
- **Zero-copy message buffers** - better memory efficiency
- **Lock-free data structures** - improved concurrency
- **Coroutines** - lower overhead than callbacks
- **Connection pooling** - reduced connection overhead

## Getting Help

- Check `examples/simple_chat/` for complete migration example
- Run tests with `ctest` to verify functionality
- Enable debug logging for troubleshooting:
```cpp
builder.with_log_level("debug");
```

## Timeline Recommendations

1. **Week 1-2:** Update build system and dependencies
2. **Week 3-4:** Migrate core initialization and message handling
3. **Week 5-6:** Add security features and error handling
4. **Week 7-8:** Complete migration and remove legacy code
5. **Week 9-10:** Testing and optimization

Remember: The legacy API remains functional during migration. Take an incremental approach to minimize disruption.