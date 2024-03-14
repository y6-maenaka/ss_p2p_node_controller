#include <ss_p2p/kademlia/union_observer.hpp>
#include <ss_p2p/kademlia/observer.hpp>
#include <ss_p2p/kademlia/k_routing_table.hpp>
#include <ss_p2p/kademlia/k_node.hpp>

#include <iostream>
#include <vector>
#include <memory>
#include <string>

#include "boost/asio.hpp"


using namespace ss::kademlia;
using namespace boost::asio;


int setup_union_observer()
{
  io_context io_ctx;

  ip::udp::endpoint self_endpoint( ip::udp::v4(), 8100 );
  ss::kademlia::node_id self_id = ss::kademlia::calc_node_id( self_endpoint );
  ss::kademlia::k_routing_table routing_table( self_id );

  ip::udp::endpoint peer_endpoint_1( ip::address::from_string("127.0.0.1"), 8090 );
  ss::kademlia::k_node k_node_1( peer_endpoint_1 ); 
  ip::udp::endpoint peer_endpoint_2( ip::address::from_string("127.0.0.4"), 8100 );
  ss::kademlia::k_node k_node_2( peer_endpoint_2 );

  ping_observer p_obs( routing_table, k_node_1, k_node_2 );
  // union_observer u_obs( std::make_shared<p_obs> );
  union_observer_ptr u_obs_p = std::make_shared<union_observer>( "ping", routing_table, k_node_1, k_node_2 );


  u_obs_p->init( io_ctx );
  // u_obs_p->print();
  
  std::cout << "done" << "\n";
  return 0;
}
