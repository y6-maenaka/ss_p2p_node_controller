#include <ss_p2p/kademlia/k_routing_table.hpp>


int setup_k_routing_table()
{
  ip::udp::endpoint self_endpoint( ip::udp::v4(), 8100 );

  ip::udp::endpoint peer_endpoint_1( ip::address::from_string("127.0.0.1"), 8090 );
  ip::udp::endpoint peer_endpoint_2( ip::address::from_string("127.0.0.2"), 8100 );
  ip::udp::endpoint peer_endpoint_3( ip::address::from_string("127.0.0.3"), 8110 );
  ip::udp::endpoint peer_endpoint_4( ip::address::from_string("127.0.0.4"), 8120 );
  ip::udp::endpoint peer_endpoint_5( ip::address::from_string("127.0.0.5"), 8130 );

  ss::kademlia::k_routing_table routing_table( self_endpoint );
  routing_table.calc_branch( peer_endpoint_1 );
  routing_table.calc_branch( peer_endpoint_2 );
  routing_table.calc_branch( peer_endpoint_3 );
  routing_table.calc_branch( peer_endpoint_4 );
  routing_table.calc_branch( peer_endpoint_5 );

  return 0;
}
