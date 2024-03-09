#ifndef F11E57F6_B4E1_4C58_BCD4_EF7EB00504DB
#define F11E57F6_B4E1_4C58_BCD4_EF7EB00504DB


#include <array>

#include <ss_p2p/endpoint.hpp>

#include "./node_id.hpp"

namespace ss
{
namespace kademlia
{



class k_node : public node_id
{
public:
  k_node( endpoint& ep );

private: 
  const endpoint &_endpoint;
  node_id _id;
};



};
};


#endif 


