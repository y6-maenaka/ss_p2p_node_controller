#include <ss_p2p/kademlia/dht_manager.hpp>


namespace ss
{
namespace kademlia
{


dht_manager::dht_manager( boost::asio::io_context &io_ctx ) : _io_ctx(io_ctx)
{
  return; 
}

dht_manager::update_state_t dht_manager::handle_msg( std::shared_ptr<ss::message> msg, ip::udp::endpoint &ep )
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
}



}; // namespace kademlia
}; // namespace ss
