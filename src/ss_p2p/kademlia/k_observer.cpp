#include <ss_p2p/kademlia/k_observer.hpp>


namespace ss
{
namespace kademlia
{


k_observer::k_observer( io_context &io_ctx, k_routing_table &routing_table )
  : base_observer( io_ctx ) ,
  _routing_table( routing_table )
{
  return;
}


ping::ping( io_context &io_ctx, k_routing_table &routing_table, ip::udp::endpoint ep, on_pong_handler pong_handler, on_timeout_handler timeout_handler ) :
  k_observer( io_ctx, routing_table ) 
  , _timer( io_ctx )
  , _dest_ep(ep)
  , _pong_handler( pong_handler )
  , _timeout_handler( timeout_handler )
  , _is_pong_arrived( false )
{
  return;
}

void ping::init()
{
  #if SS_VERBOSE
  std::cout << "(init observer) ping" << "\n";
  #endif

  _timer.expires_from_now( boost::posix_time::seconds(DEFAULT_PING_RESPONSE_TIMEOUT_s) );
  _timer.async_wait( std::bind( &ping::timeout, this, std::placeholders::_1 ) );
  // 指定時間経過後にpongが到着していなければswapする
} 

void ping::timeout( const boost::system::error_code &ec )
{
 if( _is_pong_arrived ) return; // pongが到着していれば特に何もしない
  
  this->destruct_self() ; // 実質破棄を許可する
 
  if( !_is_pong_arrived ){
  _io_ctx.post( [this]() // タイムアウトした時ようのハンドラを呼び出す
	  { 
		this->_timeout_handler();
	  }) ;
  }

  // _routing_table.swap_node( _host_node, _swap_node ); // タイムアウトしていればノードを交換する
}

int ping::income_message( message &msg, ip::udp::endpoint &ep )
{
  _is_pong_arrived = true;

  _io_ctx.post([this](){ // on_pong_handlerを呼び出す
		this->_pong_handler();
	  });

  this->destruct_self() ; // 実質破棄を許可する(refresh_tickで削除される)
  return 0;
}

void ping::print() const
{
  std::cout << "[observer] (ping) " << "<" << _id << ">" << "\n";
}


find_node::find_node( io_context &io_ctx, k_routing_table &routing_table ) :
  k_observer( io_ctx, routing_table )
{
  return; 
}

void find_node::init()
{
  #if SS_VERBOSE
  std::cout << "(init observer) find_node" << "\n";
  #endif
}

int find_node::income_message( message &msg, ip::udp::endpoint &ep )
{
  return 0;
}

void find_node::print() const
{
  std::cout << "[observer] (find_node) " << "<" << _id << ">" << "\n";
}


}; // namespace kademlia
}; // namespace ss
