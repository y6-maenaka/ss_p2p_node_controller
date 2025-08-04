#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ss_p2p/ice/i_nat_traversal.hpp"
#include "ss_p2p/ice/i_stun_client.hpp"
#include "ss_p2p/ice/ice_candidate.hpp"
#include "ss_p2p/core/types.hpp"

#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <future>
#include <chrono>
#include <atomic>

using namespace ss::ice;
using namespace ss::core;

namespace ss::ice::test {

// Mock STUN client for testing
class mock_stun_client : public i_stun_client {
public:
    explicit mock_stun_client(boost::asio::io_context& io_context)
        : io_context_(io_context) {}
    
    // i_component interface
    std::string name() const noexcept override { return "mock_stun_client"; }
    std::string version() const noexcept override { return "1.0.0"; }
    bool is_healthy() const noexcept override { return healthy_; }
    std::string status() const override { return "{}"; }
    
    async_void initialize() override { 
        initialized_ = true;
        co_return; 
    }
    
    async_void cleanup() override { 
        cleaned_up_ = true;
        co_return; 
    }
    
    async_void start() override { 
        running_ = true;
        co_return; 
    }
    
    async_void stop() override { 
        running_ = false;
        co_return; 
    }
    
    bool is_running() const noexcept override { return running_; }
    
    // i_stun_client interface mocks
    MOCK_METHOD(async_result<result<stun_binding_response, stun_error>>, 
                bind_request, (const endpoint& stun_server, const endpoint& local_endpoint), (override));
    
    MOCK_METHOD(async_result<result<void, stun_error>>, 
                keep_alive, (const endpoint& stun_server, const endpoint& local_endpoint), (override));
    
    MOCK_METHOD(void, set_timeout, (std::chrono::milliseconds timeout), (override));
    MOCK_METHOD(std::chrono::milliseconds, get_timeout, (), (const, override));
    
    // Test helpers
    void set_healthy(bool healthy) { healthy_ = healthy; }
    bool is_initialized() const { return initialized_; }
    bool is_cleaned_up() const { return cleaned_up_; }
    
private:
    boost::asio::io_context& io_context_;
    bool running_ = false;
    bool healthy_ = true;
    bool initialized_ = false;
    bool cleaned_up_ = false;
};

// Mock NAT traversal implementation for testing
class mock_nat_traversal : public i_nat_traversal {
public:
    explicit mock_nat_traversal(boost::asio::io_context& io_context)
        : io_context_(io_context) {}
    
    // i_component interface
    std::string name() const noexcept override { return "mock_nat_traversal"; }
    std::string version() const noexcept override { return "1.0.0"; }
    bool is_healthy() const noexcept override { return healthy_; }
    std::string status() const override { return "{}"; }
    
    async_void initialize() override { 
        initialized_ = true;
        co_return; 
    }
    
    async_void cleanup() override { 
        cleaned_up_ = true;
        co_return; 
    }
    
    async_void start() override { 
        running_ = true;
        co_return; 
    }
    
    async_void stop() override { 
        running_ = false;
        co_return; 
    }
    
    bool is_running() const noexcept override { return running_; }
    
    // i_nat_traversal interface mocks
    MOCK_METHOD(async_result<result<std::vector<ice_candidate>, nat_traversal_error>>,
                gather_candidates, (const candidate_gathering_config& config), (override));
    
    MOCK_METHOD(async_result<result<connectivity_check_result, nat_traversal_error>>,
                perform_connectivity_check, (const candidate_pair& pair), (override));
    
    MOCK_METHOD(async_result<result<void, nat_traversal_error>>,
                establish_connection, (const candidate_pair& selected_pair), (override));
    
    MOCK_METHOD(void, set_stun_servers, (const std::vector<endpoint>& servers), (override));
    MOCK_METHOD(void, set_turn_servers, (const std::vector<turn_server_config>& servers), (override));
    MOCK_METHOD(void, set_gathering_timeout, (std::chrono::milliseconds timeout), (override));
    
