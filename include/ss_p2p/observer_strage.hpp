#ifndef CF21C64E_9786_4874_8330_AB7D2077BDA2
#define CF21C64E_9786_4874_8330_AB7D2077BDA2


#include <unordered_map>
#include <unordered_set>
#include <optional>

#include "./observer.hpp"
#include <utils.hpp>

#include "boost/asio.hpp"


using namespace boost::asio;


namespace ss
{


constexpr unsigned short DEFAULT_OBSERVER_STRAGE_TICK_TIME_s = 60/*[seconds]*/;
constexpr unsigned short DEFAULT_OBSERVER_STRAGE_SHOW_STATE_TIME_s = 2/*[seconds]*/;


template <typename... Ts> class observer_strage
{
protected:
  template < typename T > using entry = std::unordered_set< observer<T>, typename observer<T>::Hash >;

  std::tuple< entry<Ts>... > _strage;
  io_context &_io_ctx;

  template < typename T > void delete_expires_observer( entry<T> &entry );
  template < typename T > void print_entry_state( entry<T> &entry );

  void call_tick();
  void refresh_tick( const boost::system::error_code &ec );
  deadline_timer _refresh_tick_timer;

public:
  observer_strage( io_context &io_ctx );

  template < typename T > std::optional< observer<T> > find_observer( observer_id id ); // 検索・取得メソッド
  template < typename T > void add_observer( observer<T> obs ); // 追加メソッド
  
  #if SS_DEBUG
  deadline_timer _debug_tick_timer;

  void show_state( const boost::system::error_code &ec )
  {
	std::apply([this](auto &... args)
	  {
		  ((print_entry_state(args)), ...);
	  }, _strage );

	_debug_tick_timer.expires_from_now(boost::posix_time::seconds( DEFAULT_OBSERVER_STRAGE_SHOW_STATE_TIME_s ));
	_debug_tick_timer.async_wait( std::bind( &observer_strage::show_state, this , std::placeholders::_1 ) );
  }
  #endif
};


template < typename... Ts >
observer_strage<Ts...>::observer_strage( io_context &io_ctx ) :
  _io_ctx(io_ctx)
  , _refresh_tick_timer( io_ctx )
  #if SS_DEBUG
  , _debug_tick_timer( io_ctx )
  #endif
{
  this->call_tick();
}

template< typename... Ts >
template< typename T > void observer_strage<Ts...>::delete_expires_observer( observer_strage::entry<T> &entry )
{
  for( auto itr = entry.begin(); itr != entry.end(); )
  {
	if( (*itr).is_expired() )
	{
	  #if SS_VERBOSE
	  std::cout << "\x1b[33m" << " | [" << (*itr).get_type_name() << "](delete observer) :: " <<"\x1b[39m" << (*itr).get_id() << "\n";
	  #endif
	  itr = entry.erase(itr);
	}
	else itr++;
  }
  return;
}

template < typename... Ts >
template < typename T > void observer_strage<Ts...>::print_entry_state( observer_strage::entry<T> &entry ) // 修正が必要
{
  for( int i=0; i<get_console_width()/2; i++ ){ printf("="); } std::cout << "\n";
  /* if constexpr (std::is_same_v<T, signaling_request>)
	std::cout << "| signaling_request" << "\n";
  else if constexpr (std::is_same_v<T, signaling_relay>) std::cout << "| signaling_relay" << "\n";
  else if constexpr (std::is_same_v<T, signaling_response>) std::cout << "| signaling_response" << "\n";
  else if constexpr (std::is_same_v<T, binding_request>) std::cout << "| binding_request" << "\n";
  else if constexpr (std::is_same_v<T, ping>) std::cout << "| ping" << "\n";
  else if constexpr (std::is_same_v<T, find_node>) std::cout << "| find_node" << "\n";
  else std::cout << "| undefine" << "\n"; */
  std::cout << "| ENTRY" << "\n";

  unsigned int count = 0;
  for( int i=0; i<get_console_width()/2; i++ ){ printf("-"); } std::cout << "\n";
  std::cout << "\x1b[32m";
  for( auto &itr : entry )
  {
	std::cout << "| ("<< count << ") ";
	itr.print(); std::cout << "\n";
	count++;
  }
  std::cout << "\x1b[39m" << "\n";
}

template < typename... Ts >
void observer_strage<Ts...>::call_tick()
{
  _refresh_tick_timer.expires_from_now(boost::posix_time::seconds( DEFAULT_OBSERVER_STRAGE_TICK_TIME_s ));
  _refresh_tick_timer.async_wait( std::bind( &observer_strage::refresh_tick, this , std::placeholders::_1 ) );
}

template < typename... Ts >
void observer_strage<Ts...>::refresh_tick( const boost::system::error_code &ec )
{
  std::apply([this](auto &... args)
	  {
		  ((delete_expires_observer(args)), ...);
	  }, _strage );

  call_tick(); // 循環的に呼び出し
}

template < typename... Ts >
template < typename T > std::optional< observer<T> > observer_strage<Ts...>::find_observer( observer_id id ) // 検索・取得メソッド
{
 auto &s_entry = std::get< entry<T> >(_strage);
  for( auto &itr : s_entry )
	if( itr.get_id() == id && !(itr.is_expired()) ) return itr;

  return std::nullopt;
}

template < typename... Ts >
template < typename T > void observer_strage<Ts...>::add_observer( observer<T> obs ) // 修正が必要
{
  /* #if SS_VERBOSE
  if constexpr (std::is_same_v<T, signaling_request>) std::cout << "| [ice observer strage](signaling_request observer) store" << "\n";
  else if constexpr (std::is_same_v<T, signaling_relay>) std::cout << "| [ice observer strage](signaling_relay observer) store" << "\n";
  else if constexpr (std::is_same_v<T, signaling_response>) std::cout << "| [ice observer strage](signaling_response observer) store" << "\n";
  else if constexpr (std::is_same_v<T, binding_request>) std::cout << "| [ice observer strage](binding_request observer) store" << "\n";
  else std::cout << "| [ice observer strage](undefined observer) store" << "\n";
  #endif */

  auto &s_entry = std::get< observer_strage::entry<T> >(_strage);
  s_entry.insert(obs);
}


};


#endif


