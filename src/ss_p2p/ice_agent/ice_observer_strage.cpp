#include <ss_p2p/ice_agent/ice_observer_strage.hpp>


namespace ss
{
namespace ice
{


ice_observer_strage::ice_observer_strage( io_context &io_ctx ) :
  observer_strage( io_ctx )
  , _d_timer( deadline_timer(io_ctx) )
{
  return;
}

void ice_observer_strage::refresh_tick( const boost::system::error_code &ec )
{
  std::apply([this](auto &... args)
	  {
		  ((delete_expires_observer(args)), ...);
	  }, _strage );

  this->call_tick(); // 循環的に呼び出し
}

void ice_observer_strage::call_tick( std::time_t tick_time_s )
{
  _d_timer.expires_from_now(boost::posix_time::seconds( tick_time_s ));
  _d_timer.async_wait( std::bind( &ice_observer_strage::refresh_tick, this , std::placeholders::_1 ) );
}


};
};
