#include <ss_p2p/node_controller.hpp>


namespace ss
{


node_controller::node_controller( ip::udp::endpoint &ep ,const std::shared_ptr<io_context> io_ctx ) : 
  _d_timer(deadline_timer(*_core_io_ctx) ) ,
  _u_sock_manager( ep, *io_ctx ),
  _udp_server( std::make_shared<udp_server>(_u_sock_manager) )
{
  return;
}

void node_controller::start()
{
  std::thread daemon([&]()
	{
	  _d_timer.expires_from_now(boost::posix_time::seconds(DEFAULT_NODE_CONTROLLER_TICK_TIME_S));
	  _d_timer.async_wait( std::bind(&node_controller::tick, this , std::placeholders::_1) ); // node_controller::tickの起動

	  _core_io_ctx->run();
	});
  daemon.detach();

  #if SS_VERBOSE
  std::cout << "[ \x1b[32m stop \x1b[39m ] node controller" << "\n";
  #endif
}

void node_controller::stop()
{
  _core_io_ctx->stop();

  #if SS_VERBOSE
  std::cout << "[ \x1b[31m stop \x1b[39m ] node controller" << "\n";
  #endif
}

void node_controller::tick( const boost::system::error_code& ec )
{
  std::cout << "tick" << "\n";

  // _d_timer.expires_from_now(boost::posix_time::seconds(DEFAULT_NODE_CONTROLLER_TIME_S));
  // _d_timer.async_wait();
}

void node_controller::on_receive_packet()
{
  /*
  if( msg["kademlia_rpc"] )
  {
	// _dht_manager->handle( msg , peer );
  }

  if( msg['ice_agent'] )
  {
	// _ice_manager->handle( msg , peer );
  }
  */


  // long long index = messages.push();
  // peer.buffer.push( std::make_pair(message, index) );
}


};
