#pragma once

#include "../core/types.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <random>
#include <functional>

namespace ss::dht {

/**
 * @brief DHT-specific node ID type (alias for core::node_id)
 * 
 * This is an alias for ss::core::node_id with DHT-specific utility functions.
 * Provides additional methods for Kademlia distance calculations and bucket operations.
 */
using node_id = ss::core::node_id;

/**
 * @brief DHT-specific utility functions for node ID operations
 * 
 * Provides Kademlia-specific operations like bucket index calculation,
 * common prefix length determination, and distance-based comparisons.
 */
class node_id_utils {
public:
    /**
     * @brief Calculate Kademlia bucket index for two node IDs
     * @param local_id Local node's ID
     * @param remote_id Remote node's ID
     * @return Bucket index (0-159 for 160-bit IDs)
     */
    static constexpr std::uint32_t bucket_index(const node_id& local_id, const node_id& remote_id) noexcept {
        auto distance = local_id.distance(remote_id);
        return leading_zero_bits(distance);
    }

    /**
     * @brief Calculate number of leading zero bits in a node ID
     * @param id Node ID to analyze
     * @return Number of leading zero bits (0-160)
     */
    static constexpr std::uint32_t leading_zero_bits(const node_id& id) noexcept {
        const auto& data = id.data();
        std::uint32_t zero_bits = 0;
        
        for (std::size_t i = 0; i < node_id::SIZE; ++i) {
            if (data[i] == 0) {
                zero_bits += 8;
            } else {
                // Count leading zeros in this byte
                std::uint8_t byte = data[i];
                while ((byte & 0x80) == 0) {
                    ++zero_bits;
                    byte <<= 1;
                }
                break;
            }
        }
        
        return std::min(zero_bits, std::uint32_t{160});
    }

    /**
     * @brief Calculate common prefix length between two node IDs
     * @param id1 First node ID
     * @param id2 Second node ID
     * @return Number of common prefix bits
     */
    static constexpr std::uint32_t common_prefix_length(const node_id& id1, const node_id& id2) noexcept {
        return leading_zero_bits(id1.distance(id2));
    }

    /**
     * @brief Check if one distance is closer than another
     * @param target Target node ID
     * @param candidate1 First candidate node ID
     * @param candidate2 Second candidate node ID
     * @return true if candidate1 is closer to target than candidate2
     */
    static constexpr bool is_closer(const node_id& target, 
                                   const node_id& candidate1, 
                                   const node_id& candidate2) noexcept {
        auto dist1 = target.distance(candidate1);
        auto dist2 = target.distance(candidate2);
        return dist1 < dist2;
    }

    /**
     * @brief Generate random node ID in a specific bucket relative to local ID
     * @param local_id Local node's ID
     * @param bucket_index Target bucket index (0-159)
     * @return Random node ID in the specified bucket
     */
    static node_id random_in_bucket(const node_id& local_id, std::uint32_t bucket_index) {
        if (bucket_index >= 160) {
            throw std::invalid_argument("Bucket index must be less than 160");
        }

        auto result = node_id::random();
        auto& data = result.data();
        const auto& local_data = local_id.data();

        // Copy the common prefix (bucket_index bits)
        std::uint32_t prefix_bytes = bucket_index / 8;
        std::uint32_t prefix_bits = bucket_index % 8;

        // Copy complete bytes
        for (std::uint32_t i = 0; i < prefix_bytes; ++i) {
            data[i] = local_data[i];
        }

        // Handle partial byte if needed
        if (prefix_bits > 0) {
            std::uint8_t mask = static_cast<std::uint8_t>(0xFF << (8 - prefix_bits));
            data[prefix_bytes] = (local_data[prefix_bytes] & mask) | (data[prefix_bytes] & ~mask);
        }

        // Ensure the result is actually in the target bucket by setting the differing bit
        if (bucket_index < 160) {
            std::uint32_t bit_byte = bucket_index / 8;
            std::uint32_t bit_pos = 7 - (bucket_index % 8);
            std::uint8_t bit_mask = static_cast<std::uint8_t>(1 << bit_pos);
            
            // Flip the bit to ensure different bucket
            data[bit_byte] ^= bit_mask;
        }

        return result;
    }

    /**
     * @brief Create node ID with specific bit pattern for testing
     * @param bit_pattern Bit pattern as string (e.g., "1010")
     * @return Node ID with specified bit pattern
     */
    static node_id from_bit_pattern(const std::string& bit_pattern) {
        if (bit_pattern.length() > 160) {
            throw std::invalid_argument("Bit pattern too long (max 160 bits)");
        }

        node_id result;
        auto& data = result.data();
        
        for (std::size_t i = 0; i < bit_pattern.length(); ++i) {
            if (bit_pattern[i] == '1') {
                std::size_t byte_index = i / 8;
                std::size_t bit_index = 7 - (i % 8);
                data[byte_index] |= (1 << bit_index);
            } else if (bit_pattern[i] != '0') {
                throw std::invalid_argument("Bit pattern must contain only '0' and '1'");
            }
        }

        return result;
    }

