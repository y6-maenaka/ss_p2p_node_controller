#include <ss_p2p/udp_server.hpp>


namespace ss
{


udp_server::udp_server( udp_socket_manager &sock_manager ) : _sock_manager(sock_manager)
{
  return;
}

void udp_server::start()
{
  // ブートストラップする
  _sock_manager.self_sock().async_receive_from(
		boost::asio::buffer( _recv_buff ),
		_src_ep,
		std::bind( &udp_server::on_receive, this, std::placeholders::_1 , std::placeholders::_2 )
	  );

  return;
}

void udp_server::on_receive( const boost::system::error_code& ec, std::size_t bytes_transferred )
{
  std::string raw_msg( _recv_buff.data(), bytes_transferred );

  _sock_manager.self_sock().async_receive_from( // 再び受信を行う
		boost::asio::buffer( _recv_buff ),
		_src_ep,
		std::bind( &udp_server::on_receive, this, std::placeholders::_1 , std::placeholders::_2 )
	  );
}

void udp_server::stop()
{
  _sock_manager.self_sock().cancel();
}


};
