#include <ss_p2p/kademlia/observer.hpp>


namespace ss
{
namespace kademlia
{


observer::observer() : 
  _obs_id( random_generator()() )
{
  return;
}


ping_observer::ping_observer( k_node host_node, k_node swap_node ) : 
  _host_node(host_node) ,
  _swap_node(swap_node)
{
  return;
}

template< typename ... Args >
union_observer::union_observer( std::string type, Args ... args )
{
  if( type == "ping" )
  {
	_obs = std::make_shared<ping_observer>( std::forward<Args>(args)... );
  }
  else if( type == "find_node" )
  {
	return;
  }
  return;
}


}; // namespace kademlia
}; // namespace ss
