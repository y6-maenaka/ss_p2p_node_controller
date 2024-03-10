#include <ss_p2p/message_pool>


namespace ss
{


message_pool::message_pool( io_context &io_ctx,  bool requires_refresh ) :
  _io_ctx( io_ctx ) ,
  _d_timer( io_ctx ) ,
  _requires_refresh( requires_refresh )
{
  #if SS_VERBOSE
  std::cout << "[\x1b[32m start \x1b[39m] message pool " << "\n";
  #endif

  return;
}

void message_pool::call_refresh_tick()
{
  _d_timer.expires_from_now(boost::posix_time::seconds(DEFAULT_MEMPOO_REFRESH_TICK_TIME_S));
  _d_timer.async_wait( std::bind(&message_pool::refresh_tick, this , std::placeholders::_1) ); // node_controller::tickの起動
}

void message_pool::requires_refresh( bool b )
{
  if( b )
  {
	_requires_refresh = true;
	this->call_refresh_tick();
	return;
  }

  _requires_refresh = false;
}

void message_pool::refresh_tick()
{
  if( _requires_refresh ) this->call_refresh_tick();
}


};