    // Test helpers
    void set_healthy(bool healthy) { healthy_ = healthy; }
    bool is_initialized() const { return initialized_; }
    bool is_cleaned_up() const { return cleaned_up_; }
    
private:
    boost::asio::io_context& io_context_;
    bool running_ = false;
    bool healthy_ = true;
    bool initialized_ = false;
    bool cleaned_up_ = false;
};

class NATTraversalTest : public ::testing::Test {
protected:
    void SetUp() override {
        io_context_ = std::make_shared<boost::asio::io_context>();
        work_guard_ = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
            io_context_->get_executor());
        
        // Start IO context in separate thread
        io_thread_ = std::thread([this]() {
            io_context_->run();
        });
        
        // Create mock instances
        stun_client_ = std::make_unique<mock_stun_client>(*io_context_);
        nat_traversal_ = std::make_unique<mock_nat_traversal>(*io_context_);
        
        // Setup test endpoints
        local_endpoint_ = endpoint("192.168.1.100", 5000);
        remote_endpoint_ = endpoint("192.168.1.200", 5001);
        stun_server1_ = endpoint("stun1.example.com", 3478);
        stun_server2_ = endpoint("stun2.example.com", 3478);
        turn_server_ = endpoint("turn.example.com", 3478);
        
        // Setup STUN binding response
        public_endpoint_ = endpoint("203.0.113.100", 12345);
        stun_response_.external_endpoint = public_endpoint_;
        stun_response_.mapped_address = public_endpoint_;
        stun_response_.server_endpoint = stun_server1_;
        stun_response_.rtt = std::chrono::milliseconds(50);
    }
    
    void TearDown() override {
        work_guard_.reset();
        if (io_thread_.joinable()) {
            io_context_->stop();
            io_thread_.join();
        }
    }
    
    // Helper function to wait for async operations
    template<typename T>
    T wait_for_result(boost::asio::awaitable<T> awaitable, std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
        std::promise<T> promise;
        auto future = promise.get_future();
        
        boost::asio::co_spawn(*io_context_, 
            [awaitable = std::move(awaitable), &promise]() mutable -> boost::asio::awaitable<void> {
                try {
                    if constexpr (std::is_void_v<T>) {
                        co_await awaitable;
                        promise.set_value();
                    } else {
                        auto result = co_await awaitable;
                        promise.set_value(std::move(result));
                    }
                } catch (...) {
                    promise.set_exception(std::current_exception());
                }
            }, boost::asio::detached);
        
        auto status = future.wait_for(timeout);
        if (status == std::future_status::timeout) {
            throw std::runtime_error("Operation timed out");
        }
        
        return future.get();
    }
    
    std::shared_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_;
    std::thread io_thread_;
    
    std::unique_ptr<mock_stun_client> stun_client_;
    std::unique_ptr<mock_nat_traversal> nat_traversal_;
    
    endpoint local_endpoint_;
    endpoint remote_endpoint_;
    endpoint public_endpoint_;
    endpoint stun_server1_;
    endpoint stun_server2_;
    endpoint turn_server_;
    
    stun_binding_response stun_response_;
};

// STUN client tests
TEST_F(NATTraversalTest, STUNClientLifecycle) {
    // Test component interface
    EXPECT_EQ(stun_client_->name(), "mock_stun_client");
    EXPECT_FALSE(stun_client_->version().empty());
    EXPECT_TRUE(stun_client_->is_healthy());
    EXPECT_FALSE(stun_client_->is_running());
    
    // Test initialization
    wait_for_result(stun_client_->initialize());
    EXPECT_TRUE(stun_client_->is_initialized());
    
    // Test start
    wait_for_result(stun_client_->start());
    EXPECT_TRUE(stun_client_->is_running());
    
    // Test stop
    wait_for_result(stun_client_->stop());
    EXPECT_FALSE(stun_client_->is_running());
    
    // Test cleanup
    wait_for_result(stun_client_->cleanup());
    EXPECT_TRUE(stun_client_->is_cleaned_up());
}

