#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ss_p2p/network/transport_factory.hpp"

#include <boost/asio.hpp>

namespace ss::network::test {

/**
 * @brief Test fixture for transport factory tests
 */
class TransportFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        factory_ = &transport_factory::instance();
        factory_->clear();  // Start with clean state
    }

    void TearDown() override {
        factory_->clear();
    }

    transport_factory* factory_;
    boost::asio::io_context io_context_;
};

TEST_F(TransportFactoryTest, Singleton) {
    auto& factory1 = transport_factory::instance();
    auto& factory2 = transport_factory::instance();
    
    EXPECT_EQ(&factory1, &factory2);
}

TEST_F(TransportFactoryTest, TransportTypeConversion) {
    EXPECT_EQ(to_string(transport_type::udp), "udp");
    EXPECT_EQ(to_string(transport_type::tcp), "tcp");
    EXPECT_EQ(to_string(transport_type::quic), "quic");
    EXPECT_EQ(to_string(transport_type::websocket), "websocket");
    
    auto udp_result = transport_type_from_string("udp");
    ASSERT_TRUE(udp_result.is_ok());
    EXPECT_EQ(udp_result.value(), transport_type::udp);
    
    auto invalid_result = transport_type_from_string("invalid");
    EXPECT_FALSE(invalid_result.is_ok());
}

TEST_F(TransportFactoryTest, TransportRegistration) {
    transport_info info;
    info.type = transport_type::udp;
    info.name = "Test UDP";
    info.description = "Test UDP transport";
    info.connection_based = false;
    
    auto mock_factory = [](const transport_params&) -> ss::core::result<transport_ptr, factory_error> {
        return ss::core::result<transport_ptr, factory_error>::err(factory_error::not_found);
    };
    
    // Register transport
    auto result = factory_->register_transport(transport_type::udp, info, mock_factory);
    EXPECT_TRUE(result.is_ok());
    
    // Check if registered
    EXPECT_TRUE(factory_->is_transport_registered(transport_type::udp));
    
    // Try to register again (should fail)
    auto duplicate_result = factory_->register_transport(transport_type::udp, info, mock_factory);
    EXPECT_FALSE(duplicate_result.is_ok());
    EXPECT_EQ(duplicate_result.error(), factory_error::already_registered);
}

TEST_F(TransportFactoryTest, TransportUnregistration) {
    transport_info info;
    info.type = transport_type::tcp;
    
    auto mock_factory = [](const transport_params&) -> ss::core::result<transport_ptr, factory_error> {
        return ss::core::result<transport_ptr, factory_error>::err(factory_error::not_found);
    };
    
    // Register and then unregister
    factory_->register_transport(transport_type::tcp, info, mock_factory);
    EXPECT_TRUE(factory_->is_transport_registered(transport_type::tcp));
    
    auto result = factory_->unregister_transport(transport_type::tcp);
    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(factory_->is_transport_registered(transport_type::tcp));
    
    // Try to unregister non-existent transport
    auto not_found_result = factory_->unregister_transport(transport_type::tcp);
    EXPECT_FALSE(not_found_result.is_ok());
    EXPECT_EQ(not_found_result.error(), factory_error::not_found);
}

TEST_F(TransportFactoryTest, GetTransportInfo) {
    transport_info info;
    info.type = transport_type::quic;
    info.name = "Test QUIC";
    info.description = "Test QUIC transport";
    info.max_message_size = 1024;
    
    auto mock_factory = [](const transport_params&) -> ss::core::result<transport_ptr, factory_error> {
        return ss::core::result<transport_ptr, factory_error>::err(factory_error::not_found);
    };
    
    factory_->register_transport(transport_type::quic, info, mock_factory);
    
    auto info_result = factory_->get_transport_info(transport_type::quic);
    ASSERT_TRUE(info_result.is_ok());
    
    auto retrieved_info = info_result.value();
    EXPECT_EQ(retrieved_info.type, transport_type::quic);
    EXPECT_EQ(retrieved_info.name, "Test QUIC");
    EXPECT_EQ(retrieved_info.description, "Test QUIC transport");
    EXPECT_EQ(retrieved_info.max_message_size, 1024);
}

TEST_F(TransportFactoryTest, GetRegisteredTransports) {
    auto mock_factory = [](const transport_params&) -> ss::core::result<transport_ptr, factory_error> {
        return ss::core::result<transport_ptr, factory_error>::err(factory_error::not_found);
    };
    
    // Initially empty
    auto types = factory_->get_registered_transports();
    EXPECT_TRUE(types.empty());
    
    // Register some transports
    transport_info udp_info{transport_type::udp, "UDP", "UDP transport"};
    transport_info tcp_info{transport_type::tcp, "TCP", "TCP transport"};
    
    factory_->register_transport(transport_type::udp, udp_info, mock_factory);
    factory_->register_transport(transport_type::tcp, tcp_info, mock_factory);
    
    types = factory_->get_registered_transports();
    EXPECT_EQ(types.size(), 2);
    EXPECT_THAT(types, ::testing::Contains(transport_type::udp));
    EXPECT_THAT(types, ::testing::Contains(transport_type::tcp));
}

