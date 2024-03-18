#include <ss_p2p/ice_agent/ice_sender.hpp>


namespace ss
{
namespace ice
{

ice_sender::ice_sender( udp_socket_manager &sock_manager, ip::udp::endpoint &glob_self_ep, message::app_id id ) :
  _sock_manager( sock_manager )
  , _glob_self_ep( glob_self_ep )
  , _app_id( id )
{
  return;
}

void ice_sender::send_done( const boost::system::error_code &ec )
{
  #if SS_VERBOSE
  std::cout << "nat traversal send done" << "\n";
  #endif
}

ip::udp::endpoint &ice_sender::get_self_endpoint() const
{
  return _glob_self_ep;
}


};
};