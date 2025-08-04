#pragma once

// Implementation file for ss_logger template methods
// This file contains the constructor and non-template method implementations

#include <iostream>
#include <string>

namespace ss {

// Constructor implementation (inline in header since it's simple)
inline ss_logger::ss_logger() {
    try {
        // Create system logger with file output
        #ifndef SS_LOGGING_DISABLE
        _system_logger = logger::create_file_logger(
            "ss_p2p_system", 
            SS_LOGGER_SYSTEM_OUTFILE_NAME
        );
        _system_logger->set_pattern("[SS_P2P][%Y-%m-%d %H:%M:%S] [%l] %v");
        
        // Create packet logger with separate file
        _packet_logger = logger::create_file_logger(
            "ss_p2p_packet", 
            SS_LOGGER_PACKET_OUTFILE_NAME
        );
        _packet_logger->set_pattern("[SS_P2P][%Y-%m-%d %H:%M:%S] [%l] %v");
        #else
        // Logging disabled - create null loggers
        _system_logger = nullptr;
        _packet_logger = nullptr;
        #endif
        
    } catch (const std::exception& e) {
        // Fallback to console logging if file logging fails
        std::cerr << "ss_logger: Failed to create file loggers, using console: " << e.what() << std::endl;
        
        _system_logger = logger::create_console_logger("ss_p2p_system_console");
        _packet_logger = logger::create_console_logger("ss_p2p_packet_console");
        
        if (_system_logger) {
            _system_logger->set_pattern("[SS_P2P][%H:%M:%S] [%l] %v");
        }
        if (_packet_logger) {
            _packet_logger->set_pattern("[SS_P2P][%H:%M:%S] [%l] %v");
        }
    }
}

// Convert legacy log level to new logger level
inline logger::level ss_logger::convert_log_level(const logger::log_level& ll) const {
    switch (ll) {
        case logger::log_level::DEBUG:
            return logger::level::debug;
        case logger::log_level::INFO:
            return logger::level::info;
        case logger::log_level::WARN:
            return logger::level::warn;
        case logger::log_level::ERROR:
            return logger::level::err;
        case logger::log_level::ALERT:
            return logger::level::critical;
        default:
            return logger::level::info;
    }
}

} // namespace ss
