/**
 * @file test_ice_candidate.cpp
 * @brief Unit tests for ICE candidate classes and functionality
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ss_p2p/ice/ice_candidate.hpp"
#include "ss_p2p/core/types.hpp"

#include <chrono>
#include <vector>
#include <algorithm>

using namespace ss::ice;
using namespace ss::core;

namespace {

/**
 * @brief Test fixture for ICE candidate tests
 */
class ICECandidateTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test endpoints
        local_host_endpoint_ = endpoint("192.168.1.100", 5000);
        remote_host_endpoint_ = endpoint("192.168.1.200", 5001);
        srflx_endpoint_ = endpoint("203.0.113.1", 12345);
        relay_endpoint_ = endpoint("198.51.100.1", 9999);
        stun_server_ = endpoint("203.0.113.10", 3478);
        turn_server_ = endpoint("198.51.100.10", 3478);
        
        // Create test foundations
        host_foundation_ = candidate_foundation::generate(
            candidate_type::host, local_host_endpoint_.address());
        srflx_foundation_ = candidate_foundation::generate(
            candidate_type::server_reflexive, local_host_endpoint_.address(), 
            stun_server_.to_string());
        relay_foundation_ = candidate_foundation::generate(
            candidate_type::relay, turn_server_.address(), 
            turn_server_.to_string());
    }

    endpoint local_host_endpoint_;
    endpoint remote_host_endpoint_;
    endpoint srflx_endpoint_;
    endpoint relay_endpoint_;
    endpoint stun_server_;
    endpoint turn_server_;
    
    candidate_foundation host_foundation_;
    candidate_foundation srflx_foundation_;
    candidate_foundation relay_foundation_;
};

/**
 * @brief Test candidate foundation generation and comparison
 */
TEST_F(ICECandidateTest, FoundationGenerationAndComparison) {
    // Test foundation generation
    EXPECT_FALSE(host_foundation_.value().empty());
    EXPECT_FALSE(srflx_foundation_.value().empty());
    EXPECT_FALSE(relay_foundation_.value().empty());
    
    // Different candidate types should have different foundations
    EXPECT_NE(host_foundation_, srflx_foundation_);
    EXPECT_NE(host_foundation_, relay_foundation_);
    EXPECT_NE(srflx_foundation_, relay_foundation_);
    
    // Same parameters should generate same foundation
    auto duplicate_host = candidate_foundation::generate(
        candidate_type::host, local_host_endpoint_.address());
    EXPECT_EQ(host_foundation_, duplicate_host);
    
    // Foundation equality and ordering
    EXPECT_TRUE(host_foundation_ == host_foundation_);
    EXPECT_FALSE(host_foundation_ != host_foundation_);
    EXPECT_TRUE(host_foundation_ < srflx_foundation_ || srflx_foundation_ < host_foundation_);
}

/**
 * @brief Test candidate priority calculation
 */
TEST_F(ICECandidateTest, PriorityCalculation) {
    // Test priority calculation for different types
    auto host_priority = candidate_priority::calculate(
        candidate_type::host, 65535, candidate_component::rtp);
    auto srflx_priority = candidate_priority::calculate(
        candidate_type::server_reflexive, 65534, candidate_component::rtp);
    auto peer_reflexive_priority = candidate_priority::calculate(
        candidate_type::peer_reflexive, 65533, candidate_component::rtp);
    auto relay_priority = candidate_priority::calculate(
        candidate_type::relay, 0, candidate_component::rtp);
    
    // Host candidates should have highest priority
    EXPECT_GT(host_priority.value(), srflx_priority.value());
    EXPECT_GT(srflx_priority.value(), peer_reflexive_priority.value());
    EXPECT_GT(peer_reflexive_priority.value(), relay_priority.value());
    
    // RTP component should have higher priority than RTCP
    auto rtp_priority = candidate_priority::calculate(
        candidate_type::host, 65535, candidate_component::rtp);
    auto rtcp_priority = candidate_priority::calculate(
        candidate_type::host, 65535, candidate_component::rtcp);
    
    EXPECT_GT(rtp_priority.value(), rtcp_priority.value());
    
    // Priority comparison operators
    EXPECT_TRUE(host_priority > srflx_priority);
    EXPECT_FALSE(host_priority < srflx_priority);
    EXPECT_TRUE(host_priority == host_priority);
    EXPECT_FALSE(host_priority != host_priority);
}

