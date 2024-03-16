#include <optional>

#include <ss_p2p/ice_agent/ice_agent.hpp>
#include <ss_p2p/ice_agent/signaling_server.hpp>
#include <ss_p2p/observer.hpp>



namespace ss
{
namespace ice
{


ice_agent::ice_agent( io_context &io_ctx, deadline_timer d_timer, udp_socket_manager &sock_manager, ip::udp::endpoint &glob_self_ep, message::app_id id, ss::kademlia::direct_routing_table_controller &d_routing_table_controller ) :
  _sock_manager( sock_manager )
  , _glob_self_ep( glob_self_ep )
  , _app_id( id )
  , _ice_sender( sock_manager, glob_self_ep, id )
{
  return;
}

void ice_agent::income_msg( std::shared_ptr<message> msg )
{
  if( auto ice_param = msg->get_param("ice_agent"); ice_param == nullptr ) return; // 上レイヤで処理されているため,恐らくreturnされることはない
  ice_message ice_msg( *(msg->get_param("ice_agent")) );

  const auto protocol = ice_msg.get_protocol();
  if( protocol == ice_message::protocol_t::req_signaling_open )
  {
	observer_id obs_id = ice_msg.get_param<observer_id>("observer_id");
	if( std::optional<observer<signaling_open>> &obs = _obs_strage.find_observer<signaling_open>(obs_id); obs != std::nullopt )
	{
	  return obs->income_message( *msg ); // observerがあれば優先する
	}

	_sgnl_server->handle_message( msg );
	return;
  }
  else if( protocol == ice_message::protocol_t::req_stun )
  {
	return;
  }

  return;
}

ice_agent::ice_sender& ice_agent::get_ice_sender()
{
  return _ice_sender;
}


ice_agent::ice_sender::ice_sender( udp_socket_manager &sock_manager, ip::udp::endpoint &glob_self_ep, message::app_id id ) :
  _sock_manager( sock_manager )
  , _glob_self_ep( glob_self_ep )
  , _app_id( id )
{
  return;
}

template < typename SuccessHandler >
void ice_agent::ice_sender::ice_send( ip::udp::endpoint &dest_ep, ice_message &ice_msg, SuccessHandler handler )
{
  message msg = message::request(_app_id);
  msg.set_param("ice_agent", ice_msg.encode() );

  std::span<char> enc_msg = message::encode( msg );

  _sock_manager.self_sock().async_send_to
	(
		boost::asio::buffer( enc_msg )	
		, dest_ep 
		, handler
	  );
  return;
}

ip::udp::endpoint &ice_agent::ice_sender::get_host_endpoint() const
{
  return _glob_self_ep;
}


};
};
