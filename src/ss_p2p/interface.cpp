#include <ss_p2p/interface.hpp>
#include <iostream>
#include <sstream>

namespace ss {

interface::interface(core::io_context& io_ctx, 
                    const input_command_callback& notify_func, 
                    std::shared_ptr<logger::logger> logger_ptr)
    : _io_ctx(io_ctx)
    , _notify_func(notify_func)
    , _logger(logger_ptr) {
    
    _running = true;
    _input_thread = std::thread([this]() { this->input_processing_loop(); });

    if (_logger) {
        _logger->info("Legacy interface started (deprecated - use application layer instead)");
    }
}

interface::~interface() {
    stop();
    
    if (_logger) {
        _logger->info("Legacy interface stopped");
    }
}

void interface::listen_stdin() {
    // Legacy API - delegates to the new implementation
    // The actual listening is handled by the background thread
    
    if (_logger) {
        _logger->debug("listen_stdin called (legacy API - running in background thread)");
    }
}

void interface::stop() {
    std::lock_guard<std::mutex> lock(_mutex);
    
    if (_running) {
        _stop_requested = true;
        _running = false;
        
        if (_input_thread.joinable()) {
            _input_thread.join();
        }
    }
}

bool interface::is_running() const noexcept {
    std::lock_guard<std::mutex> lock(_mutex);
    return _running;
}

void interface::input_processing_loop() {
    std::vector<std::string> inputs;
    std::string input_line;

    while (!_stop_requested) {
        try {
            if (!std::getline(std::cin, input_line)) {
                // EOF or input error - stop processing
                break;
            }

            if (_stop_requested) {
                break;
            }

            // Parse input line into tokens
            std::istringstream iss(input_line);
            std::string token;
            inputs.clear();
            
            while (iss >> token) {
                inputs.push_back(token);
            }

            if (!inputs.empty()) {
                // Post command to IO context for processing
                _io_ctx.post([this, command_tokens = inputs]() {
                    try {
                        _notify_func(command_tokens);
                    } catch (const std::exception& e) {
                        if (_logger) {
                            _logger->error("Error processing input command: {}", e.what());
                        }
                    }
                });
            }

        } catch (const std::exception& e) {
            if (_logger) {
                _logger->error("Error in input processing loop: {}", e.what());
            }
            // Continue processing other inputs
        }
    }

    _running = false;
}

} // namespace ss
