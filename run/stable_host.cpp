#include <ss_p2p/node_controller.hpp>

#include "boost/asio.hpp"

#include <mutex>
#include <condition_variable>
#include <memory>
#include <span>


using namespace boost::asio;


int main( int argc, const char* argv[] )
{
  std::span args( argv, argc );
  std::shared_ptr<io_context> io_ctx = std::make_shared<io_context>();

  ip::udp::endpoint self_ep( ip::udp::v4(), std::atoi(args[0]) );
  ss::node_controller n_controller( self_ep, io_ctx );


  n_controller.start();






  std::mutex mtx;
  std::condition_variable cv;
  std::unique_lock<std::mutex> lock(mtx);
  cv.wait( lock );
}

