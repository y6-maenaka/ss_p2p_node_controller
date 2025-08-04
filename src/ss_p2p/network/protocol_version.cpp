#include "../../../include/ss_p2p/network/i_protocol.hpp"

#include <sstream>
#include <regex>

namespace ss::network {

std::string protocol_version::to_string() const {
    std::ostringstream oss;
    oss << major << "." << minor << "." << patch;
    return oss.str();
}

ss::core::result<protocol_version, std::string> protocol_version::from_string(const std::string& version_str) {
    // Regular expression to match version format: major.minor.patch
    std::regex version_regex(R"(^(\d+)\.(\d+)\.(\d+)$)");
    std::smatch match;
    
    if (!std::regex_match(version_str, match, version_regex)) {
        return ss::core::result<protocol_version, std::string>::err(
            "Invalid version format. Expected 'major.minor.patch', got: " + version_str);
    }
    
    try {
        auto major_val = static_cast<std::uint16_t>(std::stoul(match[1].str()));
        auto minor_val = static_cast<std::uint16_t>(std::stoul(match[2].str()));
        auto patch_val = static_cast<std::uint16_t>(std::stoul(match[3].str()));
        
        // Check for overflow
        if (std::stoul(match[1].str()) > std::numeric_limits<std::uint16_t>::max() ||
            std::stoul(match[2].str()) > std::numeric_limits<std::uint16_t>::max() ||
            std::stoul(match[3].str()) > std::numeric_limits<std::uint16_t>::max()) {
            return ss::core::result<protocol_version, std::string>::err(
                "Version component too large for uint16_t: " + version_str);
        }
        
        return ss::core::result<protocol_version, std::string>::ok(
            protocol_version{major_val, minor_val, patch_val});
            
    } catch (const std::exception& e) {
        return ss::core::result<protocol_version, std::string>::err(
            "Failed to parse version: " + std::string(e.what()));
    }
}

} // namespace ss::network