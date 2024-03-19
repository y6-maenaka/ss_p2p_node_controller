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
#include <ss_p2p/socket_manager.hpp>
#include <ss_p2p/udp_server.hpp>
#include <utils.hpp>
#include <json.hpp>

#include <iostream>
#include <vector>
#include <span>
#include <memory>
#include <string>
#include <mutex>
#include <condition_variable>

#include "boost/asio.hpp"


using namespace boost::asio;
using json = nlohmann::json;


void show_buff( std::span<char> buff, ip::udp::endpoint &src_ep )
{
  for( auto itr : buff ) std::cout << itr << "\n";
  std::cout << "\n";
  std::cout << src_ep << "\n";
}


int setup_ice_agent()
{
  #if SS_DEBUG

  


  ss::message::app_id id = {'a','b','c','d','e','f','g','h'};

  std::shared_ptr<io_context> io_ctx = std::make_shared<io_context>();
  ip::udp::endpoint self_endpoint( ip::udp::v4(), 8090 );
  ss::udp_socket_manager sock_manager( self_endpoint, *io_ctx );

  // 受信用サーバ スタート
  ss::udp_server udp_server( sock_manager, *io_ctx, show_buff );
  udp_server.start();


  // ルーティングテーブルのセットアップ
  ss::kademlia::k_node self_k_node( self_endpoint );
  ss::kademlia::k_routing_table routing_table( self_k_node.get_id() );
  ss::kademlia::direct_routing_table_controller routing_table_controller( routing_table );


  // ダミーのピアをルーティングテーブルに追加
  for( int i=0; i<20; i++ )
  {
	ip::udp::endpoint dummy_ep = ss::generate_random_endpoint();
	ss::kademlia::k_node dummy_k_node(dummy_ep);
	routing_table.auto_update( dummy_k_node );
  }


  // ice_agentのセットアップ
  ss::ice::ice_agent i_agent( *io_ctx, sock_manager, self_endpoint, id, routing_table_controller );
  auto s_send_func = i_agent.get_signaling_send_func();
  
  ip::udp::endpoint local_dest_ep( ip::address::from_string("127.0.0.1"), 8100 );
  json payload; payload["greet"] = "Hello World";
  s_send_func( local_dest_ep, "body", payload, boost::system::error_code{} );

  auto& ice_observer_strage = i_agent.get_observer_strage();
  ice_observer_strage.show_state( boost::system::error_code{} );


  io_ctx->run();

  #endif

  std::cout << "done" << "\n";
  
  std::mutex mtx;
  std::condition_variable cv;
  std::unique_lock<std::mutex> lock(mtx);
  cv.wait( lock );


  std::cout << "setup_ice_agent done" << "\n";
  return 0;
}
