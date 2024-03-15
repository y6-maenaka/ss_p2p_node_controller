#include <ss_p2p/kademlia/direct_routing_table_controller.hpp>


namespace ss
{
namespace kademlia
{


direct_routing_table_controller::direct_routing_table_controller( k_routing_table &routing_table ) : 
  _routing_table( routing_table )
{
  return;
}

std::vector< std::shared_ptr<k_node> > direct_routing_table_controller::collect_nodes( k_node &root_node, std::size_t max_count, const std::vector<k_node*> &ignore_nodes )
{
 return _routing_table.collect_nodes( root_node, max_count, ignore_nodes );
}

bool direct_routing_table_controller::is_exist( k_node &kn ) const 
{
  return _routing_table.is_exist(kn);
}


};
};
