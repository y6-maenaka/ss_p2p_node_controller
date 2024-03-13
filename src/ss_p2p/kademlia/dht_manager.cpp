#include <ss_p2p/kademlia/dht_manager.hpp>


namespace ss
{
namespace kademlia
{


dht_manager::dht_manager( boost::asio::io_context &io_ctx, ip::udp::endpoint ep ) :
  _io_ctx(io_ctx) , 
  _self_ep(ep) 
{
  _self_id = calc_node_id( ep );
  _rpc_manager = std::make_shared<rpc_manager>( _self_id );

  #if SS_VERBOSE 
  std::cout << "[\x1b[32m start \x1b[39m] dht manager" << "\n";
  std::cout << "dht manager hosting with :: "; _self_id.print(); std::cout << "\n";
  #endif

  return; 
}

void dht_manager::handle_msg( json &msg, ip::udp::endpoint &ep )
{
  std::shared_ptr<ss::kademlia::message> k_msg = std::make_shared<ss::kademlia::message>(msg);

  // 必ずメッセージは検証する
  if( bool flag = k_msg->validate(); !flag ) return;
  if( k_msg->is_request() ) // handle request
  {
	auto update_ctx = _rpc_manager->incoming_request( k_msg, ep );
	// observer_ptr->setup( io_context &io_ctx );
  }
  else // handle response
  {
	auto update_ctx = _rpc_manager->incoming_response( k_msg, ep );
  }

}

/* dht_manager::update_state_t dht_manager::handle_msg( std::shared_ptr<ss::message> msg, ip::udp::endpoint &ep )
{
  ss::kademlia::message k_msg( *(msg->get_param("kademlia")) );
  if( !(k_msg.validate()) ) return update_state_t::error;

  if( k_msg.is_request() )
  {
  }
  else 
  {

  }

  return update_state_t::error;
} */


}; // namespace kademlia
}; // namespace ss
