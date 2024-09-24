#include <ss_p2p/node_controller.hpp>
#include <ss_p2p/kademlia/dht_manager.hpp>
#include <ss_p2p/ice_agent/ice_agent.hpp>
#include <ss_p2p/kademlia/k_node.hpp>
#include <utils.hpp>

#include "boost/asio.hpp"

#include <json.hpp>
#include <utils.hpp>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <span>
#include <fstream>
#include <vector>
#include <iostream>

#include "./test_messenger_server.hpp"

using namespace boost::asio;
using json = nlohmann::json;


int setup_node_controller( int argc, const char* argv[] )
{
  std::span args( argv, argc );

  std::string boot_nodes_file_path = "../boot_node_lists.json";
  std::ifstream f_stream( boot_nodes_file_path );
  if( !f_stream.is_open() ){
	std::cerr << "boot_node_lists not found." << "\n";
	return -1;
  }
  json boot_nodes_j;
  f_stream >> boot_nodes_j;
  f_stream.close();

  std::vector<ip::udp::endpoint> boot_nodes;
  for( auto itr : boot_nodes_j ) boot_nodes.push_back( ss::addr_pair_to_endpoint( itr["ip"], itr["port"] ) );

  ip::udp::endpoint self_endpoint( ip::udp::v4(), std::atoi(args[1]) );
  ss::node_controller n_controller( self_endpoint );

  auto &k_observer_strage = n_controller.get_dht_manager().get_observer_strage();
  auto &ice_observer_strage = n_controller.get_ice_agent().get_observer_strage();
  auto &direct_routing_table_controller = n_controller.get_direct_routing_table_controller();
  direct_routing_table_controller.auto_update_batch( boot_nodes );
  auto &message_hub = n_controller.get_message_hub();

  n_controller.start( boot_nodes );

  message_server msg_server;
  message_hub.start( std::bind( &message_server::on_receive_message, msg_server, std::placeholders::_1, std::placeholders::_2) );

  // ice_observer_strage.show_state( boost::system::error_code{} );
  // k_observer_strage.show_state( boost::system::error_code{} );

  std::mutex mtx;
  std::condition_variable cv;
  std::unique_lock<std::mutex> lock(mtx);
  cv.wait( lock );


  std::cout << __FUNCTION__ << " finished \n";
  return 10;
}