TEST_F(NATTraversalTest, STUNBindingRequest) {
    wait_for_result(stun_client_->initialize());
    wait_for_result(stun_client_->start());
    
    // Setup mock expectation
    EXPECT_CALL(*stun_client_, bind_request(stun_server1_, local_endpoint_))
        .Times(1)
        .WillOnce(::testing::Return(boost::asio::co_spawn(*io_context_, 
            [this]() -> async_result<result<stun_binding_response, stun_error>> {
                co_return result<stun_binding_response, stun_error>::ok(stun_response_);
            }(), boost::asio::use_awaitable)));
    
    auto bind_result = wait_for_result(stun_client_->bind_request(stun_server1_, local_endpoint_));
    
    EXPECT_TRUE(bind_result.is_ok());
    auto response = bind_result.value();
    EXPECT_EQ(response.external_endpoint, public_endpoint_);
    EXPECT_EQ(response.server_endpoint, stun_server1_);
    EXPECT_GT(response.rtt.count(), 0);
}

TEST_F(NATTraversalTest, STUNBindingTimeout) {
    wait_for_result(stun_client_->initialize());
    wait_for_result(stun_client_->start());
    
    // Setup mock to return timeout error
    EXPECT_CALL(*stun_client_, bind_request(stun_server1_, local_endpoint_))
        .Times(1)
        .WillOnce(::testing::Return(boost::asio::co_spawn(*io_context_, 
            []() -> async_result<result<stun_binding_response, stun_error>> {
                co_return result<stun_binding_response, stun_error>::err(stun_error::timeout);
            }(), boost::asio::use_awaitable)));
    
    auto bind_result = wait_for_result(stun_client_->bind_request(stun_server1_, local_endpoint_));
    
    EXPECT_TRUE(bind_result.is_err());
    EXPECT_EQ(bind_result.error(), stun_error::timeout);
}

TEST_F(NATTraversalTest, STUNKeepAlive) {
    wait_for_result(stun_client_->initialize());
    wait_for_result(stun_client_->start());
    
    // Setup mock expectation
    EXPECT_CALL(*stun_client_, keep_alive(stun_server1_, local_endpoint_))
        .Times(1)
        .WillOnce(::testing::Return(boost::asio::co_spawn(*io_context_, 
            []() -> async_result<result<void, stun_error>> {
                co_return result<void, stun_error>::ok();
            }(), boost::asio::use_awaitable)));
    
    auto keep_alive_result = wait_for_result(stun_client_->keep_alive(stun_server1_, local_endpoint_));
    
    EXPECT_TRUE(keep_alive_result.is_ok());
}

// Candidate gathering tests
TEST_F(NATTraversalTest, CandidateGathering) {
    wait_for_result(nat_traversal_->initialize());
    wait_for_result(nat_traversal_->start());
    
    candidate_gathering_config config;
    config.gather_host_candidates = true;
    config.gather_server_reflexive = true;
    config.gather_relay_candidates = true;
    config.stun_servers = {stun_server1_, stun_server2_};
    config.gathering_timeout = std::chrono::milliseconds(5000);
    
    // Create expected candidates
    std::vector<ice_candidate> expected_candidates;
    
    // Host candidate
    auto host_foundation = candidate_foundation::generate(candidate_type::host, local_endpoint_.address());
    auto host_priority = candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp);
    expected_candidates.emplace_back(host_foundation, candidate_component::rtp, candidate_transport::udp,
                                   host_priority, local_endpoint_, candidate_type::host);
    
    // Server reflexive candidate
    auto srflx_foundation = candidate_foundation::generate(candidate_type::server_reflexive, 
                                                          local_endpoint_.address(), stun_server1_.to_string());
    auto srflx_priority = candidate_priority::calculate(candidate_type::server_reflexive, 65534, candidate_component::rtp);
    ice_candidate srflx_candidate(srflx_foundation, candidate_component::rtp, candidate_transport::udp,
                                srflx_priority, public_endpoint_, candidate_type::server_reflexive);
    srflx_candidate.set_base_address(local_endpoint_);
    srflx_candidate.set_related_address(stun_server1_);
    expected_candidates.push_back(srflx_candidate);
    
    // Setup mock expectation
    EXPECT_CALL(*nat_traversal_, gather_candidates(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(boost::asio::co_spawn(*io_context_, 
            [expected_candidates]() -> async_result<result<std::vector<ice_candidate>, nat_traversal_error>> {
                co_return result<std::vector<ice_candidate>, nat_traversal_error>::ok(expected_candidates);
            }(), boost::asio::use_awaitable)));
    
    auto gather_result = wait_for_result(nat_traversal_->gather_candidates(config));
    
    EXPECT_TRUE(gather_result.is_ok());
    auto candidates = gather_result.value();
    EXPECT_EQ(candidates.size(), expected_candidates.size());
    
    // Verify candidate types and properties
    bool found_host = false, found_srflx = false;
    for (const auto& candidate : candidates) {
        if (candidate.type() == candidate_type::host) {
            found_host = true;
            EXPECT_EQ(candidate.address(), local_endpoint_);
        } else if (candidate.type() == candidate_type::server_reflexive) {
            found_srflx = true;
            EXPECT_EQ(candidate.address(), public_endpoint_);
            EXPECT_TRUE(candidate.base_address().has_value());
            EXPECT_EQ(candidate.base_address().value(), local_endpoint_);
        }
    }
    
    EXPECT_TRUE(found_host);
    EXPECT_TRUE(found_srflx);
}

