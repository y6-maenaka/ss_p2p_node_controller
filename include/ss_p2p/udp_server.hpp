#pragma once

#include "./network/impl/udp_transport.hpp"
#include "./network/transport_factory.hpp"
#include "./core/interfaces.hpp"
#include "./core/result.hpp"
#include "./core/types.hpp"
#include <logger/logger.hpp>

#include <functional>
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <boost/asio.hpp>

namespace ss {

/**
 * @brief Legacy UDP server adapter for backward compatibility
 * 
 * This class wraps the new network::udp_transport implementation to maintain
 * API compatibility with existing code while providing modern error handling,
 * thread safety, and RAII resource management.
 * 
 * Thread Safety: All public methods are thread-safe
 * Exception Safety: Strong guarantee - operations either succeed completely or have no effect
 */
class udp_server final : public ss::core::i_component {
public:
    /// Legacy packet handler signature for backward compatibility
    using recv_packet_handler = std::function<void(std::vector<std::uint8_t>, boost::asio::ip::udp::endpoint&)>;
    
    /// Result type for operations that can fail
    template<typename T = void>
    using result = ss::core::result<T, std::string>;

    /**
     * @brief Constructor
     * @param local_endpoint Local endpoint to bind to
     * @param io_context IO context for async operations
     * @param recv_handler Message receive handler
     * @param logger Logger instance (optional)
     * @throws std::invalid_argument if recv_handler is null
     */
    explicit udp_server(
        const boost::asio::ip::udp::endpoint& local_endpoint,
        ss::core::io_context& io_context,
        recv_packet_handler recv_handler,
        logger* logger = nullptr
    );

    /**
     * @brief Destructor - automatically stops server and cleans up resources
     */
    ~udp_server() noexcept override;

    // Non-copyable, movable
    udp_server(const udp_server&) = delete;
    udp_server& operator=(const udp_server&) = delete;
    udp_server(udp_server&&) = delete;
    udp_server& operator=(udp_server&&) = delete;

    // ss::core::i_component interface
    std::string name() const noexcept override { return "udp_server_legacy_adapter"; }
    std::string version() const noexcept override { return "2.0.0"; }
    bool is_healthy() const noexcept override;
    std::string status() const override;
    ss::core::async_void initialize() override;
    ss::core::async_void cleanup() override;
    ss::core::async_void start() override;
    ss::core::async_void stop() override;
    bool is_running() const noexcept override;

    /**
     * @brief Start the UDP server (legacy API)
     * @return true on success, false on failure
     * @note This method is kept for backward compatibility
     */
    bool start_legacy();

    /**
     * @brief Stop the UDP server (legacy API)
     * @note This method is kept for backward compatibility
     */
    void stop_legacy();

    /**
     * @brief Get server statistics
     * @return Transport statistics
     */
    ss::network::transport_stats get_stats() const noexcept;

    /**
     * @brief Reset server statistics
     */
    void reset_stats() noexcept;

    /**
     * @brief Get local endpoint
     * @return Local endpoint the server is bound to
     */
    boost::asio::ip::udp::endpoint local_endpoint() const noexcept;

    /**
     * @brief Check if server can receive messages
     * @return true if ready to receive, false otherwise
     */
    bool can_receive() const noexcept;

    /**
     * @brief Get current receive queue size
     * @return Number of messages in receive queue
     */
    std::uint32_t receive_queue_size() const noexcept;

private:
    /// IO context reference
    ss::core::io_context& io_context_;
    
    /// Underlying UDP transport
    std::unique_ptr<ss::network::impl::udp_transport> transport_;
    
    /// Transport factory for creating transport instance
    std::unique_ptr<ss::network::transport_factory> transport_factory_;
    
    /// Legacy message handler
    recv_packet_handler recv_handler_;
    
    /// Logger instance
    logger* logger_;
    
    /// Local endpoint
    boost::asio::ip::udp::endpoint local_endpoint_;
    
    /// Running state
    std::atomic<bool> running_;
    
    /// Initialization state
    std::atomic<bool> initialized_;
    
    /// Thread safety mutex
    mutable std::mutex mutex_;

    /**
     * @brief Handle incoming messages from transport
     * @param message Incoming message
     */
    void handle_incoming_message(const ss::network::incoming_message& message);

    /**
     * @brief Convert transport endpoint to legacy endpoint
     * @param endpoint Transport endpoint
     * @return Legacy Boost.Asio endpoint
     */
    boost::asio::ip::udp::endpoint convert_endpoint(
        const ss::core::endpoint& endpoint
    ) const noexcept;

    /**
     * @brief Convert legacy endpoint to transport endpoint
     * @param endpoint Legacy Boost.Asio endpoint
     * @return Transport endpoint
     */
    ss::core::endpoint convert_endpoint(
        const boost::asio::ip::udp::endpoint& endpoint
    ) const noexcept;

    /**
     * @brief Log message with appropriate level
     * @param level Log level
     * @param message Log message
     */
    void log(logger::log_level level, const std::string& message) const noexcept;

    /**
     * @brief Log packet information
     * @param level Log level
     * @param direction Packet direction
     * @param endpoint Remote endpoint
     */
    void log_packet(
        logger::log_level level,
        const std::string& direction,
        const boost::asio::ip::udp::endpoint& endpoint
    ) const noexcept;
};

} // namespace ss