/*
#include <ss_p2p/node_controller.hpp>
#include <ss_p2p/message.hpp>
#include <ss_p2p/message_pool.hpp>
#include <hash.hpp>
#include <utils.hpp>
*/
#include <ss_p2p/node_controller.hpp>
#include <ss_p2p/peer.hpp>
#include <ss_p2p/message_pool.hpp>

#include "boost/asio.hpp"

#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <memory>
#include <string>
#include <span>


class message_jank
{
public:
  void on_receive( ss::peer::ref peer_ref, ss::ss_message::ref msg_ref ){
	std::cout << "(" << msg_ref->meta.src_endpoint << ") : " << msg_ref->body.dump() << "\n";
  }
};


int setup_peer( int argc, const char *argv[] )
{
  message_jank mj;

  ip::udp::endpoint self_ep;
  ss::node_controller nc( self_ep );

  auto &msg_hub = nc.get_message_hub();
  msg_hub.start( std::bind( &message_jank::on_receive, mj, std::placeholders::_1, std::placeholders::_2 ) );


  /*
  std::span args( argv, argc );


  ip::udp::endpoint self_endpoint( ip::udp::v4(), 8100 );
  ss::node_controller node_controller( self_endpoint );
  node_controller.start();

  ip::udp::endpoint peer_endpoint( ip::address::from_string("127.0.0.1"), 9000 );
  ss::peer peer_1 = node_controller.get_peer( peer_endpoint );
  auto &msg_pool = node_controller.get_message_pool();
 
  std::this_thread::sleep_for( std::chrono::seconds(1) );
  */

  /* for(;;)
  {
	 auto received_message = peer_1.receive();
	 std::cout << "(" << peer_1.get_endpoint() << ") " << (received_message.first->get_param("messenger"))->dump() << "\n";
  } */

  /* auto &message_hub = msg_pool.get_message_hub();

  class message_jank mj;
  message_hub.start( std::bind( &message_jank::on_receive, mj, std::placeholders::_1 ) );

  std::mutex mtx;
  std::condition_variable cv;
  std::unique_lock<std::mutex> lock(mtx);
  cv.wait( lock ); */

  return 0;
}
