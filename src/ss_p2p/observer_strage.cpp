#include <ss_p2p/observer_strage.hpp>


namespace ss
{


observer_strage::observer_strage( io_context &io_ctx ) :
  _io_ctx( io_ctx )
  , _d_timer( io_ctx )
{
  return;
}

template < typename T >
void observer_strage::delete_expires_observer( const observer_strage_entry<T> &entry )
{
  for( auto &itr = entry.begin(); itr != entry.end(); )
  {
	if( itr.is_expires() ) itr = entry.erase(itr) ;
  }
  return;
}

void observer_strage::refresh_tick( const boost::system::error_code &ec )
{
  std::apply([this](auto &... args)
	  {
		  ((delete_expires_observer(args)), ...);
	  }, _strage );

  this->call_tick(); // 循環的に呼び出し
}

void observer_strage::call_tick( std::time_t tick_time_s )
{
  _d_timer.expires_from_now(boost::posix_time::seconds( tick_time_s ));
  _d_timer.async_wait( std::bind( &observer_strage::refresh_tick, this , std::placeholders::_1 ) );
}

template <typename T>
std::optional< observer<T> >& observer_strage::find_observer( base_observer::id id )
{
  auto &s_entry = std::get< observer_strage_entry<T> >(_strage);
  for( auto &itr : s_entry )
	if( itr.get_id() == id ) return itr;
  
  return std::nullopt;
}

template < typename T >
void observer_strage::add_observer( observer<T> obs )
{
  auto &s_entry = std::get< observer_strage_entry<T> >(_strage);
  s_entry.insert(obs);
}


}; // namesapce ss
