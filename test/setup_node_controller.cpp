#include <ss_p2p/node_controller.hpp>
#include <ss_p2p/kademlia/k_node.hpp>

#include "boost/asio.hpp"

#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>

using namespace boost::asio;

int setup_node_controller()
{

  ip::udp::endpoint self_endpoint( ip::udp::v4(), 8100 );
  ss::node_controller node_controller( self_endpoint );


  auto &routing_table = node_controller.get_routing_table();
  ip::udp::endpoint peer_1_endpoint( ip::address::from_string("127.0.0.1"), 9000 );
  k_node peer_1_k_node(peer_1_endpoint);

  routing_table.auto_update( peer_1_endpoint );


  node_controller.start();
  

  std::mutex mtx;
  std::condition_variable cv;
  std::unique_lock<std::mutex> lock(mtx);
  cv.wait( lock );


  std::cout << __FUNCTION__ << " finished \n";
  return 10;
}
