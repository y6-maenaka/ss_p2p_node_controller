#include <ss_p2p/ice_agent/ice_agent.hpp>
#include <ss_p2p/ice_agent/ice_observer.hpp>
#include <ss_p2p/ice_agent/ice_observer_strage.hpp>
#include <ss_p2p/kademlia/k_node.hpp>
#include <ss_p2p/kademlia/node_id.hpp>
#include <ss_p2p/kademlia/k_observer.hpp>
#include <ss_p2p/kademlia/k_observer_strage.hpp>
#include <ss_p2p/kademlia/k_routing_table.hpp>
#include <ss_p2p/kademlia/rpc_manager.hpp>
#include <ss_p2p/kademlia/direct_routing_table_controller.hpp>
#include <ss_p2p/socket_manager.hpp>
#include <ss_p2p/message.hpp>
#include <ss_p2p/observer.hpp>
#include <ss_p2p/node_controller.hpp>
#include <utils.hpp>

#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <mutex>
#include <condition_variable>

#include "boost/asio.hpp"


using namespace boost::asio;


int setup_ice_agent()
{
  #if SS_DEBUG
  ss::message::app_id id = {'a','b','c','d','e','f','g','h'};

  io_context io_ctx;
  ip::udp::endpoint self_endpoint( ip::udp::v4(), 8110 );
  ss::udp_socket_manager sock_manager( self_endpoint, io_ctx );

  // ルーティングテーブルのセットアップ
  ss::kademlia::k_node self_k_node( self_endpoint );
  ss::kademlia::k_routing_table routing_table( self_k_node.get_id() );
  ss::kademlia::direct_routing_table_controller routing_table_controller( routing_table );


  ss::ice::ice_observer_strage obs_strage( io_ctx );
  obs_strage.show_state( boost::system::error_code{} );

  // ice_agentのセットアップ
  ss::ice::ice_agent ice_agent( io_ctx, sock_manager, self_endpoint, id, routing_table_controller );
  auto ice_sender = ice_agent.get_ice_sender();




  io_ctx.run();

  #endif

  std::cout << "done" << "\n";
  
  std::mutex mtx;
  std::condition_variable cv;
  std::unique_lock<std::mutex> lock(mtx);
  cv.wait( lock );


  std::cout << "setup_ice_agent done" << "\n";
  return 0;
}
