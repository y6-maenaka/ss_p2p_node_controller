#include <ss_p2p/kademlia/k_observer_strage.hpp>


namespace ss
{
namespace kademlia
{


k_observer_strage::k_observer_strage( io_context &io_ctx ) :
  observer_strage( io_ctx )
  , _refresh_tick_timer( io_ctx )
  #if SS_DEBUG
  , _debug_timer( io_ctx )
  #endif
{
  return;
}

void k_observer_strage::refresh_tick( const boost::system::error_code &ec )
{
  std::apply([this](auto &... args)
	  {
		  ((delete_expires_observer(args)), ...);
	  }, _strage );

  this->call_tick(); // 循環的に呼び出し
}

void k_observer_strage::call_tick( std::time_t tick_time_s )
{
  _refresh_tick_timer.expires_from_now(boost::posix_time::seconds( tick_time_s ));
  _refresh_tick_timer.async_wait( std::bind( &k_observer_strage::refresh_tick, this , std::placeholders::_1 ) );
}


};
};
