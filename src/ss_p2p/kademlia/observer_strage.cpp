#include <ss_p2p/kademlia/observer_strage.hpp>


namespace ss
{
namespace kademlia
{


void observer_strage::add_obs( observer_ptr obs_ptr )
{
  const auto id = obs_ptr->get_id();
  _strage.insert( {id, obs_ptr} );
}

observer_ptr observer_strage::get_obs( observer::observer_id &id )
{
  return nullptr;
}


};
};
