#include <ss_p2p/kademlia/k_bucket.hpp>
#include <ss_p2p/kademlia/k_routing_table.hpp>

#include "boost/asio.hpp"


using namespace boost::asio;


int setup_k_bucket_iterator()
{

  ip::udp::endpoint self_endpoint( ip::udp::v4(), 8000 );
  ss::kademlia::k_node self_k_node( self_endpoint );

  ss::kademlia::k_routing_table routing_table( self_k_node.get_id() );

  auto bucket_itr = routing_table.get_begin_bucket_iterator();

  // bucket_itr._print_();

  return 0;
}
