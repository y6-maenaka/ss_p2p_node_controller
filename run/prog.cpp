#include <iostream>
#include <fstream>
#include <optional>
#include <mutex>
#include <string>
#include <vector>
#include <condition_variable>

#include <utils.hpp>
#include <ss_p2p/node_controller.hpp>
#include <ss_p2p/kademlia/node_id.hpp>

#include "boost/asio.hpp"


using namespace boost::asio;


int main()
{
  std::cout << "Hello World" << "\n";

  /*
  ip::udp::endpoint ep_1( ip::address::from_string("0.0.0.0"), 8080 );
  ip::udp::endpoint ep_2( ip::address::from_string("127.0.0.1"), 8200 );
  auto id_1 = calc_node_id( ep_1 );
  auto id_2 = calc_node_id(ep_2);
  id_2 = id_1;
  id_1.print(); std::cout << "\n";
  id_2.print(); std::cout << "\n";
  */


  ip::udp::endpoint self_endpoint( ip::udp::v4(), 8080 );
  ss::node_controller n_controller( self_endpoint );
  n_controller.start();



  std::string boot_nodes_list_file_path = "../boot_node_lists.json";
  std::ifstream f_stream( boot_nodes_list_file_path );
  if( !f_stream.is_open() ){
	std::cerr << "boot_node_lists not found." << "\n";
	return -1;
  }

  json boot_nodes;
  f_stream >> boot_nodes;
  f_stream.close();

   
  std::vector<ip::udp::endpoint> stun_server_eps;
  stun_server_eps.push_back( ss::addr_pair_to_endpoint( boot_nodes[0]["ip"], boot_nodes[0]["port"] ) );


  auto global_ip = n_controller.sync_get_global_address( stun_server_eps );

  if( global_ip == std::nullopt ){
	std::cerr << "can't get global address" << "\n";
	return -1;
  }
  n_controller.update_global_self_endpoint( *global_ip );

  
  std::cout << "done" << "\n";


  std::mutex mtx;
  std::condition_variable cv;
  std::unique_lock<std::mutex> lock(mtx);
  cv.wait( lock );
}

