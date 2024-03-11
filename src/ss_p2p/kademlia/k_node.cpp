#include <ss_p2p/kademlia/k_node.hpp>


namespace ss
{
namespace kademlia
{


k_node::k_node( ip::udp::endpoint &ep, node_id id ) :
  _ep(ep) ,
  _id(id)
{
  
}


};
};