    /**
     * @brief Convert node ID to bit string representation
     * @param id Node ID to convert
     * @param max_bits Maximum number of bits to include (default: all 160)
     * @return Bit string representation
     */
    static std::string to_bit_string(const node_id& id, std::uint32_t max_bits = 160) {
        const auto& data = id.data();
        std::string result;
        result.reserve(std::min(max_bits, 160u));

        std::uint32_t bits_added = 0;
        for (std::size_t i = 0; i < node_id::SIZE && bits_added < max_bits; ++i) {
            for (int bit = 7; bit >= 0 && bits_added < max_bits; --bit) {
                result += (data[i] & (1 << bit)) ? '1' : '0';
                ++bits_added;
            }
        }

        return result;
    }

    /**
     * @brief Calculate XOR metric distance as integer
     * @param id1 First node ID
     * @param id2 Second node ID
     * @return Distance as a large integer (first 8 bytes of XOR result)
     */
    static std::uint64_t distance_metric(const node_id& id1, const node_id& id2) noexcept {
        auto xor_result = id1.distance(id2);
        const auto& data = xor_result.data();
        
        std::uint64_t metric = 0;
        for (std::size_t i = 0; i < 8 && i < node_id::SIZE; ++i) {
            metric = (metric << 8) | data[i];
        }
        
        return metric;
    }

    /**
     * @brief Check if node ID is in specific bucket relative to local ID
     * @param local_id Local node's ID
     * @param remote_id Remote node's ID to test
     * @param bucket_index Expected bucket index
     * @return true if remote_id is in the specified bucket
     */
    static constexpr bool is_in_bucket(const node_id& local_id, 
                                      const node_id& remote_id, 
                                      std::uint32_t bucket_idx) noexcept {
        return bucket_index(local_id, remote_id) == bucket_idx;
    }

    /**
     * @brief Get the bit value at specific position in node ID
     * @param id Node ID to examine
     * @param bit_position Bit position (0-159, 0 is most significant)
     * @return Bit value (0 or 1)
     */
    static constexpr std::uint8_t get_bit(const node_id& id, std::uint32_t bit_position) noexcept {
        if (bit_position >= 160) {
            return 0;
        }
        
        const auto& data = id.data();
        std::uint32_t byte_index = bit_position / 8;
        std::uint32_t bit_index = 7 - (bit_position % 8);
        
        return (data[byte_index] >> bit_index) & 1;
    }

    /**
     * @brief Set the bit value at specific position in node ID
     * @param id Node ID to modify
     * @param bit_position Bit position (0-159, 0 is most significant)
     * @param bit_value Bit value to set (0 or 1)
     */
    static void set_bit(node_id& id, std::uint32_t bit_position, std::uint8_t bit_value) noexcept {
        if (bit_position >= 160 || bit_value > 1) {
            return;
        }
        
        auto& data = id.data();
        std::uint32_t byte_index = bit_position / 8;
        std::uint32_t bit_index = 7 - (bit_position % 8);
        std::uint8_t mask = static_cast<std::uint8_t>(1 << bit_index);
        
        if (bit_value) {
            data[byte_index] |= mask;
        } else {
            data[byte_index] &= ~mask;
        }
    }

    /**
     * @brief Generate node ID with specific distance from target
     * @param target Target node ID
     * @param distance_bits Number of bits difference (bucket index)
     * @return Node ID at specified distance
     */
    static node_id generate_at_distance(const node_id& target, std::uint32_t distance_bits) {
        if (distance_bits >= 160) {
            return node_id::random();
        }

        // Start with a copy of target
        auto result = target;
        
        // Flip the bit at distance_bits position to create exact distance
        set_bit(result, distance_bits, 1 - get_bit(result, distance_bits));
        
        // Randomize bits beyond distance_bits
        auto& data = result.data();
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<std::uint8_t> dist(0, 255);
        
        std::uint32_t start_byte = (distance_bits + 1) / 8;
        std::uint32_t start_bit = (distance_bits + 1) % 8;
        
        // Handle partial byte
        if (start_bit > 0 && start_byte < node_id::SIZE) {
            std::uint8_t mask = static_cast<std::uint8_t>((1 << (8 - start_bit)) - 1);
            data[start_byte] = (data[start_byte] & ~mask) | (dist(gen) & mask);
            ++start_byte;
        }
        
        // Randomize remaining complete bytes
        for (std::uint32_t i = start_byte; i < node_id::SIZE; ++i) {
            data[i] = dist(gen);
        }
        
        return result;
    }

private:
    // Static class - no instances allowed
    node_id_utils() = delete;
    ~node_id_utils() = delete;
    node_id_utils(const node_id_utils&) = delete;
    node_id_utils& operator=(const node_id_utils&) = delete;
};

/**
 * @brief Comparator for sorting node IDs by distance to a target
 */
class distance_comparator {
private:
    node_id target_;

public:
    /**
     * @brief Constructor
     * @param target Target node ID for distance comparison
     */
    explicit distance_comparator(node_id target) noexcept : target_(std::move(target)) {}

    /**
     * @brief Compare two node IDs by distance to target
     * @param a First node ID
     * @param b Second node ID
     * @return true if a is closer to target than b
     */
    bool operator()(const node_id& a, const node_id& b) const noexcept {
        return node_id_utils::is_closer(target_, a, b);
    }
};

} // namespace ss::dht

// Note: hash<ss::dht::node_id> is already provided by hash<ss::core::node_id> since they are the same type