/**
 * @brief Test ICE candidate creation and properties
 */
TEST_F(ICECandidateTest, CandidateCreationAndProperties) {
    // Create host candidate
    auto priority = candidate_priority::calculate(
        candidate_type::host, 65535, candidate_component::rtp);
    
    ice_candidate host_candidate(
        host_foundation_,
        candidate_component::rtp,
        candidate_transport::udp,
        priority,
        local_host_endpoint_,
        candidate_type::host
    );
    
    // Test basic properties
    EXPECT_EQ(host_candidate.foundation(), host_foundation_);
    EXPECT_EQ(host_candidate.component(), candidate_component::rtp);
    EXPECT_EQ(host_candidate.transport(), candidate_transport::udp);
    EXPECT_EQ(host_candidate.priority(), priority);
    EXPECT_EQ(host_candidate.address(), local_host_endpoint_);
    EXPECT_EQ(host_candidate.type(), candidate_type::host);
    
    // Test optional properties
    EXPECT_FALSE(host_candidate.base_address().has_value());
    EXPECT_FALSE(host_candidate.related_address().has_value());
    
    // Test validity
    EXPECT_TRUE(host_candidate.is_valid());
    EXPECT_TRUE(host_candidate.is_default_candidate());
    
    // Test age calculation
    auto age = host_candidate.age();
    EXPECT_GE(age.count(), 0);
    
    // Create server reflexive candidate with related addresses
    ice_candidate srflx_candidate(
        srflx_foundation_,
        candidate_component::rtp,
        candidate_transport::udp,
        candidate_priority::calculate(candidate_type::server_reflexive, 65534, candidate_component::rtp),
        srflx_endpoint_,
        candidate_type::server_reflexive
    );
    
    srflx_candidate.set_base_address(local_host_endpoint_);
    srflx_candidate.set_related_address(stun_server_);
    
    EXPECT_TRUE(srflx_candidate.base_address().has_value());
    EXPECT_EQ(srflx_candidate.base_address().value(), local_host_endpoint_);
    EXPECT_TRUE(srflx_candidate.related_address().has_value());
    EXPECT_EQ(srflx_candidate.related_address().value(), stun_server_);
    EXPECT_TRUE(srflx_candidate.is_default_candidate());
}

/**
 * @brief Test candidate extensions
 */
TEST_F(ICECandidateTest, CandidateExtensions) {
    ice_candidate candidate(
        host_foundation_,
        candidate_component::rtp,
        candidate_transport::udp,
        candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp),
        local_host_endpoint_,
        candidate_type::host
    );
    
    // Test extension attributes
    EXPECT_TRUE(candidate.extensions().empty());
    
    candidate.add_extension("generation", "1");
    candidate.add_extension("network-id", "1");
    candidate.add_extension("network-cost", "10");
    
    const auto& extensions = candidate.extensions();
    EXPECT_EQ(extensions.size(), 3);
    EXPECT_EQ(extensions.at("generation"), "1");
    EXPECT_EQ(extensions.at("network-id"), "1");
    EXPECT_EQ(extensions.at("network-cost"), "10");
}

/**
 * @brief Test SDP candidate string conversion
 */
