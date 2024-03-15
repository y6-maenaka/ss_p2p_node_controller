#include <ss_p2p/kademlia/rpc_manager.hpp>


namespace ss
{
namespace kademlia
{


rpc_manager::rpc_manager( node_id self_id, io_context &io_ctx, deadline_timer &d_timer ) :
  _self_id( self_id ) ,
  _io_ctx( io_ctx ) ,
  _d_timer( d_timer )
{
  _routing_table = std::make_shared<k_routing_table>( self_id );
}

std::shared_ptr< observer<ping> > rpc_manager::ping( k_node host_node, k_node swap_node, ip::udp::endpoint &ep ) 
{
  ss::kademlia::message k_msg = ss::kademlia::message::request();
  k_msg.set_param("rpc", "ping");
  const auto k_payload = k_msg.export_json();

  _send_func( ep , "kademlia", k_payload );
  return nullptr;
  // return std::make_shared< observer<ping_observer> >( *_routing_table, _io_ctx, _d_timer, host_node, swap_node );
}

rpc_manager::update_context rpc_manager::incoming_request( std::shared_ptr<ss::kademlia::message> msg, ip::udp::endpoint &ep )
{
  // 処理
  k_routing_table::update_state update_state;

  switch( update_state )
  {
	case (k_routing_table::update_state::overflow) :
	{
	  k_node host_node(ep);
	  std::shared_ptr<k_node> swap_node = nullptr;
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

rpc_manager::update_context rpc_manager::incoming_response( std::shared_ptr<ss::kademlia::message> msg, ip::udp::endpoint &ep )
{
  update_context ret;

  std::string rpc = msg->get_param("rpc");
  base_observer::id obs_id = str_to_observer_id( msg->get_param("observer_id") );
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


};
};

