#include <optional>

#include <ss_p2p/ice_agent/ice_agent.hpp>
#include <ss_p2p/observer.hpp>


namespace ss
{
namespace ice
{


ice_agent::ice_agent( io_context &io_ctx, udp_socket_manager &sock_manager, ip::udp::endpoint &glob_self_ep, message::app_id id, sender &sender, ss::kademlia::direct_routing_table_controller &d_routing_table_controller, ss_logger *logger ) :
  _sock_manager( sock_manager )
  , _glob_self_ep( glob_self_ep )
  , _app_id( id )
  , _sender( sender )
  , _ice_sender( sock_manager, glob_self_ep, id )
  , _obs_strage( io_ctx )
  , _sgnl_server( io_ctx, sender, _ice_sender, glob_self_ep, d_routing_table_controller, _obs_strage, logger )
  , _stun_server( io_ctx, sender, _ice_sender, d_routing_table_controller, _obs_strage, logger )
  , _logger(logger)
{
  return;
}

int ice_agent::income_message( std::shared_ptr<message> msg, ip::udp::endpoint &ep )
{
  if( auto ice_param = msg->get_param("ice_agent"); ice_param == nullptr ) return 0; // 上レイヤで処理されているため,恐らくreturnされることはない
  ice_message ice_msg( *(msg->get_param("ice_agent")) );

  auto call_observer_income_message = [&]( auto &obs )
  {
	return obs->income_message( *msg, ep );
  };

  const auto protocol = ice_msg.get_protocol();
  if( protocol == ice_message::protocol_t::signaling )
  {
	observer_id obs_id = ice_msg.get_observer_id();

	if( ice_observer_strage::found_observers<signaling_relay> obs_vec = _obs_strage.find_observer<signaling_relay>(obs_id); !(obs_vec.empty()) ){
	  // std::cout << "\x1b[33m" << "<ice observer strage> signaling_relay found" << "\n" << "\x1b[39m";
	  return call_observer_income_message(*obs_vec.begin()); // relay_observerの検索
	}
	if( ice_observer_strage::found_observers<signaling_request> obs_vec = _obs_strage.find_observer<signaling_request>(obs_id); !(obs_vec.empty()) ){
	  // std::cout << "\x1b[33m" << "<ice observer strage> signaling_request found" << "\n" << "\x1b[39m";
	  return call_observer_income_message(*obs_vec.begin()); // relay_observerの検索
	}
	if( ice_observer_strage::found_observers<signaling_response> obs_vec = _obs_strage.find_observer<signaling_response>(obs_id); !(obs_vec.empty()) ){
	  // std::cout << "\x1b[33m" << "<ice observer strage> signaling_response found" << "\n" << "\x1b[39m";
	  return call_observer_income_message(*obs_vec.begin()); // relay_observerの検索
	}

	/* #if SS_DEBUG
	std::cout << "\x1b[31m" << "(ice_observer_strage) observer not found" << "\x1b[39m" << "\n";
	#endif */

	_sgnl_server.income_message( msg, ep ); // シグナリングサーバに処理を投げる
  }

  else if( protocol == ice_message::protocol_t::stun )
  {
	observer_id obs_id = ice_msg.get_observer_id();

	if( std::vector< observer<binding_request>::ref > obs_vec = _obs_strage.find_observer<binding_request>(obs_id); !(obs_vec.empty()) ){
	  // std::cout << "\x1b[33m" << "<stun observer strage> binding_request found" << "\n" << "\x1b[39m";
	  return call_observer_income_message(*obs_vec.begin());
	}

	_stun_server.income_message( msg, ep);
	return 0;
  }

  return 0;
}

signaling_server::s_send_func ice_agent::get_signaling_send_func()
{
  return _sgnl_server.get_signaling_send_func();
}

void ice_agent::update_global_self_endpoint( ip::udp::endpoint &ep )
{
  ip::udp::endpoint __prev_global_self_ep = _glob_self_ep;

  _glob_self_ep = ep;

  #if SS_LOGGING_DISABLE
  _logger->log( logger:;log_level::INFO, "(@signaling_agent)", "update global self endpoint", endpoint_to_str(__prev_global_self_ep).c_str(), " -> ", endpoint_to_str(_glob_self_ep).c_str() );
  #endif
}

stun_server& ice_agent::get_stun_server()
{
  return _stun_server;
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
