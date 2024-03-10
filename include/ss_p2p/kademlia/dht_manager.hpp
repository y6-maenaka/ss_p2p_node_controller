#ifndef A6BB07B7_B27C_4C21_97B9_7B2C325B4F43
#define A6BB07B7_B27C_4C21_97B9_7B2C325B4F43


#include <memory>
#include <unordered_map>

#include <ss_p2p/kademlia/k_routing_table.hpp>
#include <ss_p2p/kademlia/k_node.hpp>
#include <ss_p2p/kademlia/observer.hpp>
#include <ss_p2p/message.hpp>
#include <ss_p2p/endpoint.hpp>
#include <json.hpp>

#include "./rpc_manager.hpp"
#include "./message.hpp"

#include "boost/asio.hpp"

using namespace boost::asio;
using json = nlohmann::json;


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
  
  dht_manager( boost::asio::io_context &io_ctx );

  enum update_state_t{
	added,
	pending,
	error
  };
  update_state_t handle_msg( std::shared_ptr<ss::message> msg, ip::udp::endpoint &ep );
private:
  void find_node();
  io_context &_io_ctx;

  // const rpc_manager _rpc_manager;
  std::unordered_multimap< std::uint16_t, observer_ptr > observer_ptrs;
};


};
};


#endif 


