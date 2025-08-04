/**
 * @file test_ice_integration.cpp
 * @brief Integration tests for ICE components working together
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ss_p2p/ice/ice_candidate.hpp"
#include "ss_p2p/ice/i_stun_client.hpp"
#include "ss_p2p/ice/i_turn_relay.hpp"
#include "ss_p2p/ice/i_nat_traversal.hpp"
#include "ss_p2p/core/types.hpp"
#include "ss_p2p/core/result.hpp"

#include <memory>
#include <vector>
#include <chrono>
#include <algorithm>
#include <future>

using namespace ss::ice;
using namespace ss::core;

namespace {

/**
 * @brief Test fixture for ICE integration tests
 */
class ICEIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test endpoints
        local_host_endpoint_ = endpoint("192.168.1.100", 5000);
        remote_host_endpoint_ = endpoint("192.168.1.200", 5001);
        stun_server_ = endpoint("203.0.113.10", 3478);
        turn_server_ = endpoint("198.51.100.10", 3478);
        external_endpoint_ = endpoint("203.0.113.1", 12345);
        relay_endpoint_ = endpoint("198.51.100.100", 9999);
        
        // Setup gathering configuration
        gathering_config_.gather_host_candidates = true;
        gathering_config_.gather_server_reflexive = true;
        gathering_config_.gather_relay_candidates = true;
        gathering_config_.stun_servers.push_back(stun_server_);
        gathering_config_.turn_servers.push_back(turn_server_);
        gathering_config_.turn_credentials[turn_server_.to_string()] = "user:pass";
        gathering_config_.gathering_timeout = std::chrono::milliseconds(5000);
        
        // Setup hole punch configuration
        hole_punch_config_.max_attempts = 5;
        hole_punch_config_.attempt_interval = std::chrono::milliseconds(100);
        hole_punch_config_.total_timeout = std::chrono::milliseconds(2000);
        hole_punch_config_.enable_parallel_punch = true;
        hole_punch_config_.parallel_threads = 2;
    }

    endpoint local_host_endpoint_;
    endpoint remote_host_endpoint_;
    endpoint stun_server_;
    endpoint turn_server_;
    endpoint external_endpoint_;
    endpoint relay_endpoint_;
    
    candidate_gathering_config gathering_config_;
    hole_punch_config hole_punch_config_;
};

/**
 * @brief Test complete candidate gathering process
 */
TEST_F(ICEIntegrationTest, CompleteCandidateGathering) {
    candidate_collection candidates;
    
    // Simulate host candidate gathering
    auto host_foundation = candidate_foundation::generate(
        candidate_type::host, local_host_endpoint_.address());
    auto host_priority = candidate_priority::calculate(
        candidate_type::host, 65535, candidate_component::rtp);
    
    ice_candidate host_candidate(
        host_foundation, candidate_component::rtp, candidate_transport::udp,
        host_priority, local_host_endpoint_, candidate_type::host);
    
    candidates.push_back(host_candidate);
    
    // Simulate server reflexive candidate gathering
    auto srflx_foundation = candidate_foundation::generate(
        candidate_type::server_reflexive, local_host_endpoint_.address(), 
        stun_server_.to_string());
    auto srflx_priority = candidate_priority::calculate(
        candidate_type::server_reflexive, 65534, candidate_component::rtp);
    
    ice_candidate srflx_candidate(
        srflx_foundation, candidate_component::rtp, candidate_transport::udp,
        srflx_priority, external_endpoint_, candidate_type::server_reflexive);
    srflx_candidate.set_base_address(local_host_endpoint_);
    srflx_candidate.set_related_address(stun_server_);
    
    candidates.push_back(srflx_candidate);
    
    // Simulate relay candidate gathering
    auto relay_foundation = candidate_foundation::generate(
        candidate_type::relay, turn_server_.address(), turn_server_.to_string());
    auto relay_priority = candidate_priority::calculate(
        candidate_type::relay, 0, candidate_component::rtp);
    
    ice_candidate relay_candidate(
        relay_foundation, candidate_component::rtp, candidate_transport::udp,
        relay_priority, relay_endpoint_, candidate_type::relay);
    relay_candidate.set_base_address(local_host_endpoint_);
    relay_candidate.set_related_address(turn_server_);
    
    candidates.push_back(relay_candidate);
    
    // Verify gathering results
    EXPECT_EQ(candidates.size(), 3);
    
    // Sort candidates by priority (highest first)
    std::sort(candidates.begin(), candidates.end());
    
    // Verify priority order: host > srflx > relay
    EXPECT_EQ(candidates[0].type(), candidate_type::host);
    EXPECT_EQ(candidates[1].type(), candidate_type::server_reflexive);
    EXPECT_EQ(candidates[2].type(), candidate_type::relay);
    
    // Verify all candidates are valid
    for (const auto& candidate : candidates) {
        EXPECT_TRUE(candidate.is_valid());
    }
}

