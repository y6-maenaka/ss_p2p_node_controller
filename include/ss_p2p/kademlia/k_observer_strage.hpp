#ifndef CD87A151_55B7_427F_969A_75CA759E9C4A
#define CD87A151_55B7_427F_969A_75CA759E9C4A


#include <iostream>
#include <variant>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <optional>

#include <ss_p2p/observer.hpp>
#include <ss_p2p/observer_strage.hpp>
#include "./k_observer.hpp"

#include "boost/asio.hpp"
#include "boost/uuid/uuid.hpp"
#include "boost/uuid/uuid_io.hpp"
#include "boost/uuid/uuid_generators.hpp"
#include "boost/lexical_cast.hpp"


using namespace boost::asio;
using namespace boost::uuids;


namespace ss
{
namespace kademlia
{


constexpr unsigned short DEFAULT_OBSERVER_STRAGE_TICK_TIME_s = 60/*[seconds]*/;


class k_observer_strage : public ss::observer_strage
{
protected:
  union_observer_strage< ping, find_node > _strage;

  deadline_timer _refresh_tick_timer;

  template < typename T >
  void delete_expires_observer( observer_strage_entry<T> &entry )
  {
	for( auto itr = entry.begin(); itr != entry.end(); )
	{
	  if( (*itr).is_expired() ) itr = entry.erase(itr) ;
	}
	return;
  }

  void refresh_tick( const boost::system::error_code &ec );
  void call_tick( std::time_t tick_time_s = DEFAULT_OBSERVER_STRAGE_TICK_TIME_s );

public:
  template < typename T >
  std::optional< observer<T> > find_observer( observer_id id )
  {
   auto &s_entry = std::get< observer_strage_entry<T> >(_strage);
	for( auto &itr : s_entry )
	  if( itr.get_id() == id ) return itr;

	return std::nullopt;
  }

  template < typename T >
  void add_observer( observer<T> obs )
  {
	auto &s_entry = std::get< observer_strage_entry<T> >(_strage);
	s_entry.insert(obs);
  }
  
  k_observer_strage( io_context &io_ctx );

  #if SS_DEBUG
  deadline_timer _debug_timer;
  void show_state();
  #endif
};


};
};


#endif 





