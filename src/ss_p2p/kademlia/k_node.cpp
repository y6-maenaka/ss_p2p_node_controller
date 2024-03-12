#include <ss_p2p/kademlia/k_node.hpp>


namespace ss
{
namespace kademlia
{


k_node::k_node( ip::udp::endpoint &ep ) : 
  _ep(ep)
{
  _id = calc_node_id(_ep);
  
  /*
  #if SS_VERBOSE
  std::cout << "k_node generate with :: "; _id.print(); std::cout << "\n";
  #endif
  */
}

k_node::k_node( ip::udp::endpoint &ep, node_id id ) :
  _ep( ep ) ,
  _id( id )
{
  return;  
}

bool k_node::operator==( const k_node &kn ) const
{
  return (_id == kn._id);
}
bool k_node::operator!=( const k_node &kn ) const
{
  return (_id != kn._id);
}

void k_node::print() const
{
  std::cout << _ep << "\n";
  _id.print(); std::cout << "\n";
}


};
};