/**
 * @brief Test candidate pair formation and prioritization
 */
TEST_F(ICEIntegrationTest, CandidatePairFormationAndPrioritization) {
    // Create local candidates
    candidate_collection local_candidates;
    
    ice_candidate local_host(
        candidate_foundation::generate(candidate_type::host, local_host_endpoint_.address()),
        candidate_component::rtp, candidate_transport::udp,
        candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp),
        local_host_endpoint_, candidate_type::host);
    
    ice_candidate local_srflx(
        candidate_foundation::generate(candidate_type::server_reflexive, 
                                     local_host_endpoint_.address(), stun_server_.to_string()),
        candidate_component::rtp, candidate_transport::udp,
        candidate_priority::calculate(candidate_type::server_reflexive, 65534, candidate_component::rtp),
        external_endpoint_, candidate_type::server_reflexive);
    
    local_candidates.push_back(local_host);
    local_candidates.push_back(local_srflx);
    
    // Create remote candidates
    candidate_collection remote_candidates;
    
    ice_candidate remote_host(
        candidate_foundation::generate(candidate_type::host, remote_host_endpoint_.address()),
        candidate_component::rtp, candidate_transport::udp,
        candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp),
        remote_host_endpoint_, candidate_type::host);
    
    remote_candidates.push_back(remote_host);
    
    // Form candidate pairs
    candidate_pair_collection pairs;
    
    for (const auto& local : local_candidates) {
        for (const auto& remote : remote_candidates) {
            candidate_pair pair(local, remote);
            if (pair.is_valid()) {
                pairs.emplace_back(std::move(pair));
            }
        }
    }
    
    // Verify pair formation
    EXPECT_EQ(pairs.size(), 2); // 2 local * 1 remote
    
    // Sort pairs by priority (highest first)
    std::sort(pairs.begin(), pairs.end());
    
    // Verify all pairs are valid
    for (const auto& pair : pairs) {
        EXPECT_TRUE(pair.is_valid());
        EXPECT_GT(pair.priority(), 0);
    }
    
    // Verify highest priority pair is local_host <-> remote_host
    EXPECT_EQ(pairs[0].local().type(), candidate_type::host);
    EXPECT_EQ(pairs[0].remote().type(), candidate_type::host);
}

/**
 * @brief Test connectivity check simulation
 */
TEST_F(ICEIntegrationTest, ConnectivityCheckSimulation) {
    // Create a candidate pair
    ice_candidate local_candidate(
        candidate_foundation::generate(candidate_type::host, local_host_endpoint_.address()),
        candidate_component::rtp, candidate_transport::udp,
        candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp),
        local_host_endpoint_, candidate_type::host);
    
    ice_candidate remote_candidate(
        candidate_foundation::generate(candidate_type::host, remote_host_endpoint_.address()),
        candidate_component::rtp, candidate_transport::udp,
        candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp),
        remote_host_endpoint_, candidate_type::host);
    
    candidate_pair pair(local_candidate, remote_candidate);
    
    // Simulate connectivity check states
    EXPECT_EQ(pair.get_state(), candidate_pair::state::waiting);
    
    // Start connectivity check
    pair.set_state(candidate_pair::state::in_progress);
    EXPECT_EQ(pair.get_state(), candidate_pair::state::in_progress);
    
    // Simulate successful check
    pair.set_state(candidate_pair::state::succeeded);
    pair.set_rtt(std::chrono::milliseconds(50));
    
    EXPECT_EQ(pair.get_state(), candidate_pair::state::succeeded);
    EXPECT_TRUE(pair.rtt().has_value());
    EXPECT_EQ(pair.rtt().value().count(), 50);
    
    // Test nomination
    EXPECT_FALSE(pair.is_nominated());
    pair.set_nominated(true);
    EXPECT_TRUE(pair.is_nominated());
}

