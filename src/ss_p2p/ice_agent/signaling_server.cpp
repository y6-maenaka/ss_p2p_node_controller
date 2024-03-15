#include <ss_p2p/ice_agent/signaling_server.hpp>


namespace ss
{
namespace ice
{


signaling_server::signaling_server( io_context &io_ctx, deadline_timer &d_timer, udp_socket_manager &sock_manager, direct_routing_table_controller &d_routing_table_controller ) : 
  _io_ctx( io_ctx ) , 
  _d_timer( d_timer ) ,
  _sock_manager( sock_manager ) ,
  _d_routing_table_controller( d_routing_table_controller )
{
  return;
}

void signaling_server::set_observer( const boost::system::error_code &ec )
{
  // signaling_open s_org_obs( _io_ctx, _d_timer, _d_routing_table_controller );
  // observer<signaling_open> s_obs( s_org_obs );
  observer<signaling_open> s_obs( _io_ctx, _d_timer, _d_routing_table_controller );
  return;
}

void signaling_server::async_hello( const boost::system::error_code &ec )
{
  std::cout << "非同期こんにちは" << "\n";
}

void signaling_server::signaling_send( ip::udp::endpoint &dest_ep, std::string root_param, const json payload, const boost::system::error_code &ec )
{
  ss::message::app_id id = {'a','b','c','d','e','f','g','h'}; // 仮
  ss::message msg(id);

  ss::message::encoded_message enc_msg = ss::message::encode( msg );

  #if SS_CAPTURE_PACKET
  std::cout << dest_ep << " : ";
  for( auto itr : enc_msg ) std::cout << itr;
  std::cout << "\n";
  #endif
 
  _sock_manager.self_sock().async_send_to(
		  boost::asio::buffer(enc_msg)
		  , dest_ep
		  , std::bind( &signaling_server::set_observer, this, std::placeholders::_1 )
	  );
}

signaling_server::s_send_func signaling_server::get_signaling_send_func()
{
  std::function<void(ip::udp::endpoint &dest_ep, std::string, const json payload, const boost::system::error_code &ec)> ret =
	std::bind( 
		&signaling_server::signaling_send
		, this
		, std::placeholders::_1
		, std::placeholders::_2
		, std::placeholders::_3
		, std::placeholders::_4
	  );

  return ret;
}



}; // namespace ice
}; // namespace ss 
