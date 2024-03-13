#include <ss_p2p/kademlia/k_node.hpp>


namespace ss
{
namespace kademlia
{

  
k_node::k_node( ip::udp::endpoint &ep ) : 
  _ep(ep) , 
  _id( calc_node_id(ep) )
{
  /*
  #if SS_VERBOSE
  std::cout << "k_node generate with :: "; _id.print(); std::cout << "\n";
  #endif
  */
  return;
}

k_node::k_node( const k_node &kn ) :
  _ep(kn._ep)  ,
  _id(kn._id)
{
  return;
}

ip::udp::endpoint k_node::get_endpoint()
{
  return _ep;
}

node_id& k_node::get_id() 
{
  return _id;
}

k_node k_node::operator=( const k_node &kn ) const
{
  return *this;
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
