#ifndef D879B588_6D8D_42D0_8E22_4078BD445DA5
#define D879B588_6D8D_42D0_8E22_4078BD445DA5


#include <vector>
#include <string>
#include <span>
#include <unordered_map>
#include <thread>
#include <iostream>

#include "boost/asio.hpp"

#include "./kademlia/dht_manager.hpp"
#include "./peer.hpp"
#include "./udp_server.hpp"
#include "./message.hpp"
#include "./message_pool.hpp"
#include "./socket_manager.hpp"

using namespace boost::asio;


namespace ss
{


constexpr std::time_t DEFAULT_NODE_CONTROLLER_TICK_TIME_S = 30; // 時間周期処理


class node_controller
{
public:
  const ip::udp::socket &self_sock();
  void tick( const boost::system::error_code &ec ); // 未使用ピアノ削除, on_tickのセット, 新しいpeerへの接続要求

  peer get_peer();

  template <typename Func, typename ... Args>
  void sync_call( Func f, Args ... a );

  template < typename Func, typename ... Args>
  void async_call( Func f, Args ... a );

  void requires_routing( bool b );

  void on_receive_packet();

  node_controller( ip::udp::endpoint &self_ep ,const std::shared_ptr<io_context> io_ctx = nullptr );
  void start();
  void stop();

private:
  udp_socket_manager _u_sock_manager;
  std::shared_ptr< udp_server > const _udp_server;

  std::shared_ptr<kademlia::dht_manager> const _dht_manager;
  std::shared_ptr<io_context> const _core_io_ctx;
  boost::asio::deadline_timer _d_timer;
  
  message_pool const _msg_pool;
};


};


#endif 