TEST_F(NATTraversalTest, CandidateGatheringFailure) {
    wait_for_result(nat_traversal_->initialize());
    wait_for_result(nat_traversal_->start());
    
    candidate_gathering_config config;
    config.gather_host_candidates = true;
    
    // Setup mock to return error
    EXPECT_CALL(*nat_traversal_, gather_candidates(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(boost::asio::co_spawn(*io_context_, 
            []() -> async_result<result<std::vector<ice_candidate>, nat_traversal_error>> {
                co_return result<std::vector<ice_candidate>, nat_traversal_error>::err(
                    nat_traversal_error::network_failure);
            }(), boost::asio::use_awaitable)));
    
    auto gather_result = wait_for_result(nat_traversal_->gather_candidates(config));
    
    EXPECT_TRUE(gather_result.is_err());
    EXPECT_EQ(gather_result.error(), nat_traversal_error::network_failure);
}

// Connectivity checking tests
TEST_F(NATTraversalTest, ConnectivityCheck) {
    wait_for_result(nat_traversal_->initialize());
    wait_for_result(nat_traversal_->start());
    
    // Create candidate pair for testing
    auto local_foundation = candidate_foundation::generate(candidate_type::host, local_endpoint_.address());
    auto local_priority = candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp);
    ice_candidate local_candidate(local_foundation, candidate_component::rtp, candidate_transport::udp,
                                local_priority, local_endpoint_, candidate_type::host);
    
    auto remote_foundation = candidate_foundation::generate(candidate_type::host, remote_endpoint_.address());
    auto remote_priority = candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp);
    ice_candidate remote_candidate(remote_foundation, candidate_component::rtp, candidate_transport::udp,
                                 remote_priority, remote_endpoint_, candidate_type::host);
    
    candidate_pair test_pair(local_candidate, remote_candidate);
    
    // Expected connectivity check result
    connectivity_check_result expected_result;
    expected_result.success = true;
    expected_result.rtt = std::chrono::milliseconds(25);
    expected_result.local_candidate = local_candidate;
    expected_result.remote_candidate = remote_candidate;
    
    // Setup mock expectation
    EXPECT_CALL(*nat_traversal_, perform_connectivity_check(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(boost::asio::co_spawn(*io_context_, 
            [expected_result]() -> async_result<result<connectivity_check_result, nat_traversal_error>> {
                co_return result<connectivity_check_result, nat_traversal_error>::ok(expected_result);
            }(), boost::asio::use_awaitable)));
    
    auto check_result = wait_for_result(nat_traversal_->perform_connectivity_check(test_pair));
    
    EXPECT_TRUE(check_result.is_ok());
    auto result = check_result.value();
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.rtt.count(), 0);
    EXPECT_EQ(result.local_candidate.address(), local_endpoint_);
    EXPECT_EQ(result.remote_candidate.address(), remote_endpoint_);
}

