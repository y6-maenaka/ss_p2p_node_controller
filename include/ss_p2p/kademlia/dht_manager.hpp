#ifndef A6BB07B7_B27C_4C21_97B9_7B2C325B4F43
#define A6BB07B7_B27C_4C21_97B9_7B2C325B4F43


#include <memory>
#include <unordered_map>

#include <ss_p2p/kademlia/k_routing_table.hpp>
#include <ss_p2p/kademlia/k_node.hpp>
#include <ss_p2p/kademlia/observer.hpp>

#include "./rpc_manager.hpp"


namespace ss
{
namespace kademlia
{


class dht_manager
{
public:
  struct dht_update_context
  {
	enum state_t
	{
	  success,
	  pending
	};
	state_t _state;
  };

  void bootstrap();

  void add_node();
  
  dht_manager();
private:
  void find_node();

  // const rpc_manager _rpc_manager;
  std::unordered_multimap< std::uint16_t, observer_ptr > observer_ptrs;
};


};
};


#endif 


