#ifndef A2D6CD42_AB67_4275_9644_FC5F69116594
#define A2D6CD42_AB67_4275_9644_FC5F69116594


#include <ss_p2p/endpoint.hpp>

#include "boost/asio.hpp"


namespace ss
{
namespace kademlia
{


constexpr unsigned short K_NODE_ID_LENGTH = 160 / 8 ;


class node_id
{
public:
  using id = std::array< std::uint8_t , K_NODE_ID_LENGTH >;

  bool operator ==( const node_id &ni ) const;
  static node_id (none)() noexcept;
};


};
};

#endif  


