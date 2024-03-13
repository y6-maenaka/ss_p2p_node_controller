#include <ss_p2p/kademlia/observer.hpp>


namespace ss
{
namespace kademlia
{


observer::observer( const k_routing_table &routing_table ) : 
  _obs_id( random_generator()() ) ,
  _routing_table(routing_table)
{
  return;
}


ping_observer::ping_observer( k_node host_node, k_node swap_node, const k_routing_table &routing_table ) : 
  observer(routing_table) ,
  _host_node(host_node) ,
  _swap_node(swap_node)
{
  return;
}

template< typename ... Args >
union_observer::union_observer( std::string type, const k_routing_table &routing_table, Args ... args )
{
  if( type == "ping" )
  {
	_obs = std::make_shared<ping_observer>( routing_table, std::forward<Args>(args)... );
  }
  else if( type == "find_node" )
  {
	return;
  }
  return;
}

void union_observer::init( io_context &io_ctx ) 
{
  std::visit( _union_observer_init, _obs );
}


}; // namespace kademlia
}; // namespace ss