TEST_F(ICECandidateTest, SDPConversion) {
    // Create test candidate
    ice_candidate candidate(
        host_foundation_,
        candidate_component::rtp,
        candidate_transport::udp,
        candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp),
        local_host_endpoint_,
        candidate_type::host
    );
    
    // Convert to SDP
    auto sdp_string = candidate.to_sdp();
    EXPECT_FALSE(sdp_string.empty());
    EXPECT_THAT(sdp_string, ::testing::HasSubstr("candidate:"));
    EXPECT_THAT(sdp_string, ::testing::HasSubstr(host_foundation_.value()));
    EXPECT_THAT(sdp_string, ::testing::HasSubstr("1")); // Component
    EXPECT_THAT(sdp_string, ::testing::HasSubstr("udp"));
    EXPECT_THAT(sdp_string, ::testing::HasSubstr(local_host_endpoint_.address()));
    EXPECT_THAT(sdp_string, ::testing::HasSubstr(std::to_string(local_host_endpoint_.port())));
    EXPECT_THAT(sdp_string, ::testing::HasSubstr("host"));
    
    // Parse from SDP
    auto parse_result = ice_candidate::from_sdp(sdp_string);
    ASSERT_TRUE(parse_result.is_ok());
    
    auto parsed_candidate = parse_result.value();
    EXPECT_EQ(parsed_candidate.foundation().value(), candidate.foundation().value());
    EXPECT_EQ(parsed_candidate.component(), candidate.component());
    EXPECT_EQ(parsed_candidate.transport(), candidate.transport());
    EXPECT_EQ(parsed_candidate.address(), candidate.address());
    EXPECT_EQ(parsed_candidate.type(), candidate.type());
    
    // Test invalid SDP parsing
    auto invalid_result = ice_candidate::from_sdp("invalid sdp line");
    EXPECT_TRUE(invalid_result.is_err());
}

/**
 * @brief Test candidate comparison and equality
 */
TEST_F(ICECandidateTest, CandidateComparison) {
    auto priority1 = candidate_priority::calculate(
        candidate_type::host, 65535, candidate_component::rtp);
    auto priority2 = candidate_priority::calculate(
        candidate_type::server_reflexive, 65534, candidate_component::rtp);
    
    ice_candidate candidate1(
        host_foundation_, candidate_component::rtp, candidate_transport::udp,
        priority1, local_host_endpoint_, candidate_type::host);
    
    ice_candidate candidate2(
        host_foundation_, candidate_component::rtp, candidate_transport::udp,
        priority1, local_host_endpoint_, candidate_type::host);
    
    ice_candidate candidate3(
        srflx_foundation_, candidate_component::rtp, candidate_transport::udp,
        priority2, srflx_endpoint_, candidate_type::server_reflexive);
    
    // Test equality
    EXPECT_TRUE(candidate1 == candidate2);
    EXPECT_FALSE(candidate1 != candidate2);
    EXPECT_FALSE(candidate1 == candidate3);
    EXPECT_TRUE(candidate1 != candidate3);
    
    // Test priority-based ordering (higher priority should be "less than" for sorting)
    EXPECT_TRUE(candidate1 < candidate3); // Higher priority candidate comes first
}

/**
 * @brief Test candidate pair creation and properties
 */
TEST_F(ICECandidateTest, CandidatePairCreation) {
    // Create local and remote candidates
    ice_candidate local_candidate(
        host_foundation_, candidate_component::rtp, candidate_transport::udp,
        candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp),
        local_host_endpoint_, candidate_type::host);
    
    ice_candidate remote_candidate(
        host_foundation_, candidate_component::rtp, candidate_transport::udp,
        candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp),
        remote_host_endpoint_, candidate_type::host);
    
    // Create candidate pair
    candidate_pair pair(local_candidate, remote_candidate);
    
    // Test basic properties
    EXPECT_EQ(pair.local(), local_candidate);
    EXPECT_EQ(pair.remote(), remote_candidate);
    EXPECT_EQ(pair.get_state(), candidate_pair::state::waiting);
    EXPECT_FALSE(pair.is_nominated());
    EXPECT_FALSE(pair.is_default());
    EXPECT_FALSE(pair.rtt().has_value());
    
    // Test validity
    EXPECT_TRUE(pair.is_valid());
    EXPECT_TRUE(pair.has_matching_foundations());
    
    // Test state management
    pair.set_state(candidate_pair::state::in_progress);
    EXPECT_EQ(pair.get_state(), candidate_pair::state::in_progress);
    
    pair.set_state(candidate_pair::state::succeeded);
    EXPECT_EQ(pair.get_state(), candidate_pair::state::succeeded);
    
    // Test nomination and default flags
    pair.set_nominated(true);
    EXPECT_TRUE(pair.is_nominated());
    
    pair.set_default(true);
    EXPECT_TRUE(pair.is_default());
    
    // Test RTT measurement
    auto rtt = std::chrono::milliseconds(50);
    pair.set_rtt(rtt);
    EXPECT_TRUE(pair.rtt().has_value());
    EXPECT_EQ(pair.rtt().value(), rtt);
}

