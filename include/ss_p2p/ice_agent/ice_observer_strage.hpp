#ifndef F348B366_1EA4_449B_943F_E1BB62918819
#define F348B366_1EA4_449B_943F_E1BB62918819


#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <tuple>
#include <functional>

#include <ss_p2p/observer.hpp>
#include <ss_p2p/observer_strage.hpp>
#include "./ice_observer.hpp"

#include "boost/asio.hpp"


using namespace boost::asio;


namespace ss
{
namespace ice
{


constexpr unsigned short DEFAULT_OBSERVER_STRAGE_TICK_TIME_s = 5/*[seconds]*/;
constexpr unsigned short DEFAULT_OBSERVER_STRAGE_SHOW_STATE_TIME_s = 2/*[seconds]*/;


class ice_observer_strage : public ss::observer_strage
{
protected:
  union_observer_strage< signaling_request, signaling_response, signaling_relay, stun > _strage;


  template < typename T >
  void delete_expires_observer( observer_strage_entry<T> &entry )
  {
	for( auto itr = entry.begin(); itr != entry.end(); )
	{
	  if( (*itr).is_expired() ) itr = entry.erase(itr) ;
	}
	return;
  }

  template < typename T >
  void print_entry_state( observer_strage_entry<T> &entry )
  {
	unsigned int count = 0;
	std::cout << "-------------------------------------------" << "\n";
	for( auto &itr : entry )
	{
	  std::cout << "("<< count << ")";
	  itr.print();
	  count++;
	}
	return;
  }

  void refresh_tick( const boost::system::error_code &ec );
  void call_tick();

  deadline_timer _refresh_tick_timer;

public:
  template < typename T >
  std::optional< observer<T> > find_observer( observer_id id ) // 検索・取得メソッド
  {
   auto &s_entry = std::get< observer_strage_entry<T> >(_strage);
	for( auto &itr : s_entry )
	  if( itr.get_id() == id ) return itr;

	return std::nullopt;
  }

  template < typename T >
  void add_observer( observer<T> obs ) // 追加メソッド
  {
	auto &s_entry = std::get< observer_strage_entry<T> >(_strage);
	s_entry.insert(obs);
  }

  ice_observer_strage( io_context &io_ctx );

  #if SS_DEBUG
  deadline_timer _debug_tick_timer;
  void show_state( const boost::system::error_code &ec );
  #endif

};


};
};


#endif 




