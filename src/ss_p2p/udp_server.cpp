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

/* void udp_server::call_receiver()
{
  _sock_manager.self_sock().async_receive_from
	(
	  boost::asio::buffer( _recv_buff )
	  , _src_ep
	  , std::bind( &udp_server::call_income_message_handler, this
		, std::placeholders::error_code
		, std::placeholders::bytes_transferred 
	  );
	)
} */

/* void udp_server::call_income_message_handler( const boost::system::error_code &ec, std::size_t bytes_transferred )
{
  if( ec ){
	return call_receiver();
  }

  std::pair< std::span<char>, std::size_t > recv_msg = std::make_pair( _recv_buff, bytes_transferred );
  _io_ctx.post([this, recv_msg]()
  {
	this->_recv_handler( recv_msg.first, recv_msg.second );
  });

  return call_receiver();
} */

void udp_server::on_sock_read( const boost::system::error_code& ec, std::size_t bytes_transferred )
{
  std::unique_lock<std::mutex> lock(_mtx);
  #if SS_CAPTURE_PACKET
  std::cout << "(recv) " << _src_ep << " : " << bytes_transferred << "[bytes]" << "\n";
  #endif
  if( !ec || boost::asio::error::message_size )
  {
	 // std::span<char> raw_msg( _recv_buff.data(), bytes_transferred );
	 std::vector<std::uint8_t> raw_msg; raw_msg.reserve( bytes_transferred );
	 std::copy( _recv_buff.begin(), _recv_buff.begin() + bytes_transferred, std::back_inserter(raw_msg) );
	 _io_ctx.post([this, raw_msg]() // node_contollerに転送
		{
		  this->_recv_handler( raw_msg, this->_src_ep );
		}); 
  }

  _sock_manager.self_sock().async_receive_from( // 再び受信を行う
		boost::asio::buffer( _recv_buff ),
		_src_ep,
		std::bind( &udp_server::on_sock_read, this, std::placeholders::_1 , std::placeholders::_2 )
	  );
}

void udp_server::stop()
{
  _sock_manager.self_sock().cancel();
 #if SS_VERBOSE
  std::cout << "[\x1b[31m stop \x1b[39m] udp server" << "\n";
  #endif 
}


};