TEST_F(NATTraversalTest, ConnectivityCheckFailure) {
    wait_for_result(nat_traversal_->initialize());
    wait_for_result(nat_traversal_->start());
    
    // Create a candidate pair that will fail
    auto local_foundation = candidate_foundation::generate(candidate_type::host, local_endpoint_.address());
    auto local_priority = candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp);
    ice_candidate local_candidate(local_foundation, candidate_component::rtp, candidate_transport::udp,
                                local_priority, local_endpoint_, candidate_type::host);
    
    endpoint unreachable_endpoint("10.255.255.255", 9999);
    auto remote_foundation = candidate_foundation::generate(candidate_type::host, unreachable_endpoint.address());
    auto remote_priority = candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp);
    ice_candidate remote_candidate(remote_foundation, candidate_component::rtp, candidate_transport::udp,
                                 remote_priority, unreachable_endpoint, candidate_type::host);
    
    candidate_pair test_pair(local_candidate, remote_candidate);
    
    // Expected failed result
    connectivity_check_result expected_result;
    expected_result.success = false;
    expected_result.error = nat_traversal_error::connectivity_check_failed;
    
    // Setup mock expectation
    EXPECT_CALL(*nat_traversal_, perform_connectivity_check(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(boost::asio::co_spawn(*io_context_, 
            [expected_result]() -> async_result<result<connectivity_check_result, nat_traversal_error>> {
                co_return result<connectivity_check_result, nat_traversal_error>::ok(expected_result);
            }(), boost::asio::use_awaitable)));
    
    auto check_result = wait_for_result(nat_traversal_->perform_connectivity_check(test_pair));
    
    EXPECT_TRUE(check_result.is_ok());
    auto result = check_result.value();
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error.value(), nat_traversal_error::connectivity_check_failed);
}

// Connection establishment tests
TEST_F(NATTraversalTest, ConnectionEstablishment) {
    wait_for_result(nat_traversal_->initialize());
    wait_for_result(nat_traversal_->start());
    
    // Create successful candidate pair
    auto local_foundation = candidate_foundation::generate(candidate_type::host, local_endpoint_.address());
    auto local_priority = candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp);
    ice_candidate local_candidate(local_foundation, candidate_component::rtp, candidate_transport::udp,
                                local_priority, local_endpoint_, candidate_type::host);
    
    auto remote_foundation = candidate_foundation::generate(candidate_type::host, remote_endpoint_.address());
    auto remote_priority = candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp);
    ice_candidate remote_candidate(remote_foundation, candidate_component::rtp, candidate_transport::udp,
                                 remote_priority, remote_endpoint_, candidate_type::host);
    
    candidate_pair selected_pair(local_candidate, remote_candidate);
    selected_pair.set_state(candidate_pair::state::succeeded);
    selected_pair.set_nominated(true);
    
    // Setup mock expectation
    EXPECT_CALL(*nat_traversal_, establish_connection(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(boost::asio::co_spawn(*io_context_, 
            []() -> async_result<result<void, nat_traversal_error>> {
                co_return result<void, nat_traversal_error>::ok();
            }(), boost::asio::use_awaitable)));
    
    auto establish_result = wait_for_result(nat_traversal_->establish_connection(selected_pair));
    
    EXPECT_TRUE(establish_result.is_ok());
}

// Configuration tests
TEST_F(NATTraversalTest, STUNServerConfiguration) {
    std::vector<endpoint> stun_servers = {stun_server1_, stun_server2_};
    
    EXPECT_CALL(*nat_traversal_, set_stun_servers(stun_servers))
        .Times(1);
    
    nat_traversal_->set_stun_servers(stun_servers);
}

TEST_F(NATTraversalTest, TURNServerConfiguration) {
    std::vector<turn_server_config> turn_servers;
    turn_server_config turn_config;
    turn_config.server = turn_server_;
    turn_config.username = "testuser";
    turn_config.password = "testpass";
    turn_config.realm = "example.com";
    turn_servers.push_back(turn_config);
    
    EXPECT_CALL(*nat_traversal_, set_turn_servers(turn_servers))
        .Times(1);
    
    nat_traversal_->set_turn_servers(turn_servers);
}

TEST_F(NATTraversalTest, TimeoutConfiguration) {
    auto timeout = std::chrono::milliseconds(3000);
    
    EXPECT_CALL(*stun_client_, set_timeout(timeout))
        .Times(1);
    EXPECT_CALL(*stun_client_, get_timeout())
        .Times(1)
        .WillOnce(::testing::Return(timeout));
    
    stun_client_->set_timeout(timeout);
    auto retrieved_timeout = stun_client_->get_timeout();
    EXPECT_EQ(retrieved_timeout, timeout);
    
    EXPECT_CALL(*nat_traversal_, set_gathering_timeout(timeout))
        .Times(1);
    
    nat_traversal_->set_gathering_timeout(timeout);
}

