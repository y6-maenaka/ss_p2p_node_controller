#include <ss_p2p/ice_agent/ice_agent.hpp>
#include <ss_p2p/ice_agent/stun_server.hpp>
#include <ss_p2p/node_controller.hpp>
#include "boost/asio.hpp"

#include <memory>
#include <mutex>
#include <vector>
#include <thread>
#include <chrono>
#include <condition_variable>


using namespace boost::asio;


int setup_stun_client()
{
  std::shared_ptr<io_context> io_ctx = std::make_shared<io_context>();
  ip::udp::endpoint self_ep( ip::udp::v4(), 8200 );

  ip::udp::endpoint stun_server_ep( ip::address::from_string("127.0.0.1"), 8300 );


  ss::node_controller n_controller( self_ep, io_ctx );
  n_controller.start();
  std::this_thread::sleep_for( std::chrono::seconds(1) ); // buff

  auto &ice_agent = n_controller.get_ice_agent();
  auto &stun_server = ice_agent.get_stun_server();
  auto &ice_obs_strage = ice_agent.get_observer_strage();
  auto &routing_table = n_controller.get_routing_table();
  
  std::vector<ip::udp::endpoint> target_eps;
  target_eps.push_back( stun_server_ep );
  auto ctx = stun_server.sync_binding_request( target_eps );
  std::cout << ctx.get_state() << "\n";

  
  ice_obs_strage.show_state( boost::system::error_code{} );


  std::mutex mtx;
  std::condition_variable cv;
  std::unique_lock<std::mutex> lock(mtx);
  cv.wait(lock);


  return 0;
}
