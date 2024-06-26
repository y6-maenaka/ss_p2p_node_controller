#ifndef A2D6CD42_AB67_4275_9644_FC5F69116594
#define A2D6CD42_AB67_4275_9644_FC5F69116594


#include <vector>
#include <span>
#include <algorithm>
#include <iostream>

#include "boost/asio.hpp"


namespace ss
{
namespace kademlia
{


constexpr unsigned short K_NODE_ID_LENGTH = 160 / 8 ;
using namespace boost::asio;


class node_id
{
public:
  using value_type = std::uint8_t;
  using id = std::array< std::uint8_t , K_NODE_ID_LENGTH >;
  id _id;

  node_id();
  node_id( const void *from );
  node_id( const node_id &nid );
  
  std::string to_str() const;
  bool operator==( const node_id &nid ) const;
  id operator()() const;
  // node_id operator=( const node_id &nid ) const;
  unsigned char operator[](unsigned short idx) const;
  static node_id (none)() noexcept;

   void print() const;
};


node_id calc_node_id( ip::udp::endpoint &ep );
unsigned short calc_node_xor_distance( const node_id &nid_1, const node_id &nid_2 );
node_id str_to_node_id( std::string from );


};
};

#endif  


