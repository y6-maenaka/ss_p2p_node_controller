#include <ss_p2p/kademlia/k_routing_table.hpp>
#include <ss_p2p/kademlia/dht_manager.hpp>
#include <ss_p2p/kademlia/node_id.hpp>
#include <ss_p2p/kademlia/k_bucket.hpp>
#include <ss_p2p/kademlia/k_node.hpp>
#include <utils.hpp>

#include <vector>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>

#include "boost/asio.hpp"


using namespace boost::asio;


int setup_k_routing_table()
{
  ip::udp::endpoint self_endpoint( ip::udp::v4(), 8100 );
  // std::vector< ss::kademlia::k_node > nodes;

  ip::udp::endpoint peer_endpoint_1( ip::address::from_string("127.0.0.1"), 8090 );
  ss::kademlia::k_node k_node_1( peer_endpoint_1 );
  ip::udp::endpoint peer_endpoint_2( ip::address::from_string("127.0.0.2"), 8100 );
  ss::kademlia::k_node k_node_2( peer_endpoint_2 );
  ip::udp::endpoint peer_endpoint_3( ip::address::from_string("127.0.0.3"), 8110 );
  ss::kademlia::k_node k_node_3( peer_endpoint_3 );
  ip::udp::endpoint peer_endpoint_4( ip::address::from_string("127.0.0.4"), 8120 );
  ss::kademlia::k_node k_node_4( peer_endpoint_4 );
  ip::udp::endpoint peer_endpoint_5( ip::address::from_string("127.0.0.5"), 8130 );
  ss::kademlia::k_node k_node_5( peer_endpoint_5 );

  ss::kademlia::node_id self_id = ss::kademlia::calc_node_id( self_endpoint );
  ss::kademlia::k_routing_table routing_table( self_id );

  // nodes.push_back( k_node_1 );

  routing_table.calc_branch_index( peer_endpoint_1 );
  routing_table.calc_branch_index( peer_endpoint_2 );
  routing_table.calc_branch_index( peer_endpoint_3 );
  routing_table.calc_branch_index( peer_endpoint_4 );
  routing_table.calc_branch_index( peer_endpoint_5 );


  routing_table.auto_update( peer_endpoint_1 );
  routing_table.auto_update( peer_endpoint_2 );
  routing_table.auto_update( peer_endpoint_2 );
  routing_table.auto_update( peer_endpoint_3 );
  routing_table.auto_update( peer_endpoint_4 );
  routing_table.auto_update( peer_endpoint_5 );

   

  /*
  std::mutex mtx;
  std::condition_variable cv;
  std::unique_lock<std::mutex> lock(mtx);
  cv.wait( lock );
  */

  return 0;
}
