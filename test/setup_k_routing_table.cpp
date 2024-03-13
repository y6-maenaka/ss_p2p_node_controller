#include <ss_p2p/kademlia/k_routing_table.hpp>
#include <ss_p2p/kademlia/dht_manager.hpp>
#include <ss_p2p/kademlia/node_id.hpp>
#include <ss_p2p/kademlia/k_bucket.hpp>
#include <ss_p2p/kademlia/k_node.hpp>
#include <utils.hpp>

#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>

#include "boost/asio.hpp"
#include "boost/uuid/uuid.hpp"
#include "boost/uuid/uuid_io.hpp"
#include "boost/uuid/uuid_generators.hpp"
#include "boost/lexical_cast.hpp"


using namespace boost::asio;
using namespace boost::uuids;


int setup_k_routing_table()
{
  ip::udp::endpoint self_endpoint( ip::udp::v4(), 8100 );
  // std::vector< ss::kademlia::k_node > nodes;

  ip::udp::endpoint peer_endpoint_1( ip::address::from_string("127.0.0.1"), 8090 );
  ss::kademlia::k_node k_node_1( peer_endpoint_1 ); 
  ip::udp::endpoint peer_endpoint_2( ip::address::from_string("127.0.0.4"), 8100 );
  ss::kademlia::k_node k_node_2( peer_endpoint_2 );
  ip::udp::endpoint peer_endpoint_3( ip::address::from_string("127.0.0.7"), 8110 );
  ss::kademlia::k_node k_node_3( peer_endpoint_3 );
  ip::udp::endpoint peer_endpoint_4( ip::address::from_string("127.0.0.7"), 8120 );
  ss::kademlia::k_node k_node_4( peer_endpoint_4 );
  ip::udp::endpoint peer_endpoint_5( ip::address::from_string("127.0.0.5"), 8130 );
  ss::kademlia::k_node k_node_5( peer_endpoint_5 );

  ss::kademlia::node_id self_id = ss::kademlia::calc_node_id( self_endpoint );
  ss::kademlia::k_routing_table routing_table( self_id );

  routing_table.calc_branch_index( k_node_1 );
  routing_table.calc_branch_index( k_node_2 );
  routing_table.calc_branch_index( k_node_3 );
  routing_table.calc_branch_index( k_node_4 );
  routing_table.calc_branch_index( k_node_5 );


  routing_table.auto_update( k_node_1 );
  /*
  routing_table.auto_update( k_node_2 );
  routing_table.auto_update( k_node_3 );
  routing_table.auto_update( k_node_4 );
  routing_table.auto_update( k_node_5 );
  */

  auto &bucket = routing_table.get_bucket( k_node_1 );
  bucket.print();
  std::cout << "---------------------------" << "\n";
  std::cout << bucket.swap_node( k_node_1, k_node_5 ) << "\n";
  std::cout << "---------------------------" << "\n";
  bucket.print();


  k_node_1 = k_node_2;
  k_node_1.print();
  k_node_2.print();

  /*
  std::mutex mtx;
  std::condition_variable cv;
  std::unique_lock<std::mutex> lock(mtx);
  cv.wait( lock );
  */

  return 0;
}