/**
 * @brief Test ICE role and tie-breaking scenarios
 */
TEST_F(ICEIntegrationTest, ICERoleAndTieBreaking) {
    // Simulate ICE role assignment
    enum class ice_role { controlling, controlled };
    
    ice_role local_role = ice_role::controlling;
    ice_role remote_role = ice_role::controlled;
    
    // Generate tie-breaker values
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<std::uint64_t> dis;
    
    std::uint64_t local_tie_breaker = dis(gen);
    std::uint64_t remote_tie_breaker = dis(gen);
    
    // Ensure different tie-breaker values for test
    if (local_tie_breaker == remote_tie_breaker) {
        remote_tie_breaker = local_tie_breaker + 1;
    }
    
    // Test role conflict resolution
    if (local_role == remote_role) {
        // Both have same role - use tie-breaker
        if (local_tie_breaker > remote_tie_breaker) {
            local_role = ice_role::controlling;
            // Remote would switch to controlled
        } else {
            local_role = ice_role::controlled;
            // Remote would switch to controlling
        }
    }
    
    // Verify role assignment logic
    EXPECT_TRUE(local_role == ice_role::controlling || local_role == ice_role::controlled);
    EXPECT_NE(local_tie_breaker, remote_tie_breaker);
}

/**
 * @brief Test NAT type compatibility matrix
 */
TEST_F(ICEIntegrationTest, NATTypeCompatibilityMatrix) {
    // Define NAT type compatibility matrix
    struct nat_compatibility {
        nat_type local;
        nat_type remote;
        bool direct_possible;
        bool hole_punch_possible;
        bool relay_required;
    };
    
    std::vector<nat_compatibility> compatibility_matrix = {
        // Open Internet scenarios
        {nat_type::open_internet, nat_type::open_internet, true, true, false},
        {nat_type::open_internet, nat_type::full_cone, true, true, false},
        {nat_type::open_internet, nat_type::restricted_cone, true, true, false},
        {nat_type::open_internet, nat_type::port_restricted_cone, true, true, false},
        {nat_type::open_internet, nat_type::symmetric, true, false, false},
        
        // Full Cone NAT scenarios
        {nat_type::full_cone, nat_type::full_cone, false, true, false},
        {nat_type::full_cone, nat_type::restricted_cone, false, true, false},
        {nat_type::full_cone, nat_type::port_restricted_cone, false, true, false},
        {nat_type::full_cone, nat_type::symmetric, false, false, true},
        
        // Restricted Cone NAT scenarios
        {nat_type::restricted_cone, nat_type::restricted_cone, false, true, false},
        {nat_type::restricted_cone, nat_type::port_restricted_cone, false, true, false},
        {nat_type::restricted_cone, nat_type::symmetric, false, false, true},
        
        // Port Restricted Cone NAT scenarios
        {nat_type::port_restricted_cone, nat_type::port_restricted_cone, false, true, false},
        {nat_type::port_restricted_cone, nat_type::symmetric, false, false, true},
        
        // Symmetric NAT scenarios
        {nat_type::symmetric, nat_type::symmetric, false, false, true},
        
        // Blocked/Unknown scenarios
        {nat_type::blocked, nat_type::open_internet, false, false, true},
        {nat_type::unknown, nat_type::full_cone, false, false, true}
    };
    
    // Test each compatibility scenario
    for (const auto& compat : compatibility_matrix) {
        nat_detection_result local_nat;
        local_nat.type = compat.local;
        
        nat_detection_result remote_nat;
        remote_nat.type = compat.remote;
        
        // Test traversability
        bool local_traversable = local_nat.is_traversable();
        bool remote_traversable = remote_nat.is_traversable();
        
        if (compat.direct_possible) {
            EXPECT_TRUE(local_traversable || remote_traversable);
        }
        
        if (compat.relay_required) {
            // At least one side should require relay
            EXPECT_TRUE(!local_traversable || !remote_traversable ||
                       compat.local == nat_type::symmetric || compat.remote == nat_type::symmetric);
        }
    }
}