/**
 * @brief Test candidate pair priority calculation
 */
TEST_F(ICECandidateTest, CandidatePairPriority) {
    auto local_priority = candidate_priority::calculate(
        candidate_type::host, 65535, candidate_component::rtp);
    auto remote_priority = candidate_priority::calculate(
        candidate_type::server_reflexive, 65534, candidate_component::rtp);
    
    // Test priority calculation directly
    auto calculated_priority = candidate_pair::calculate_pair_priority(
        local_priority, remote_priority);
    EXPECT_GT(calculated_priority, 0);
    
    // Create pair and verify priority
    ice_candidate local_candidate(
        host_foundation_, candidate_component::rtp, candidate_transport::udp,
        local_priority, local_host_endpoint_, candidate_type::host);
    
    ice_candidate remote_candidate(
        srflx_foundation_, candidate_component::rtp, candidate_transport::udp,
        remote_priority, remote_host_endpoint_, candidate_type::server_reflexive);
    
    candidate_pair pair(local_candidate, remote_candidate);
    EXPECT_EQ(pair.priority(), calculated_priority);
    
    // Test that higher priority pairs sort first
    auto local_priority2 = candidate_priority::calculate(
        candidate_type::server_reflexive, 65534, candidate_component::rtp);
    auto remote_priority2 = candidate_priority::calculate(
        candidate_type::relay, 0, candidate_component::rtp);
    
    ice_candidate local_candidate2(
        srflx_foundation_, candidate_component::rtp, candidate_transport::udp,
        local_priority2, srflx_endpoint_, candidate_type::server_reflexive);
    
    ice_candidate remote_candidate2(
        relay_foundation_, candidate_component::rtp, candidate_transport::udp,
        remote_priority2, relay_endpoint_, candidate_type::relay);
    
    candidate_pair pair2(local_candidate2, remote_candidate2);
    
    // Higher priority pair should be "less than" for sorting
    EXPECT_TRUE(pair < pair2 || pair2 < pair);
}

/**
 * @brief Test candidate pair state conversion
 */
TEST_F(ICECandidateTest, CandidatePairStateConversion) {
    // Test state to string conversion
    EXPECT_EQ(candidate_pair::to_string(candidate_pair::state::waiting), "waiting");
    EXPECT_EQ(candidate_pair::to_string(candidate_pair::state::in_progress), "in_progress");
    EXPECT_EQ(candidate_pair::to_string(candidate_pair::state::succeeded), "succeeded");
    EXPECT_EQ(candidate_pair::to_string(candidate_pair::state::failed), "failed");
    EXPECT_EQ(candidate_pair::to_string(candidate_pair::state::frozen), "frozen");
}

/**
 * @brief Test invalid candidate scenarios
 */
TEST_F(ICECandidateTest, InvalidCandidateScenarios) {
    // Create candidate with invalid endpoint
    endpoint invalid_endpoint;
    EXPECT_FALSE(invalid_endpoint.is_valid());
    
    ice_candidate invalid_candidate(
        host_foundation_, candidate_component::rtp, candidate_transport::udp,
        candidate_priority(0), invalid_endpoint, candidate_type::host);
    
    EXPECT_FALSE(invalid_candidate.is_valid());
    
    // Create candidate pair with mismatched components
    ice_candidate local_rtp(
        host_foundation_, candidate_component::rtp, candidate_transport::udp,
        candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp),
        local_host_endpoint_, candidate_type::host);
    
    ice_candidate remote_rtcp(
        host_foundation_, candidate_component::rtcp, candidate_transport::udp,
        candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtcp),
        remote_host_endpoint_, candidate_type::host);
    
    candidate_pair mismatched_pair(local_rtp, remote_rtcp);
    EXPECT_FALSE(mismatched_pair.is_valid());
}

/**
 * @brief Test gathering configuration
 */
