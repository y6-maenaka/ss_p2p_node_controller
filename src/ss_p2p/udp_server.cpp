#include <ss_p2p/udp_server.hpp>


namespace ss
{


udp_server::udp_server( udp_socket_manager &sock_manager, io_context &io_ctx, const recv_packet_handler recv_handler ) :
  _sock_manager(sock_manager) ,
  _io_ctx(io_ctx) ,
  _recv_handler(recv_handler)
{
  return;
}

bool udp_server::start()
{
  // ブートストラップする
  if( !(_sock_manager.self_sock().is_open()) ) return false;
  this->call_receiver();

 #if SS_VERBOSE
  std::cout << "\n";
  std::cout << "[\x1b[32m start \x1b[39m] udp server" << "\n";
  std::cout << "\n";
  #endif 

  return true;
}

void udp_server::call_receiver()
{
  _sock_manager.self_sock().async_receive_from
	(
	  boost::asio::buffer( _recv_buff )
	  , _src_ep
	  , std::bind(
		&udp_server::call_income_message_handler
		, this
		, std::placeholders::_1
		, std::placeholders::_2 
	  )
	);
}

void udp_server::call_income_message_handler( const boost::system::error_code &ec, std::size_t bytes_transferred )
{
  if( ec ){
	return call_receiver();
  }

  std::vector<std::uint8_t> raw_msg; raw_msg.reserve( bytes_transferred );
  std::copy( _recv_buff.begin(), _recv_buff.begin() + bytes_transferred, std::back_inserter(raw_msg) );
  _io_ctx.post([this, raw_msg]()
  {
	this->_recv_handler( raw_msg, this->_src_ep );
  });

  return call_receiver();
}

void udp_server::stop()
{
  _sock_manager.self_sock().cancel();
 #if SS_VERBOSE
  std::cout << "[\x1b[31m stop \x1b[39m] udp server" << "\n";
  #endif 
}


};
