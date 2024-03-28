#ifndef C43E8114_AAEE_43C0_B816_07ECAD6C5E83
#define C43E8114_AAEE_43C0_B816_07ECAD6C5E83


#include <ss_p2p/socket_manager.hpp>
#include <ss_p2p/message.hpp>
#include <json.hpp>

#include "boost/asio.hpp"


using namespace boost::asio;
using json = nlohmann::json;


namespace ss
{


class sender
{
public:
  
  template < typename SuccessHandler >
  void async_send( ip::udp::endpoint dest_ep, std::string param, json &payload, SuccessHandler handler )
  {
	message msg = message::request(_app_id);
	msg.set_param(param, payload);

	auto enc_msg = message::encode( msg );

	_sock_manager.self_sock().async_send_to(
		  boost::asio::buffer( enc_msg )
		  , dest_ep
		  , handler
		);
	
	#if SS_CAPTURE_PACKET
	std::cout << dest_ep << " (send)" << "\n";
	#endif
  }

  template < typename SuccessHandler >
  void async_send( ip::udp::endpoint dest_ep, message &msg, SuccessHandler handler )
  {
	auto enc_msg = message::encode( msg );

	_sock_manager.self_sock().async_send_to(
		  boost::asio::buffer( enc_msg )
		  , dest_ep
		  , handler
		);

	#if SS_CAPTURE_PACKET
	std::cout << dest_ep << " (send)" << "\n";
	#endif
  }

  bool sync_send( ip::udp::endpoint dest_ep, message &msg );
  bool sync_send( ip::udp::endpoint dest_ep, std::string param, json &payload );
  
  sender( udp_socket_manager &sock_manager, message::app_id id );
private:
  udp_socket_manager &_sock_manager;
  message::app_id _app_id;
};


};


#endif 


