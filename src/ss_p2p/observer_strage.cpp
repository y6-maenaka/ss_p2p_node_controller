#include <ss_p2p/observer_strage.hpp>


namespace ss
{


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
