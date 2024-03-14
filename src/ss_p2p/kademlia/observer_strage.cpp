#include <ss_p2p/kademlia/observer_strage.hpp>


namespace ss
{
namespace kademlia
{


void observer_strage::add_obs( union_observer_ptr obs_ptr )
{
  _strage.insert( *obs_ptr );
}

union_observer_ptr observer_strage::get_obs( observer::observer_id &id )
{
  return nullptr;
}

void observer_strage::refresh()
{
  for( auto &itr : _strage )
  {
	if( itr.is_expired() )
	  _strage.erase(itr); // 期限切れであれば削除する
  }
}


};
};
