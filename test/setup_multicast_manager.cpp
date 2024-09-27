#include <ss_p2p/multicast_manager.hpp>
#include <ss_p2p/node_controller.hpp>


int setup_multicast_manager( int argc, const char* argv[] )
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


  auto multicast_manager = n_controller.get_multicast_manager();


  std::cout << "setup multicast manager" << "\n";
  return 0;
}
