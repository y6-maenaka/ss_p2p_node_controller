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
#include <utils.hpp>

#include <iostream>
#include <vector>
#include <memory>
#include <string>

#include "boost/asio.hpp"


using namespace boost::asio;


int setup_ice_agent()
{
  ss::message::app_id id = {'a','b','c','d','e','f','g','h'};

  io_context io_context;
  deadline_timer d_timer(io_context);
  ip::udp::endpoint self_endpoint( ip::udp::v4(), 8100 );
  ss::udp_socket_manager sock_manager( self_endpoint, io_context );

  // ルーティングテーブルのセットアップ
  ss::kademlia::k_node self_k_node( self_endpoint );
  ss::kademlia::k_routing_table routing_table( self_k_node.get_id() );
  ss::kademlia::direct_routing_table_controller routing_table_controller( routing_table );

  ss::ice::ice_sender ice_sender( sock_manager, self_endpoint, id );

  // ice_agentのセットアップ
  std::shared_ptr<ss::ice::ice_agent> ice_agent = std::make_shared<ss::ice::ice_agent>( io_context, sock_manager, self_endpoint, id, routing_table_controller );



  std::cout << "setup_ice_agent done" << "\n";
  return 0;
}
