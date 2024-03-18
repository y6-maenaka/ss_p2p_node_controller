#include <optional>

#include <ss_p2p/ice_agent/ice_agent.hpp>
#include <ss_p2p/ice_agent/signaling_server.hpp>
#include <ss_p2p/ice_agent/ice_observer_strage.hpp>
#include <ss_p2p/observer.hpp>



namespace ss
{
namespace ice
{


ice_agent::ice_agent( io_context &io_ctx, udp_socket_manager &sock_manager, ip::udp::endpoint &glob_self_ep, message::app_id id, ss::kademlia::direct_routing_table_controller &d_routing_table_controller ) :
  _sock_manager( sock_manager )
  , _glob_self_ep( glob_self_ep )
  , _app_id( id )
  , _ice_sender( sock_manager, glob_self_ep, id )
  , _obs_strage( io_ctx )
{
  return;
}

void ice_agent::income_msg( std::shared_ptr<message> msg )
{
  if( auto ice_param = msg->get_param("ice_agent"); ice_param == nullptr ) return; // 上レイヤで処理されているため,恐らくreturnされることはない
  ice_message ice_msg( *(msg->get_param("ice_agent")) );

  auto call_observer_income_message = [&]( auto &obs )
  {
	return obs.income_message( *msg );
  };

  const auto protocol = ice_msg.get_protocol();
  if( protocol == ice_message::protocol_t::signaling )
  {
	observer_id obs_id = ice_msg.get_param<observer_id>("observer_id");

	if( std::optional<observer<signaling_relay>> obs = _obs_strage.find_observer<signaling_relay>(obs_id); obs != std::nullopt ){
	  return call_observer_income_message(*obs); // relay_observerの検索
	}
	if( std::optional<observer<signaling_request>> obs = _obs_strage.find_observer<signaling_request>(obs_id); obs != std::nullopt ){
	  // request_observerの検索
	  return call_observer_income_message(*obs); // relay_observerの検索
	}
	if( std::optional<observer<signaling_response>> obs = _obs_strage.find_observer<signaling_response>(obs_id); obs != std::nullopt ){
	  // response_observerの検索
	  return call_observer_income_message(*obs); // relay_observerの検索
	}

	_sgnl_server->income_message( msg ); // シグナリングサーバに処理を投げる
  }

  else if( protocol == ice_message::protocol_t::stun )
  {
	return;
  }

  return;
}

ice_sender& ice_agent::get_ice_sender()
{
  return _ice_sender;
}

#if SS_DEBUG
ice_observer_strage &ice_agent::get_observer_strage()
{
  return _obs_strage;
}
#endif


};
};
