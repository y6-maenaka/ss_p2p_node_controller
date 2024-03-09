#ifndef C4E43B13_FAEA_4116_A00D_3D850F6C947E
#define C4E43B13_FAEA_4116_A00D_3D850F6C947E


#include <functional>
#include <array>
#include <limits>
#include <span>
#include <string>

#include "./socket_manager.hpp"

#include "boost/asio.hpp"

using namespace boost::asio;

namespace ss
{

class udp_server
{
public:
  udp_server( udp_socket_manager &sock_manager );
  void start();
  void stop();

private:
  std::array< char, std::numeric_limits<std::uint16_t>::max() >  _recv_buff;
  boost::asio::ip::udp::endpoint _src_ep;

  void on_receive( const boost::system::error_code& ec, std::size_t bytes_transferred );
  udp_socket_manager& _sock_manager;
};


};


#endif 


