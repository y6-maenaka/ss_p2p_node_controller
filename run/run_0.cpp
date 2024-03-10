#include "run_0.hpp"

#include <ss_p2p/kademlia/k_routing_table.hpp>
#include <ss_p2p/node_controller.hpp>
#include <string>
#include <chrono>
#include <mutex>
#include <condition_variable>

#include "openssl/evp.h"
#include "boost/asio.hpp"

#include "../test/setup_node_controller.cpp"
#include "../test/dummy_message.cpp"

using boost::asio::ip::udp;

int main()
{
  std::cout << "Hello" << "\n";

  dummy_message();
  //setup_node_controller();
  

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