// Integration scenario tests
TEST_F(NATTraversalTest, FullNATTraversalScenario) {
    wait_for_result(nat_traversal_->initialize());
    wait_for_result(nat_traversal_->start());
    
    // Step 1: Gather candidates
    candidate_gathering_config config;
    config.gather_host_candidates = true;
    config.gather_server_reflexive = true;
    config.stun_servers = {stun_server1_};
    
    std::vector<ice_candidate> gathered_candidates;
    
    // Host candidate
    auto host_foundation = candidate_foundation::generate(candidate_type::host, local_endpoint_.address());
    auto host_priority = candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp);
    gathered_candidates.emplace_back(host_foundation, candidate_component::rtp, candidate_transport::udp,
                                   host_priority, local_endpoint_, candidate_type::host);
    
    // Server reflexive candidate
    auto srflx_foundation = candidate_foundation::generate(candidate_type::server_reflexive, 
                                                          local_endpoint_.address(), stun_server1_.to_string());
    auto srflx_priority = candidate_priority::calculate(candidate_type::server_reflexive, 65534, candidate_component::rtp);
    ice_candidate srflx_candidate(srflx_foundation, candidate_component::rtp, candidate_transport::udp,
                                srflx_priority, public_endpoint_, candidate_type::server_reflexive);
    srflx_candidate.set_base_address(local_endpoint_);
    gathered_candidates.push_back(srflx_candidate);
    
    EXPECT_CALL(*nat_traversal_, gather_candidates(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(boost::asio::co_spawn(*io_context_, 
            [gathered_candidates]() -> async_result<result<std::vector<ice_candidate>, nat_traversal_error>> {
                co_return result<std::vector<ice_candidate>, nat_traversal_error>::ok(gathered_candidates);
            }(), boost::asio::use_awaitable)));
    
    auto gather_result = wait_for_result(nat_traversal_->gather_candidates(config));
    EXPECT_TRUE(gather_result.is_ok());
    
    // Step 2: Perform connectivity checks
    auto candidates = gather_result.value();
    auto remote_foundation = candidate_foundation::generate(candidate_type::host, remote_endpoint_.address());
    auto remote_priority = candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp);
    ice_candidate remote_candidate(remote_foundation, candidate_component::rtp, candidate_transport::udp,
                                 remote_priority, remote_endpoint_, candidate_type::host);
    
    candidate_pair test_pair(candidates[0], remote_candidate); // Use host candidate
    
    connectivity_check_result check_result;
    check_result.success = true;
    check_result.rtt = std::chrono::milliseconds(30);
    check_result.local_candidate = candidates[0];
    check_result.remote_candidate = remote_candidate;
    
    EXPECT_CALL(*nat_traversal_, perform_connectivity_check(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(boost::asio::co_spawn(*io_context_, 
            [check_result]() -> async_result<result<connectivity_check_result, nat_traversal_error>> {
                co_return result<connectivity_check_result, nat_traversal_error>::ok(check_result);
            }(), boost::asio::use_awaitable)));
    
    auto connectivity_result = wait_for_result(nat_traversal_->perform_connectivity_check(test_pair));
    EXPECT_TRUE(connectivity_result.is_ok());
    EXPECT_TRUE(connectivity_result.value().success);
    
    // Step 3: Establish connection
    test_pair.set_state(candidate_pair::state::succeeded);
    test_pair.set_nominated(true);
    
    EXPECT_CALL(*nat_traversal_, establish_connection(::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(boost::asio::co_spawn(*io_context_, 
            []() -> async_result<result<void, nat_traversal_error>> {
                co_return result<void, nat_traversal_error>::ok();
            }(), boost::asio::use_awaitable)));
    
    auto establish_result = wait_for_result(nat_traversal_->establish_connection(test_pair));
    EXPECT_TRUE(establish_result.is_ok());
}

// Error handling and edge cases
TEST_F(NATTraversalTest, MultipleSTUNServerFallback) {
    wait_for_result(stun_client_->initialize());
    wait_for_result(stun_client_->start());
    
    // First server fails, second succeeds
    EXPECT_CALL(*stun_client_, bind_request(stun_server1_, local_endpoint_))
        .Times(1)
        .WillOnce(::testing::Return(boost::asio::co_spawn(*io_context_, 
            []() -> async_result<result<stun_binding_response, stun_error>> {
                co_return result<stun_binding_response, stun_error>::err(stun_error::network_failure);
            }(), boost::asio::use_awaitable)));
    
    EXPECT_CALL(*stun_client_, bind_request(stun_server2_, local_endpoint_))
        .Times(1)
        .WillOnce(::testing::Return(boost::asio::co_spawn(*io_context_, 
            [this]() -> async_result<result<stun_binding_response, stun_error>> {
                stun_binding_response response = stun_response_;
                response.server_endpoint = stun_server2_;
                co_return result<stun_binding_response, stun_error>::ok(response);
            }(), boost::asio::use_awaitable)));
    
    // Test first server failure
    auto first_result = wait_for_result(stun_client_->bind_request(stun_server1_, local_endpoint_));
    EXPECT_TRUE(first_result.is_err());
    
    // Test second server success
    auto second_result = wait_for_result(stun_client_->bind_request(stun_server2_, local_endpoint_));
    EXPECT_TRUE(second_result.is_ok());
    EXPECT_EQ(second_result.value().server_endpoint, stun_server2_);
}

TEST_F(NATTraversalTest, ConcurrentConnectivityChecks) {
    wait_for_result(nat_traversal_->initialize());
    wait_for_result(nat_traversal_->start());
    
    // Create multiple candidate pairs
    std::vector<candidate_pair> pairs;
    std::vector<std::future<result<connectivity_check_result, nat_traversal_error>>> futures;
    
    for (int i = 0; i < 5; ++i) {
        endpoint remote_ep("192.168.1." + std::to_string(200 + i), 5000 + i);
        
        auto local_foundation = candidate_foundation::generate(candidate_type::host, local_endpoint_.address());
        auto local_priority = candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp);
        ice_candidate local_candidate(local_foundation, candidate_component::rtp, candidate_transport::udp,
                                    local_priority, local_endpoint_, candidate_type::host);
        
        auto remote_foundation = candidate_foundation::generate(candidate_type::host, remote_ep.address());
        auto remote_priority = candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp);
        ice_candidate remote_candidate(remote_foundation, candidate_component::rtp, candidate_transport::udp,
                                     remote_priority, remote_ep, candidate_type::host);
        
        pairs.emplace_back(local_candidate, remote_candidate);
    }
    
    // Setup mock expectations for concurrent checks
    EXPECT_CALL(*nat_traversal_, perform_connectivity_check(::testing::_))
        .Times(5)
        .WillRepeatedly(::testing::Return(boost::asio::co_spawn(*io_context_, 
            []() -> async_result<result<connectivity_check_result, nat_traversal_error>> {
                connectivity_check_result result;
                result.success = true;
                result.rtt = std::chrono::milliseconds(40);
                co_return result<connectivity_check_result, nat_traversal_error>::ok(result);
            }(), boost::asio::use_awaitable)));
    
    // Launch concurrent connectivity checks
    for (const auto& pair : pairs) {
        auto promise = std::make_shared<std::promise<result<connectivity_check_result, nat_traversal_error>>>();
        futures.push_back(promise->get_future());
        
        boost::asio::co_spawn(*io_context_, 
            [this, pair, promise]() -> boost::asio::awaitable<void> {
                try {
                    auto result = co_await nat_traversal_->perform_connectivity_check(pair);
                    promise->set_value(result);
                } catch (...) {
                    promise->set_exception(std::current_exception());
                }
            }, boost::asio::detached);
    }
    
    // Wait for all checks to complete
    int successful_checks = 0;
    for (auto& future : futures) {
        auto status = future.wait_for(std::chrono::milliseconds(2000));
        if (status == std::future_status::ready) {
            auto result = future.get();
            if (result.is_ok() && result.value().success) {
                successful_checks++;
            }
        }
    }
    
    EXPECT_EQ(successful_checks, 5);
}

} // namespace ss::ice::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}