TEST_F(ICECandidateTest, GatheringConfiguration) {
    candidate_gathering_config config;
    
    // Test default configuration
    EXPECT_TRUE(config.gather_host_candidates);
    EXPECT_TRUE(config.gather_server_reflexive);
    EXPECT_FALSE(config.gather_relay_candidates);
    EXPECT_TRUE(config.stun_servers.empty());
    EXPECT_TRUE(config.turn_servers.empty());
    EXPECT_TRUE(config.interface_filter.empty());
    EXPECT_EQ(config.port_range.first, 0);
    EXPECT_EQ(config.port_range.second, 0);
    EXPECT_TRUE(config.enable_ipv4);
    EXPECT_TRUE(config.enable_ipv6);
    
    // Test configuration modification
    config.gather_relay_candidates = true;
    config.stun_servers.push_back(stun_server_);
    config.turn_servers.push_back(turn_server_);
    config.turn_credentials[turn_server_.to_string()] = "user:pass";
    config.port_range = {10000, 20000};
    config.gathering_timeout = std::chrono::milliseconds(10000);
    
    EXPECT_TRUE(config.gather_relay_candidates);
    EXPECT_EQ(config.stun_servers.size(), 1);
    EXPECT_EQ(config.turn_servers.size(), 1);
    EXPECT_EQ(config.turn_credentials.size(), 1);
    EXPECT_EQ(config.port_range.first, 10000);
    EXPECT_EQ(config.port_range.second, 20000);
    EXPECT_EQ(config.gathering_timeout.count(), 10000);
}

/**
 * @brief Test candidate collections and sorting
 */
TEST_F(ICECandidateTest, CandidateCollectionsAndSorting) {
    // Create candidates with different priorities
    std::vector<ice_candidate> candidates;
    
    // Host candidate (highest priority)
    candidates.emplace_back(
        host_foundation_, candidate_component::rtp, candidate_transport::udp,
        candidate_priority::calculate(candidate_type::host, 65535, candidate_component::rtp),
        local_host_endpoint_, candidate_type::host);
    
    // Server reflexive candidate
    candidates.emplace_back(
        srflx_foundation_, candidate_component::rtp, candidate_transport::udp,
        candidate_priority::calculate(candidate_type::server_reflexive, 65534, candidate_component::rtp),
        srflx_endpoint_, candidate_type::server_reflexive);
    
    // Relay candidate (lowest priority)
    candidates.emplace_back(
        relay_foundation_, candidate_component::rtp, candidate_transport::udp,
        candidate_priority::calculate(candidate_type::relay, 0, candidate_component::rtp),
        relay_endpoint_, candidate_type::relay);
    
    // Sort candidates (should be in priority order)
    std::sort(candidates.begin(), candidates.end());
    
    // Verify sorting order (highest priority first)
    EXPECT_EQ(candidates[0].type(), candidate_type::host);
    EXPECT_EQ(candidates[1].type(), candidate_type::server_reflexive);
    EXPECT_EQ(candidates[2].type(), candidate_type::relay);
    
    // Test candidate pair collection
    candidate_pair_collection pairs;
    
    for (size_t i = 0; i < candidates.size(); ++i) {
        for (size_t j = 0; j < candidates.size(); ++j) {
            if (i != j) {
                candidate_pair pair(candidates[i], candidates[j]);
                if (pair.is_valid()) {
                    pairs.emplace_back(std::move(pair));
                }
            }
        }
    }
    
    // Sort pairs by priority
    std::sort(pairs.begin(), pairs.end());
    
    // Verify that pairs are sorted correctly
    for (size_t i = 1; i < pairs.size(); ++i) {
        EXPECT_GE(pairs[i-1].priority(), pairs[i].priority());
    }
}

/**
 * @brief Test string conversion functions
 */
TEST_F(ICECandidateTest, StringConversions) {
    // Test candidate type to string
    EXPECT_EQ(to_string(candidate_type::host), "host");
    EXPECT_EQ(to_string(candidate_type::server_reflexive), "srflx");
    EXPECT_EQ(to_string(candidate_type::peer_reflexive), "prflx");
    EXPECT_EQ(to_string(candidate_type::relay), "relay");
    
    // Test transport to string
    EXPECT_EQ(to_string(candidate_transport::udp), "udp");
    EXPECT_EQ(to_string(candidate_transport::tcp), "tcp");
    
    // Test component to string
    EXPECT_EQ(to_string(candidate_component::rtp), "1");
    EXPECT_EQ(to_string(candidate_component::rtcp), "2");
}

} // anonymous namespace