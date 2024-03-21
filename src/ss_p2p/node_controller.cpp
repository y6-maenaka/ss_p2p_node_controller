#include <ss_p2p/node_controller.hpp>


namespace ss
{


node_controller::node_controller( ip::udp::endpoint &self_ep, std::shared_ptr<io_context> io_ctx ) :
   _self_ep(self_ep)
  , _core_io_ctx( io_ctx ) 
  , _u_sock_manager( self_ep, std::ref(*io_ctx) ) 
  , _tick_timer( *io_ctx ) 
  , _msg_pool( *io_ctx, true )
{
  const std::function<void(std::span<char>, ip::udp::endpoint&)> recv_handler = std::bind(
	  &node_controller::on_receive_packet,
	  this,
	  std::placeholders::_1,
	  std::placeholders::_2
	  );
  _udp_server = std::make_shared<udp_server>( _u_sock_manager, *_core_io_ctx, recv_handler );
  _dht_manager = std::make_shared<dht_manager>( *io_ctx, _self_ep );

  _d_routing_table_controller = std::make_shared<kademlia::direct_routing_table_controller>( get_routing_table() );
  _ice_agent = std::make_shared<ice::ice_agent>( *_core_io_ctx, _u_sock_manager, self_ep/*一旦*/, _id, *_d_routing_table_controller );
}

udp_socket_manager &node_controller::get_socket_manager()
{
  return _u_sock_manager;
}

ice::ice_agent &node_controller::get_ice_agent()
{
  return *_ice_agent;
}

kademlia::dht_manager &node_controller::get_dht_manager()
{
  return *_dht_manager;
}

kademlia::k_routing_table &node_controller::get_routing_table()
{
  return _dht_manager->get_routing_table();
}

void node_controller::start()
{
  std::thread daemon([&]()
	{
	  assert( _udp_server != nullptr );
	  _udp_server->start();

	  call_tick(); // tickを呼び出す
	  _msg_pool.requires_refresh(true); // msg_poolの定期リフレッシュを停止する

	  _core_io_ctx->run();
	});
  daemon.detach();

  #if SS_VERBOSE
  std::cout << "[\x1b[32m start \x1b[39m] node controller" << "\n";
  #endif
}

void node_controller::stop()
{
  _udp_server->stop();
  _core_io_ctx->stop();
  _msg_pool.requires_refresh(false); // msg_poolの定期リフレッシュを再開する

  #if SS_VERBOSE
  std::cout << "[\x1b[31m stop \x1b[39m] node controller" << "\n";
  #endif
}

peer node_controller::get_peer( ip::udp::endpoint &ep )
{
  peer ret( ep, _msg_pool.get_symbolic(ep) );
  return ret;
}

void node_controller::tick( const boost::system::error_code& ec )
{
  std::cout << "tick" << "\n";
  
  this->call_tick();
}

void node_controller::call_tick()
{
  _tick_timer.expires_from_now(boost::posix_time::seconds(DEFAULT_NODE_CONTROLLER_TICK_TIME_S));
  _tick_timer.async_wait( std::bind(&node_controller::tick, this , std::placeholders::_1) ); // node_controller::tickの起動
}

void node_controller::on_receive_packet( std::span<char> raw_msg, ip::udp::endpoint &ep )
{
  int flag = 1;
  std::shared_ptr<message> msg = std::make_shared<message>( message::decode(raw_msg) );
  if( msg == nullptr ) return;

  if( msg->is_contain_param("kademlia") )
  {
	// int state = _dht_manager->handle_msg( msg , ep );
  }

  if( msg->is_contain_param("ice_agent") )
  {
	flag = _ice_agent->income_message( msg, ep );
  }
 
  if( flag == 1 ){
	_msg_pool.store( msg, ep ); // メッセージの保存
  }
}


};
