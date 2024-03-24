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
  _sock_manager.self_sock().async_receive_from(
		boost::asio::buffer( _recv_buff ),
		_src_ep,
		std::bind( &udp_server::on_sock_read, this, std::placeholders::_1 , std::placeholders::_2 )
	  );

 #if SS_VERBOSE
  std::cout << "\n";
  std::cout << "[\x1b[32m start \x1b[39m] udp server" << "\n";
  std::cout << "\n";
  #endif 

  return true;
}

void udp_server::on_sock_read( const boost::system::error_code& ec, std::size_t bytes_transferred )
{
  #if SS_CAPTURE_PACKET
  std::cout << "(recv) " << _src_ep << "\n";
  #endif

   std::span<char> raw_msg( _recv_buff.data(), bytes_transferred );
   _io_ctx.post([this, raw_msg]() // node_contollerに転送
	  {
		this->_recv_handler( raw_msg, this->_src_ep );
	  }); 

  _sock_manager.self_sock().async_receive_from( // 再び受信を行う
		boost::asio::buffer( _recv_buff ),
		_src_ep,
		std::bind( &udp_server::on_sock_read, this, std::placeholders::_1 , std::placeholders::_2 )
	  );
}

void udp_server::notice( const boost::system::error_code& ec, std::size_t bytes_transferred )
{
  std::cout << "packed received" << "\n";
}

void udp_server::stop()
{
  _sock_manager.self_sock().cancel();
 #if SS_VERBOSE
  std::cout << "[\x1b[31m stop \x1b[39m] udp server" << "\n";
  #endif 
}


};
