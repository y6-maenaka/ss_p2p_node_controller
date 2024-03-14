#ifndef C93919D3_F914_40AA_9B56_FA0F3445F739
#define C93919D3_F914_40AA_9B56_FA0F3445F739


#include <functional>
#include <memory>
#include <string>

#include <ss_p2p/message.hpp>
#include <hash.hpp>
#include "./observer.hpp"
#include "./union_observer.hpp"
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
	enum update_state_t
	{
	  none ,
	  generate_obs ,
	  failure
	};
	enum rpc_t
	{
	  ping ,
	  find_node ,
	};
	update_state_t update_state;
	rpc_t rpc;
	union_observer_ptr obs_ptr;
	static update_context (error)() noexcept
	{
	  update_context ret;
	  ret.update_state = update_state_t::failure;
	  ret.obs_ptr = nullptr;
	  return ret;
	};
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
