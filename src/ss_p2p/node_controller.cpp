#include <ss_p2p/node_controller.hpp>


namespace ss
{


node_controller::node_controller( ip::udp::endpoint &self_ep, std::shared_ptr<io_context> io_ctx ) :
  _core_io_ctx( io_ctx ) ,
  _u_sock_manager( self_ep, std::ref(*io_ctx) ) ,
  _d_timer( *io_ctx )
{
  const std::function<void(std::span<char>, ip::udp::endpoint&)> recv_handler = std::bind(
	  &node_controller::on_receive_packet,
	  this,
	  std::placeholders::_1,
	  std::placeholders::_2
	  );
  _udp_server = std::make_shared<udp_server>( _u_sock_manager, *_core_io_ctx, recv_handler );
  return;
}

void node_controller::start()
{
  std::thread daemon([&]()
	{
	  assert( _udp_server != nullptr );
	  _udp_server->start();

	  call_tick(); // tickを呼び出す

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

  #if SS_VERBOSE
  std::cout << "[\x1b[31m stop \x1b[39m] node controller" << "\n";
  #endif
}

void node_controller::tick( const boost::system::error_code& ec )
{
  std::cout << "tick" << "\n";
  
  this->call_tick();
}

void node_controller::call_tick()
{
  _d_timer.expires_from_now(boost::posix_time::seconds(DEFAULT_NODE_CONTROLLER_TICK_TIME_S));
  _d_timer.async_wait( std::bind(&node_controller::tick, this , std::placeholders::_1) ); // node_controller::tickの起動
}

void node_controller::on_receive_packet( std::span<char> raw_msg, ip::udp::endpoint &ep )
{
  std::cout << "パケットを受信しました" << "\n";
  std::shared_ptr<message> msg = std::make_shared<message>(raw_msg);
  if( msg == nullptr ) return;

  if( msg->is_contain_param("kademlia") )
  {
	// _dht_manager->handle( msg , peer );
  }

  if( msg->is_contain_param("ice_agent") )
  {
	// _ice_manager->handle( msg , peer );
  }


  // long long index = messages.push();
  // peer.buffer.push( std::make_pair(message, index) );
}


};
