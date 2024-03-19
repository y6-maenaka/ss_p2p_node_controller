#ifndef D879B588_6D8D_42D0_8E22_4078BD445DA5
#define D879B588_6D8D_42D0_8E22_4078BD445DA5


#include <vector>
#include <string>
#include <span>
#include <unordered_map>
#include <thread>
#include <iostream>
#include <cassert>
#include <mutex>
#include <condition_variable>

#include "boost/asio.hpp"

#include <ss_p2p/kademlia/dht_manager.hpp>
#include <ss_p2p/kademlia/direct_routing_table_controller.hpp>
#include <ss_p2p/ice_agent/ice_agent.hpp>
#include <ss_p2p/peer.hpp>
#include <ss_p2p/udp_server.hpp>
#include <ss_p2p/message.hpp>
#include <ss_p2p/message_pool.hpp>
#include <ss_p2p/socket_manager.hpp>

using namespace boost::asio;


namespace ss
{


constexpr std::time_t DEFAULT_NODE_CONTROLLER_TICK_TIME_S = 30/*[s]*/; // 時間周期処理


class node_controller
{
public:
  const message::app_id _id = {'a','b','c','d','e','f','g','h'};
  const ip::udp::socket &self_sock();

  template <typename Func, typename ... Args>
  void sync_call( Func f, Args ... a );

  template < typename Func, typename ... Args>
  void async_call( Func f, Args ... a );

  void requires_routing( bool b );
  void on_receive_packet( std::span<char> raw_msg , ip::udp::endpoint &ep );

  void start();
  void stop();
  peer get_peer( ip::udp::endpoint &ep );
  node_controller( ip::udp::endpoint &self_ep , std::shared_ptr<io_context> io_ctx = std::make_shared<boost::asio::io_context>() );
  udp_socket_manager& get_socket_manager();
  kademlia::k_routing_table &get_routing_table();
  #if SS_DEBUG
  ice::ice_agent &get_ice_agent();
  kademlia::dht_manager &get_dht_manager();
  #endif

protected:
  void tick( const boost::system::error_code &ec ); // 未使用ピアノ削除, on_tickのセット, 新しいpeerへの接続要求
  void call_tick();

  std::shared_ptr<ss::kademlia::direct_routing_table_controller> _d_routing_table_controller;
private:
  ip::udp::endpoint _self_ep;
  udp_socket_manager _u_sock_manager;
  std::shared_ptr< udp_server > _udp_server;
  std::shared_ptr<ice::ice_agent> _ice_agent;
  std::shared_ptr<kademlia::dht_manager> _dht_manager;
  std::shared_ptr<io_context> _core_io_ctx;
  boost::asio::deadline_timer _tick_timer;

  message_pool _msg_pool;
};


};


#endif 
