#ifndef F11E57F6_B4E1_4C58_BCD4_EF7EB00504DB
#define F11E57F6_B4E1_4C58_BCD4_EF7EB00504DB


#include <array>

#include <ss_p2p/endpoint.hpp>
#include "./node_id.hpp"

#include "boost/asio.hpp"


using namespace boost::asio;


namespace ss
{
namespace kademlia
{



class k_node : public node_id
{
public:
  k_node( ip::udp::endpoint &ep, node_id id );

private: 
  node_id _id;
  ip::udp::endpoint _ep;
};


};
};


#endif 


