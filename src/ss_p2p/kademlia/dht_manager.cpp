#include <ss_p2p/kademlia/dht_manager.hpp>


namespace ss
{
namespace kademlia
{


dht_manager::dht_manager( boost::asio::io_context &io_ctx, ip::udp::endpoint &ep ) :
  _io_ctx(io_ctx)
  , _self_ep(ep)
  , _tick_timer(io_ctx)
  , _self_id( calc_node_id(ep) )
{
  // _self_id = calc_node_id( ep );
  _rpc_manager = std::make_shared<rpc_manager>( _self_id, _io_ctx );

  #if SS_VERBOSE 
  std::cout << "[\x1b[32m start \x1b[39m] dht manager" << "\n";
  std::cout << "dht manager hosting with :: "; _self_id.print(); std::cout << "\n";
  #endif

  return; 
}

void dht_manager::handle_msg( json &msg, ip::udp::endpoint &ep )
{
  std::shared_ptr<ss::kademlia::k_message> k_msg = std::make_shared<ss::kademlia::k_message>(msg);

  // 必ずメッセージは検証する
  if( bool flag = k_msg->validate(); !flag ) return;
  if( k_msg->is_request() ) // handle request
  {
	auto update_ctx = _rpc_manager->income_request( k_msg, ep );
	// observer_ptr->setup( io_context &io_ctx );
  }
  else // handle response
  {
	auto update_ctx = _rpc_manager->income_response( k_msg, ep );
  }
  return;
}

k_routing_table &dht_manager::get_routing_table()
{
  return _rpc_manager->get_routing_table();
}

void dht_manager::update_global_self_endpoint( ip::udp::endpoint &ep )
{
  ip::udp::endpoint __prev_self_ep = _self_ep;
  node_id __prev_self_id = _self_id;

  _self_ep = ep;
  _self_id = calc_node_id(ep); // 自身のidの更新

  #if SS_VERBOSE 
  std::cout << "\x1b[33m" << "[dht_manager] update global self endpoint" << "\n" << "\x1b[39m"; 
  std::cout << __prev_self_ep << " -> " << _self_ep << "\n";
  __prev_self_id.print(); std::cout << " -> "; _self_id.print(); std::cout << "\n";
  std::cout << "\n";
  #endif

  _rpc_manager->update_self_id( _self_id ); // 配下rpc_managerの更新
}


}; // namespace kademlia
}; // namespace ss
