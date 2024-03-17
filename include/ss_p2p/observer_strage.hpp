#ifndef CF21C64E_9786_4874_8330_AB7D2077BDA2
#define CF21C64E_9786_4874_8330_AB7D2077BDA2


#include <unordered_map>
#include <unordered_set>
#include <optional>

#include "./observer.hpp"

#include "boost/asio.hpp"


using namespace boost::asio;


namespace ss
{


constexpr unsigned short DEFAULT_OBSERVER_STRAGE_TICK_TIME_s = 60/*[seconds]*/;


class observer_strage
{
protected:
  template < typename T >
  using observer_strage_entry = std::unordered_set< observer<T>, typename observer<T>::Hash >;

  template < typename ... Ts >
  using union_observer_strage = std::tuple<observer_strage_entry<Ts>...>;
  union_observer_strage<> _strage;

  io_context &_io_ctx;
  deadline_timer _d_timer;

  template < typename T >
  void delete_expires_observer( const observer_strage_entry<T> &entry );
  void refresh_tick( const boost::system::error_code &ec );
  void call_tick( std::time_t tick_time_s = DEFAULT_OBSERVER_STRAGE_TICK_TIME_s );
public:
  template < typename T >
  std::optional< observer<T> >& find_observer( observer_id id );

  template < typename T >
  void add_observer( observer<T> obs );
  
  observer_strage( io_context &io_ctx );
};


};


#endif


