#ifndef C4E43B13_FAEA_4116_A00D_3D850F6C947E
#define C4E43B13_FAEA_4116_A00D_3D850F6C947E


#include <functional>
#include <array>
#include <limits>
#include <span>
#include <string>
#include <chrono>
#include <vector>

#include "./socket_manager.hpp"

#include "boost/asio.hpp"

using namespace boost::asio;

namespace ss
{

class udp_server
{
public:
  using recv_packet_handler = std::function<void(std::span<char>, ip::udp::endpoint&)>;
  udp_server( udp_socket_manager &sock_manager, io_context &io_ctx, const recv_packet_handler recv_handler );
  bool start();
  void stop();

private:
  std::array< char, std::numeric_limits<std::uint16_t>::max() > _recv_buff;
  boost::asio::ip::udp::endpoint _src_ep;

  void on_sock_read( const boost::system::error_code& ec, std::size_t bytes_transferred );
  void notice( const boost::system::error_code& ec, std::size_t bytes_transferred );

  udp_socket_manager& _sock_manager;
  io_context &_io_ctx; // sock_managerも共通のio_sockを持っているが一応

  recv_packet_handler const _recv_handler;
};


};


#endif 


