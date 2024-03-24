#include <ss_p2p/node_controller.hpp>
#include <ss_p2p/message.hpp>
#include <hash.hpp>
#include <utils.hpp>

#include "boost/asio.hpp"

#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <memory>
#include <string>
#include <span>


int setup_peer()
{


  ip::udp::endpoint self_endpoint( ip::udp::v4(), 8100 );
  ss::node_controller node_controller( self_endpoint );
  node_controller.start();

  ip::udp::endpoint peer_endpoint( ip::address::from_string("127.0.0.1"), 8090 );
  ss::peer peer_1 = node_controller.get_peer( peer_endpoint );

  
   for(;;)
  {
	// std::shared_ptr<ss::message> received_message = peer_1.receive();
  }


  std::cout << "setup_peer done" << "\n";
  return 10;
}
