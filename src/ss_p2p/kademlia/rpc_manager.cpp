#include <ss_p2p/kademlia/rpc_manager.hpp>
#include <ss_p2p/kademlia/k_message.hpp>
#include <utils.hpp>


namespace ss
{
namespace kademlia
{


rpc_manager::rpc_manager( node_id &self_id, io_context &io_ctx, k_observer_strage &obs_strage, s_send_func &send_func ) :
  _self_id( self_id )  
  , _io_ctx( io_ctx ) 
  , _tick_timer( io_ctx )
  , _obs_strage( obs_strage )
  , _s_send_func( send_func )
  , _routing_table( self_id )
  , _d_routing_table_controller( _routing_table )
{
  return;
}

/* std::shared_ptr< observer<ping> > rpc_manager::ping( k_node host_node, k_node swap_node, ip::udp::endpoint &ep ) 
{
  k_message k_msg = k_message::_request_( k_message::rpc::ping );
  const auto k_payload = k_msg.export_json();

  _send_func( ep , "kademlia", k_payload );
  return nullptr;
} */

void rpc_manager::ping_request( ip::udp::endpoint ep, on_pong_handler pong_handler, on_ping_timeout_handler timeout_handler )
{
  observer<ping> ping_obs( _io_ctx, ep, pong_handler, timeout_handler );

  k_message k_msg = k_message::_request_( k_message::rpc::ping );
  k_msg.set_observer_id( ping_obs.get_id() );

  _s_send_func( ep, "kademlia", k_msg.encode() );

  ping_obs.init();
  _obs_strage.add_observer( ping_obs ); // ストアする
}

void rpc_manager::find_node_request( std::vector<ip::udp::endpoint> request_eps, std::vector<ip::udp::endpoint> ignore_eps, on_find_node_response_handler response_handler )
{
  observer<find_node> find_node_obs( _io_ctx , response_handler );
  find_node_obs.init();

  k_message k_msg = k_message::_request_( k_message::rpc::find_node );
  auto msg_controller = k_msg.get_find_node_message_controller();
  msg_controller.set_ignore_endpoint( ignore_eps );

  for( auto itr : request_eps )
  {
	_s_send_func( itr, "kademlia", k_msg.encode() );
  }
  _obs_strage.add_observer( find_node_obs );
}

void rpc_manager::ping_response( k_message &k_msg, ip::udp::endpoint &ep )
{
  #if SS_VERBOSE
  std::cout << "ping response -> " << ep << "\n";
  #endif  

  k_msg.set_message_type(k_message::message_type::response); // レスポンスに変更

  _s_send_func( ep, "kademlia", k_msg.encode() );  // レスポンス送信
}

void rpc_manager::find_node_response( k_message &k_msg, ip::udp::endpoint &ep )
{
  k_msg.set_message_type(k_message::message_type::response);
  auto msg_controller = k_msg.get_find_node_message_controller();
  auto routing_table_controller = direct_routing_table_controller( _routing_table );

  auto eps = routing_table_controller.collect_endpoint( ep, DEFAULT_FIND_NODE_SIZE );
  msg_controller.set_finded_endpoint( eps );

  _s_send_func( ep, "kademlia", k_msg.encode() ); // レスポンス送信
}

int rpc_manager::income_request( k_message &k_msg, ip::udp::endpoint &ep )
{
  // 処理
  const auto rpc = k_msg.get_rpc();

  switch( rpc )
  {
	case k_message::rpc::ping :
	  {
		this->ping_response( k_msg, ep );
		break;
	  }
	case k_message::rpc::find_node : 
	  {
		this->find_node_response( k_msg, ep );
		break;
	  }
  }
  
  return 0;
}

int rpc_manager::income_response( k_message &k_msg, ip::udp::endpoint &ep )
{
  // 基本的にobserverで捌かれる
  return 0;
}

int rpc_manager::income_message( std::shared_ptr<message> msg, ip::udp::endpoint &ep )
{
  if( auto k_param = msg->get_param("kademlia"); k_param == nullptr ) return 0;
  k_message k_msg( *(msg->get_param("kademlia")) );

  auto message_type = k_msg.get_message_type();

  switch( message_type )
  {
	case k_message::message_type::request :
	  {
		return this->income_request( k_msg, ep );
	  }
	case k_message::message_type::response :
	  {
		return this->income_response( k_msg, ep );
	  }
	default :
	  {
		return 0;
	  }
  }
  return 0;
}

k_routing_table &rpc_manager::get_routing_table()
{
  return _routing_table;
}

direct_routing_table_controller &rpc_manager::get_direct_routing_table_controller()
{
  return _d_routing_table_controller;
}

void rpc_manager::update_self_id( node_id &id )
{
  _self_id = id;
}


};
};

