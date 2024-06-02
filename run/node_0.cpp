#include <string>
#include <chrono>
#include <mutex>
#include <condition_variable>

#include "openssl/evp.h"
#include "boost/asio.hpp"

#include "../test/setup_message_pool.cpp"
#include "../test/setup_peer.cpp"

using boost::asio::ip::udp;

int main( int argc, const char* argv[] )
{
  std::cout << "Hello node_0" << "\n";

  setup_message_pool();
  // setup_peer( argc, argv );
  // setup_k_bucket_iterator();
  // setup_ice_agent();
  // setup_ice_observer();
  // setup_k_routing_table();
  // setup_peer();
  // dummy_message();
  // setup_node_controller();
  // standby_signaling_relay_peer();

  std::mutex mtx;
  std::condition_variable cv;
  std::unique_lock<std::mutex> lock(mtx);
  cv.wait( lock );
  return 0;

  /*
  k_routing_table routing_table;
  node_controller node_controller;
  routing_table.hello();
  node_controller.greet();

  boost::asio::io_context io_context;
  boost::asio::ip::udp::socket socket(io_context, udp::endpoint(udp::v4(), 8080));
  udp::endpoint remote_endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 8090 );

  std::string message = "Hello World";
  socket.send_to(boost::asio::buffer(message), remote_endpoint);
  std::cout << "Message sent to " << remote_endpoint << ": " << message << std::endl;


  while(true)
  {
	std::array< char , 1024 >	recv_buffer;
	size_t len = socket.receive_from(boost::asio::buffer(recv_buffer), remote_endpoint);
	std::cout << std::string(recv_buffer.begin() + len ) << "\n";
  }
  */

}
