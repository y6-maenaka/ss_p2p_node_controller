#ifndef A6BB07B7_B27C_4C21_97B9_7B2C325B4F43
#define A6BB07B7_B27C_4C21_97B9_7B2C325B4F43


#include <memory>
#include <unordered_map>
#include <functional>

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
  using s_send_func = std::function<void(ip::udp::endpoint&, std::string, json)>;
  dht_manager( boost::asio::io_context &io_ctx , ip::udp::endpoint &ep );

  enum update_state_t{
	added,
	pending,
	error
  };

  void income_msg( std::shared_ptr<message> msg ); // メッセージ受信
  void invoke_msg( std::shared_ptr<message> msg ); // メッセージ送信
  void bootstrap();

  int income_message( std::shared_ptr<message> msg, ip::udp::endpoint &ep );
  k_routing_table &get_routing_table();
  direct_routing_table_controller &get_direct_routing_table_controller();
  rpc_manager &get_rpc_manager();

  void update_global_self_endpoint( ip::udp::endpoint &ep );
  void init( s_send_func send_func ); 

  #if SS_DEBUG
  k_observer_strage &get_observer_strage();
  #endif

private:
  io_context &_io_ctx;
  deadline_timer _tick_timer;
  ip::udp::endpoint &_self_ep;
  node_id _self_id;
  rpc_manager _rpc_manager;
  k_observer_strage _obs_strage;
  s_send_func _s_send_func; // initにより初期化され

  void tick();
  void call_tick();

};


};
};


#endif 


