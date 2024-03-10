#include <ss_p2p/node_controller.hpp>

#include "boost/asio.hpp"

#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>

using namespace boost::asio;

int setup_node_controller()
{
  std::cout << __FUNCTION__ << " started \n";

  ip::udp::endpoint self_endpoint( ip::udp::v4(), 8100 );
  ss::node_controller node_controller( self_endpoint );
  node_controller.start();

  std::mutex mtx;
  std::condition_variable cv;
  std::unique_lock<std::mutex> lock(mtx);
  cv.wait( lock );


  std::cout << __FUNCTION__ << " finished \n";
  return 10;
}
