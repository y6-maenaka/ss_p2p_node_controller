#ifndef C93919D3_F914_40AA_9B56_FA0F3445F739
#define C93919D3_F914_40AA_9B56_FA0F3445F739


#include <functional>
#include <memory>
#include <string>

#include <hash.hpp>
#include <ss_p2p/observer.hpp>
#include "./k_observer.hpp"
#include "./k_observer_strage.hpp"
#include "./node_id.hpp"
#include "./k_routing_table.hpp"

#include "boost/asio.hpp"


using namespace boost::asio;


namespace ss
{
namespace kademlia
{


class k_observer_strage;
  

class rpc_manager
{
public:
  rpc_manager( node_id &self_id, io_context &io_ctx );

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
  update_context income_request( std::shared_ptr<ss::kademlia::k_message> msg, ip::udp::endpoint &ep );
  update_context income_response( std::shared_ptr<ss::kademlia::k_message> msg, ip::udp::endpoint &ep );
  void handle_msg( json &k_msg );
  k_routing_table &get_routing_table();
  void update_self_id( node_id &id );

private:
  void set_triger_observer();

  std::shared_ptr< observer<class ping> > ping( k_node host_node, k_node swap_node, ip::udp::endpoint &ep );

  const std::function<void(ip::udp::endpoint &ep, std::string, const json payload )> _send_func;
  const std::function<void(ip::udp::endpoint &ep, std::string, const json payload )> _traversal_send_func;

  std::shared_ptr<k_routing_table> _routing_table;

  node_id &_self_id;
  io_context &_io_ctx;
  deadline_timer _tick_timer;
  k_observer_strage _obs_strage;
};



};
};


#endif