/**
 * @brief Test multi-component ICE scenarios (RTP + RTCP)
 */
TEST_F(ICEIntegrationTest, MultiComponentICEScenarios) {
    // Create candidates for RTP component
    ice_candidate rtp_host(
        candidate_foundation::generate(candidate_type::host, local_host_endpoint_.address()),
        candidate_component::rtp, candidate_transport::udp,
        candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp),
        local_host_endpoint_, candidate_type::host);
    
    // Create candidates for RTCP component
    endpoint rtcp_endpoint("192.168.1.100", 5001);
    ice_candidate rtcp_host(
        candidate_foundation::generate(candidate_type::host, rtcp_endpoint.address()),
        candidate_component::rtcp, candidate_transport::udp,
        candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtcp),
        rtcp_endpoint, candidate_type::host);
    
    // Verify component separation
    EXPECT_EQ(rtp_host.component(), candidate_component::rtp);
    EXPECT_EQ(rtcp_host.component(), candidate_component::rtcp);
    EXPECT_NE(rtp_host.component(), rtcp_host.component());
    
    // Verify priority calculation (RTP should have higher priority than RTCP)
    EXPECT_GT(rtp_host.priority().value(), rtcp_host.priority().value());
    
    // Test that pairs can only be formed between matching components
    ice_candidate remote_rtp(
        candidate_foundation::generate(candidate_type::host, remote_host_endpoint_.address()),
        candidate_component::rtp, candidate_transport::udp,
        candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp),
        remote_host_endpoint_, candidate_type::host);
    
    ice_candidate remote_rtcp(
        candidate_foundation::generate(candidate_type::host, "192.168.1.200"),
        candidate_component::rtcp, candidate_transport::udp,
        candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtcp),
        endpoint("192.168.1.200", 5001), candidate_type::host);
    
    // Valid pairs (matching components)
    candidate_pair rtp_pair(rtp_host, remote_rtp);
    candidate_pair rtcp_pair(rtcp_host, remote_rtcp);
    
    EXPECT_TRUE(rtp_pair.is_valid());
    EXPECT_TRUE(rtcp_pair.is_valid());
    
    // Invalid pairs (mismatched components)
    candidate_pair invalid_pair(rtp_host, remote_rtcp);
    EXPECT_FALSE(invalid_pair.is_valid());
}

/**
 * @brief Test ICE restart scenarios
 */
