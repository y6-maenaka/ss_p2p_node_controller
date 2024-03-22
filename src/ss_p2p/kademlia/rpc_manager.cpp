#include <ss_p2p/kademlia/rpc_manager.hpp>
#include <ss_p2p/kademlia/k_observer_strage.hpp>
#include <ss_p2p/kademlia/k_message.hpp>
#include <utils.hpp>


namespace ss
{
namespace kademlia
{


rpc_manager::rpc_manager( node_id &self_id, io_context &io_ctx ) :
  _self_id( self_id )  
  , _io_ctx( io_ctx ) 
  , _tick_timer( io_ctx )
  ,  _obs_strage( io_ctx )
{
  _routing_table = std::make_shared<k_routing_table>( self_id );
}

std::shared_ptr< observer<ping> > rpc_manager::ping( k_node host_node, k_node swap_node, ip::udp::endpoint &ep ) 
{
  k_message k_msg = k_message::request();
  k_msg.set_param("rpc", "ping");
  const auto k_payload = k_msg.export_json();

  _send_func( ep , "kademlia", k_payload );
  return nullptr;
}

rpc_manager::update_context::update_context()
{
  return;
}

rpc_manager::update_context rpc_manager::income_request( std::shared_ptr<k_message> msg, ip::udp::endpoint &ep )
{
  // 処理
  k_routing_table::update_state update_state;

  switch( update_state )
  {
	case (k_routing_table::update_state::overflow) :
	{
	  k_node host_node(ep);
	  k_node swap_node;
	  if( auto ret = _routing_table->get_node_front( host_node ); ret.size() > 0 ) swap_node = ret.at(0);
	  else return update_context::error();

	  // u_obs_ptr = this->ping( host_node, *swap_node, ep );
	  break;
	}
	default :
	{
	  break;
	}
  }
  update_context ret;
  return ret;
}

rpc_manager::update_context rpc_manager::income_response( std::shared_ptr<k_message> msg, ip::udp::endpoint &ep )
{
  update_context ret;

  std::string rpc; // = msg->get_param<std::string>("rpc");
  // base_observer::id obs_id = str_to_observer_id( msg->get_param<std::string>("observer_id") );
  if( rpc == "ping" ){
	// auto obs = _obs_strage.get_observer<ping>( rpc, obs_id );
	// obs.handle_msg( msg );
  }
  else if( rpc == "find_node" ){
	// auto obs = _obs_strage.get_observer<find_node>( rpc, obs_id );
	// obs.handle_msg( msg );
  }


  return ret;
}

k_routing_table &rpc_manager::get_routing_table()
{
  return *_routing_table;
}

void rpc_manager::update_self_id( node_id &id )
{
  _self_id = id;
}


};
};

