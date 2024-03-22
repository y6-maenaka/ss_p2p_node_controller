#ifndef A6BB07B7_B27C_4C21_97B9_7B2C325B4F43
#define A6BB07B7_B27C_4C21_97B9_7B2C325B4F43


#include <memory>
#include <unordered_map>

#include <ss_p2p/message.hpp>
#include <ss_p2p/endpoint.hpp>
#include <json.hpp>
#include <utils.hpp>

#include "./k_routing_table.hpp"
#include "./k_node.hpp"
#include "./node_id.hpp"
#include "./k_observer.hpp"
#include "./k_observer_strage.hpp"
#include "./rpc_manager.hpp"
#include "./k_message.hpp"

#include "boost/asio.hpp"

using namespace boost::asio;
using namespace boost::uuids;
using json = nlohmann::json;


namespace ss
{
namespace kademlia
{


class dht_manager
{
public:
  dht_manager( boost::asio::io_context &io_ctx , ip::udp::endpoint &ep );

  enum update_state_t{
	added,
	pending,
	error
  };

  void income_msg( std::shared_ptr<message> msg ); // メッセージ受信
  void invoke_msg( std::shared_ptr<message> msg ); // メッセージ送信
  void bootstrap();
  void handle_msg( json &msg, ip::udp::endpoint &ep );
  k_routing_table &get_routing_table();

  void update_global_self_endpoint( ip::udp::endpoint &ep );

private:
  io_context &_io_ctx;
  deadline_timer _tick_timer;
  ip::udp::endpoint &_self_ep;
  node_id _self_id;
  std::shared_ptr<rpc_manager> _rpc_manager;

  void tick();
  void call_tick();

};


};
};


#endif 