TEST_F(ICEIntegrationTest, ICERestartScenarios) {
    // Initial ICE session
    std::string initial_ufrag = "initial_ufrag_12345678";
    std::string initial_pwd = "initial_password_1234567890123456";
    
    // ICE restart (new credentials)
    std::string restart_ufrag = "restart_ufrag_87654321";
    std::string restart_pwd = "restart_password_6543210987654321";
    
    // Verify credentials are different
    EXPECT_NE(initial_ufrag, restart_ufrag);
    EXPECT_NE(initial_pwd, restart_pwd);
    
    // Test credential length requirements
    EXPECT_GE(initial_ufrag.length(), 4);    // RFC 8445: 4-256 characters
    EXPECT_LE(initial_ufrag.length(), 256);
    EXPECT_GE(initial_pwd.length(), 22);     // RFC 8445: 22-256 characters
    EXPECT_LE(initial_pwd.length(), 256);
    
    EXPECT_GE(restart_ufrag.length(), 4);
    EXPECT_LE(restart_ufrag.length(), 256);
    EXPECT_GE(restart_pwd.length(), 22);
    EXPECT_LE(restart_pwd.length(), 256);
    
    // Simulate ICE restart by generating new candidates
    candidate_collection initial_candidates;
    candidate_collection restart_candidates;
    
    // Add different candidates for restart scenario
    ice_candidate initial_host(
        candidate_foundation::generate(candidate_type::host, local_host_endpoint_.address()),
        candidate_component::rtp, candidate_transport::udp,
        candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp),
        local_host_endpoint_, candidate_type::host);
    
    ice_candidate restart_host(
        candidate_foundation::generate(candidate_type::host, "192.168.1.101"), // Different IP
        candidate_component::rtp, candidate_transport::udp,
        candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp),
        endpoint("192.168.1.101", 5000), candidate_type::host);
    
    initial_candidates.push_back(initial_host);
    restart_candidates.push_back(restart_host);
    
    // Verify different foundations due to different base addresses
    EXPECT_NE(initial_host.foundation(), restart_host.foundation());
}

/**
 * @brief Test bandwidth-aware candidate selection
 */
TEST_F(ICEIntegrationTest, BandwidthAwareCandidateSelection) {
    // Create candidates with different expected bandwidths
    struct bandwidth_candidate {
        ice_candidate candidate;
        std::uint64_t expected_bandwidth; // bytes per second
        std::uint32_t expected_latency;   // milliseconds
    };
    
    std::vector<bandwidth_candidate> candidates;
    
    // Host candidate - highest bandwidth, lowest latency
    candidates.push_back({
        ice_candidate(
            candidate_foundation::generate(candidate_type::host, local_host_endpoint_.address()),
            candidate_component::rtp, candidate_transport::udp,
            candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp),
            local_host_endpoint_, candidate_type::host),
        1000000000, // 1 Gbps
        1           // 1ms
    });
    
    // Server reflexive candidate - medium bandwidth, medium latency
    candidates.push_back({
        ice_candidate(
            candidate_foundation::generate(candidate_type::server_reflexive, 
                                         local_host_endpoint_.address(), stun_server_.to_string()),
            candidate_component::rtp, candidate_transport::udp,
            candidate_priority::calculate(candidate_type::server_reflexive, 65534, candidate_component::rtp),
            external_endpoint_, candidate_type::server_reflexive),
        100000000, // 100 Mbps
        20         // 20ms
    });
    
    // Relay candidate - lowest bandwidth, highest latency
    candidates.push_back({
        ice_candidate(
            candidate_foundation::generate(candidate_type::relay, turn_server_.address(), 
                                         turn_server_.to_string()),
            candidate_component::rtp, candidate_transport::udp,
            candidate_priority::calculate(candidate_type::relay, 0, candidate_component::rtp),
            relay_endpoint_, candidate_type::relay),
        10000000, // 10 Mbps
        100       // 100ms
    });
    
    // Sort by expected performance (bandwidth descending, latency ascending)
    std::sort(candidates.begin(), candidates.end(), 
              [](const bandwidth_candidate& a, const bandwidth_candidate& b) {
                  if (a.expected_bandwidth != b.expected_bandwidth) {
                      return a.expected_bandwidth > b.expected_bandwidth;
                  }
                  return a.expected_latency < b.expected_latency;
              });
    
    // Verify performance-based ordering
    EXPECT_EQ(candidates[0].candidate.type(), candidate_type::host);
    EXPECT_EQ(candidates[1].candidate.type(), candidate_type::server_reflexive);
    EXPECT_EQ(candidates[2].candidate.type(), candidate_type::relay);
    
    // Verify bandwidth values
    EXPECT_GT(candidates[0].expected_bandwidth, candidates[1].expected_bandwidth);
    EXPECT_GT(candidates[1].expected_bandwidth, candidates[2].expected_bandwidth);
    
    // Verify latency values
    EXPECT_LT(candidates[0].expected_latency, candidates[1].expected_latency);
    EXPECT_LT(candidates[1].expected_latency, candidates[2].expected_latency);
}

