#include <ss_p2p/kademlia/rpc_manager.hpp>


namespace ss
{
namespace kademlia
{


rpc_manager::rpc_manager( node_id self_id ) :
  _self_id( self_id ) 
{
  _routing_table = std::make_shared<k_routing_table>( self_id );
  return;
}


};
};

