#include <ss_p2p/kademlia/k_observer.hpp>


namespace ss
{
namespace kademlia
{


k_observer::k_observer( io_context &io_ctx, deadline_timer &d_timer, k_routing_table &routing_table )
  : base_observer( io_ctx, d_timer ) ,
  _routing_table( routing_table )
{
  return;
}


ping::ping( k_routing_table &routing_table, io_context &io_ctx, deadline_timer &d_timer, k_node host_node, k_node swap_node ) : 
  k_observer( io_ctx, d_timer, routing_table ) ,
  _host_node( host_node ) ,
  _swap_node( swap_node )
{
  _is_pong_arrived = false;
}

void ping::init()
{
  #if SS_VERBOSE
  std::cout << "(init observer) ping" << "\n";
  #endif

  _d_timer.expires_from_now( boost::posix_time::seconds(DEFAULT_PING_RESPONSE_TIMEOUT_s) );
  _d_timer.async_wait( std::bind( &ping::timeout, this, std::placeholders::_1 ) );
  // 指定時間経過後にpongが到着していなければswapする
}

void ping::timeout( const boost::system::error_code &ec )
{
 if( _is_pong_arrived ) return; // pongが到着していれば特に何もしない
  
  this->destruct_self() ; // 実質破棄を許可する
  _routing_table.swap_node( _host_node, _swap_node ); // タイムアウトしていればノードを交換する
}

void ping::handle_response( std::shared_ptr<message> msg )
{
  _is_pong_arrived = true;
}


find_node::find_node( k_routing_table &routing_table, io_context &io_ctx, deadline_timer &d_timer ) :
  k_observer( io_ctx, d_timer, routing_table )
{
  return; 
}

void find_node::init()
{
  #if SS_VERBOSE
  std::cout << "(init observer) find_node" << "\n";
  #endif
}

void find_node::handle_response( std::shared_ptr<message> msg )
{
  return;
}


}; // namespace kademlia
}; // namespace ss
