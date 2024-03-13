#include <ss_p2p/kademlia/rpc_manager.hpp>


namespace ss
{
namespace kademlia
{


rpc_manager::rpc_manager( node_id self_id ) :
  _self_id( self_id ) 
{
  _routing_table = std::make_shared<k_routing_table>( self_id );
  return;
}

union_observer_ptr rpc_manager::ping( k_node host_node, k_node swap_node, ip::udp::endpoint &ep ) 
{
  ss::kademlia::message k_msg = ss::kademlia::message::request();
  k_msg.set_param("rpc", "ping");
  const auto k_payload = k_msg.export_json();

  _send_func( ep , "kademlia", k_payload );
  return std::make_shared<union_observer>("ping", host_node, swap_node );
}

rpc_manager::update_context rpc_manager::incoming_request( std::shared_ptr<ss::kademlia::message> msg, ip::udp::endpoint &ep )
{
  update_context ret;
  return ret;
}

rpc_manager::update_context rpc_manager::incoming_response( std::shared_ptr<ss::kademlia::message> msg, ip::udp::endpoint &ep )
{
  update_context ret;
  return ret;
}


};
};

