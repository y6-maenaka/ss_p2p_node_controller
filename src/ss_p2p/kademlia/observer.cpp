#include <ss_p2p/kademlia/observer.hpp>


namespace ss
{
namespace kademlia
{


observer::observer( k_routing_table &routing_table ) : 
  _obs_id( random_generator()() ) ,
  _routing_table(routing_table)
{
  return;
}

observer::observer_id observer::get_id() const
{
  return _obs_id;
}

void observer::destruct_self()
{
  _expire_at = 0;
}

void observer::extend_expire_at( std::time_t t )
{
  _expire_at = std::time(nullptr) + t;
}

bool observer::is_expired() const
{
  return _expire_at <= std::time(nullptr);
}


ping_observer::ping_observer( k_routing_table &routing_table, k_node host_node, k_node swap_node ) : 
  observer(routing_table) ,
  _host_node(host_node) ,
  _swap_node(swap_node)
{
  _is_pong_arrived = false;
}

void ping_observer::init( io_context &io_ctx )
{
  #if SS_VERBOSE
  std::cout << "ping_observer init" << "\n";
  #endif
}

void ping_observer::on_call( io_context &io_ctx )
{
  if( _is_pong_arrived ) return; // pongが到着していれば特に何もしない
  
  this->destruct_self() ; // 実質破棄を許可する
  _routing_table.swap_node( _host_node, _swap_node ); // タイムアウトしていればノードを交換する
}


}; // namespace kademlia
}; // namespace ss
