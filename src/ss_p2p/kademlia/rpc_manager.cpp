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

union_observer_ptr rpc_manager::ping( k_node host_node, k_node swap_node, ip::udp::endpoint &ep ) 
{
  ss::kademlia::message k_msg = ss::kademlia::message::request();
  k_msg.set_param("rpc", "ping");
  const auto k_payload = k_msg.export_json();

  _send_func( ep , "kademlia", k_payload );
  return nullptr;
  // return std::make_shared<union_observer>( "ping", *_routing_table, _io_ctx, _d_timer, host_node, swap_node );
}

rpc_manager::update_context rpc_manager::incoming_request( std::shared_ptr<ss::kademlia::message> msg, ip::udp::endpoint &ep )
{
  // 処理
  k_routing_table::update_state update_state;
  union_observer_ptr u_obs_ptr = nullptr;

  switch( update_state )
  {
	case (k_routing_table::update_state::overflow) :
	{
	  k_node host_node(ep);
	  std::shared_ptr<k_node> swap_node = _routing_table->get_front(host_node); // 対象のバケット先頭要素を取得
	  if( swap_node == nullptr ) return update_context::error();
	  u_obs_ptr = this->ping( host_node, *swap_node, ep );
	  break;
	}
	default :
	{
	  break;
	}
  }
  update_context ret;
  ret.obs_ptr = u_obs_ptr;
  return ret;
}

rpc_manager::update_context rpc_manager::incoming_response( std::shared_ptr<ss::kademlia::message> msg, ip::udp::endpoint &ep )
{
  update_context ret;
  return ret;
}


};
};