/**
 * @brief Test error recovery and fallback mechanisms
 */
TEST_F(ICEIntegrationTest, ErrorRecoveryAndFallbackMechanisms) {
    // Create candidate preference order for fallback
    std::vector<candidate_type> preference_order = {
        candidate_type::host,
        candidate_type::server_reflexive,
        candidate_type::peer_reflexive,
        candidate_type::relay
    };
    
    // Simulate connectivity failures and fallbacks
    struct connectivity_scenario {
        candidate_type local_type;
        candidate_type remote_type;
        bool connectivity_expected;
        std::string failure_reason;
    };
    
    std::vector<connectivity_scenario> scenarios = {
        {candidate_type::host, candidate_type::host, true, ""},
        {candidate_type::host, candidate_type::server_reflexive, true, ""},
        {candidate_type::server_reflexive, candidate_type::server_reflexive, false, "both_behind_nat"},
        {candidate_type::relay, candidate_type::relay, true, ""}
    };
    
    for (const auto& scenario : scenarios) {
        // Test fallback logic
        if (!scenario.connectivity_expected) {
            // Should fall back to next candidate type in preference order
            auto local_it = std::find(preference_order.begin(), preference_order.end(), scenario.local_type);
            auto remote_it = std::find(preference_order.begin(), preference_order.end(), scenario.remote_type);
            
            if (local_it != preference_order.end() && local_it + 1 != preference_order.end()) {
                candidate_type fallback_local = *(local_it + 1);
                EXPECT_NE(fallback_local, scenario.local_type);
            }
            
            if (remote_it != preference_order.end() && remote_it + 1 != preference_order.end()) {
                candidate_type fallback_remote = *(remote_it + 1);
                EXPECT_NE(fallback_remote, scenario.remote_type);
            }
        }
    }
}

/**
 * @brief Test concurrent ICE sessions
 */
TEST_F(ICEIntegrationTest, ConcurrentICESessions) {
    // Simulate multiple concurrent ICE sessions
    struct ice_session {
        std::string session_id;
        std::string local_ufrag;
        std::string local_pwd;
        candidate_collection candidates;
        bool gathering_complete;
        bool connectivity_established;
    };
    
    std::vector<ice_session> sessions;
    
    // Create multiple sessions
    for (int i = 0; i < 3; ++i) {
        ice_session session;
        session.session_id = "session_" + std::to_string(i);
        session.local_ufrag = "ufrag_" + std::to_string(i) + "_12345678";
        session.local_pwd = "password_" + std::to_string(i) + "_1234567890123456";
        session.gathering_complete = false;
        session.connectivity_established = false;
        
        // Add host candidate for each session
        endpoint session_endpoint("192.168.1." + std::to_string(100 + i), 5000 + i);
        ice_candidate host_candidate(
            candidate_foundation::generate(candidate_type::host, session_endpoint.address()),
            candidate_component::rtp, candidate_transport::udp,
            candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp),
            session_endpoint, candidate_type::host);
        
        session.candidates.push_back(host_candidate);
        session.gathering_complete = true;
        
        sessions.push_back(session);
    }
    
    // Verify session isolation
    EXPECT_EQ(sessions.size(), 3);
    
    for (size_t i = 0; i < sessions.size(); ++i) {
        for (size_t j = i + 1; j < sessions.size(); ++j) {
            EXPECT_NE(sessions[i].session_id, sessions[j].session_id);
            EXPECT_NE(sessions[i].local_ufrag, sessions[j].local_ufrag);
            EXPECT_NE(sessions[i].local_pwd, sessions[j].local_pwd);
            
            // Candidates should have different foundations due to different endpoints
            if (!sessions[i].candidates.empty() && !sessions[j].candidates.empty()) {
                EXPECT_NE(sessions[i].candidates[0].foundation(), sessions[j].candidates[0].foundation());
            }
        }
    }
}

} // anonymous namespace