TEST_F(TransportFactoryTest, DefaultTransportRegistration) {
    auto result = factory_->register_default_transports(io_context_);
    EXPECT_TRUE(result.is_ok());
    
    EXPECT_TRUE(factory_->is_transport_registered(transport_type::udp));
    EXPECT_TRUE(factory_->is_transport_registered(transport_type::tcp));
    
    auto types = factory_->get_registered_transports();
    EXPECT_GE(types.size(), 2);
}

TEST_F(TransportFactoryTest, CreateUDPTransport) {
    factory_->register_default_transports(io_context_);
    
    transport_params params;
    params.io_context = &io_context_;
    params.config.send_buffer_size = 8192;
    
    auto result = factory_->create_transport(transport_type::udp, params);
    EXPECT_TRUE(result.is_ok());
    
    if (result.is_ok()) {
        auto transport = result.value();
        EXPECT_NE(transport, nullptr);
        EXPECT_EQ(transport->name(), "udp_transport");
        EXPECT_FALSE(transport->is_connection_based());
    }
}

TEST_F(TransportFactoryTest, CreateTCPTransport) {
    factory_->register_default_transports(io_context_);
    
    transport_params params;
    params.io_context = &io_context_;
    params.config.send_buffer_size = 8192;
    
    auto result = factory_->create_transport(transport_type::tcp, params);
    EXPECT_TRUE(result.is_ok());
    
    if (result.is_ok()) {
        auto transport = result.value();
        EXPECT_NE(transport, nullptr);
        EXPECT_EQ(transport->name(), "tcp_transport");
        EXPECT_TRUE(transport->is_connection_based());
    }
}

TEST_F(TransportFactoryTest, CreateUnsupportedTransport) {
    auto result = factory_->create_transport(transport_type::quic, transport_params{});
    EXPECT_FALSE(result.is_ok());
    EXPECT_EQ(result.error(), factory_error::unsupported_transport);
}

TEST_F(TransportFactoryTest, URLParsing) {
    factory_->register_default_transports(io_context_);
    
    // Test valid UDP URL
    auto udp_result = factory_->create_transport_from_string("udp://127.0.0.1:8080", io_context_);
    EXPECT_TRUE(udp_result.is_ok());
    
    // Test valid TCP URL with parameters
    auto tcp_result = factory_->create_transport_from_string(
        "tcp://0.0.0.0:9090?buffer_size=16384", io_context_);
    EXPECT_TRUE(tcp_result.is_ok());
    
    // Test invalid URL
    auto invalid_result = factory_->create_transport_from_string("invalid://url", io_context_);
    EXPECT_FALSE(invalid_result.is_ok());
}

TEST_F(TransportFactoryTest, ProtocolRegistration) {
    protocol_info info;
    info.name = "test_protocol";
    info.description = "Test protocol";
    info.supported_versions = {protocol_version{1, 0, 0}};
    
    auto mock_factory = [](const protocol_params&) -> ss::core::result<protocol_ptr, factory_error> {
        return ss::core::result<protocol_ptr, factory_error>::err(factory_error::not_found);
    };
    
    auto result = factory_->register_protocol("test_protocol", info, mock_factory);
    EXPECT_TRUE(result.is_ok());
    
    EXPECT_TRUE(factory_->is_protocol_registered("test_protocol"));
    
    auto protocols = factory_->get_registered_protocols();
    EXPECT_THAT(protocols, ::testing::Contains("test_protocol"));
}

TEST_F(TransportFactoryTest, ConvenienceFunctions) {
    factory_->register_default_transports(io_context_);
    
    transport_config config;
    config.send_buffer_size = 4096;
    
    auto udp_result = create_udp_transport(io_context_, config);
    EXPECT_TRUE(udp_result.is_ok());
    
    auto tcp_result = create_tcp_transport(io_context_, config);
    EXPECT_TRUE(tcp_result.is_ok());
    
    // Default protocol not implemented yet
    auto protocol_result = create_default_protocol(protocol_config{});
    EXPECT_FALSE(protocol_result.is_ok());
}

TEST_F(TransportFactoryTest, ErrorCodeConversion) {
    EXPECT_NE(to_string(factory_error::none), "");
    EXPECT_NE(to_string(factory_error::unsupported_transport), "");
    EXPECT_NE(to_string(factory_error::invalid_config), "");
    EXPECT_NE(to_string(factory_error::already_registered), "");
    EXPECT_NE(to_string(factory_error::not_found), "");
}

TEST_F(TransportFactoryTest, TransportParams) {
    transport_params params;
    
    // Test property management
    params.set_property("key1", "value1");
    params.set_property("key2", "value2");
    
    EXPECT_EQ(params.get_property("key1"), "value1");
    EXPECT_EQ(params.get_property("key2"), "value2");
    EXPECT_EQ(params.get_property("nonexistent", "default"), "default");
    
    EXPECT_TRUE(params.has_property("key1"));
    EXPECT_FALSE(params.has_property("nonexistent"));
}

TEST_F(TransportFactoryTest, ProtocolParams) {
    protocol_params params;
    
    // Test property management
    params.set_property("compression", "gzip");
    params.set_property("encryption", "aes256");
    
    EXPECT_EQ(params.get_property("compression"), "gzip");
    EXPECT_EQ(params.get_property("encryption"), "aes256");
    EXPECT_TRUE(params.has_property("compression"));
}

} // namespace ss::network::test