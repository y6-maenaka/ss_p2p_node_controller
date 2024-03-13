#ifndef C93919D3_F914_40AA_9B56_FA0F3445F739
#define C93919D3_F914_40AA_9B56_FA0F3445F739


#include <functional>
#include <memory>
#include <string>

#include <ss_p2p/message.hpp>
#include <hash.hpp>
#include "./observer.hpp"
#include "./message.hpp"
#include "./node_id.hpp"
#include "./k_routing_table.hpp"

#include "boost/asio.hpp"


using namespace boost::asio;


namespace ss
{
namespace kademlia
{


class rpc_manager
{
public:
  rpc_manager( node_id self_id );

  struct update_context
  {
	enum update_state
	{
	  generate_obs ,
	  error
	};
	enum observer_handle_t
	{
	  ignore , // 特に何もしなくて良い
	};
	observer_ptr obs_ptr;

	update_context() : obs_ptr(nullptr){};
  };
  update_context incoming_request( std::shared_ptr<ss::kademlia::message> msg, ip::udp::endpoint &ep );
  update_context incoming_response( std::shared_ptr<ss::kademlia::message> msg, ip::udp::endpoint &ep );
  void handle_msg( json &k_msg );

private:
  void set_triger_observer();

  union_observer_ptr ping( k_node host_node, k_node swap_node, ip::udp::endpoint &ep );
  union_observer_ptr find_node();
  union_observer_ptr join();

  union_observer_ptr handle_observer( observer_ptr op );

  const std::function<void(ip::udp::endpoint &ep, std::string, const json payload )> _send_func;
  std::shared_ptr<k_routing_table> _routing_table;
  node_id _self_id;
};



};
};


#endif
