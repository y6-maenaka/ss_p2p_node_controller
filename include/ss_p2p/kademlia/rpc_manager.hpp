#ifndef C93919D3_F914_40AA_9B56_FA0F3445F739
#define C93919D3_F914_40AA_9B56_FA0F3445F739


#include <functional>
#include <memory>
#include <string>

#include <hash.hpp>
#include <ss_p2p/message.hpp>
#include <ss_p2p/observer.hpp>
#include "./k_observer.hpp"
#include "./k_observer_strage.hpp"
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
  rpc_manager( node_id self_id, io_context &io_ctx, deadline_timer &d_timer );

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
	static update_context (error)() noexcept
	{
	  update_context ret;
	  ret.update_state = update_state_t::failure;
	  return ret;
	};
	update_context();
  };
  update_context incoming_request( std::shared_ptr<ss::kademlia::message> msg, ip::udp::endpoint &ep );
  update_context incoming_response( std::shared_ptr<ss::kademlia::message> msg, ip::udp::endpoint &ep );
  void handle_msg( json &k_msg );

private:
  void set_triger_observer();

  std::shared_ptr< observer<ping> > ping( k_node host_node, k_node swap_node, ip::udp::endpoint &ep );
  // union_observer_ptr find_node();
  // union_observer_ptr join();

  const std::function<void(ip::udp::endpoint &ep, std::string, const json payload )> _send_func;
  const std::function<void(ip::udp::endpoint &ep, std::string, const json payload )> _traversal_send_func;

  std::shared_ptr<k_routing_table> _routing_table;

  node_id _self_id;
  io_context &_io_ctx;
  deadline_timer &_d_timer;
  k_observer_strage _obs_strage;
};



};
};


#endif
