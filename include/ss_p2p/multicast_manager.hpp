#ifndef F3E546BD_6C47_421E_AA93_1BDB088BA9F0
#define F3E546BD_6C47_421E_AA93_1BDB088BA9F0


#include <vector>
#include <memory>

#include <ss_p2p/kademlia/k_routing_table.hpp>
#include <ss_p2p/peer.hpp>


namespace ss
{


class multicast_manager
{
public:
  using peers = std::vector<peer::ref>;
  peers get_multicast_target( std::size_t n );

private:
  kademlia::k_routing_table &_routing_table;
};


};


#endif 


