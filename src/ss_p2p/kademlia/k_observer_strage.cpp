#include <ss_p2p/kademlia/k_observer_strage.hpp>


namespace ss
{
namespace kademlia
{


k_observer_strage::k_observer_strage()
{
  ping_observer_strage _ping_obs_strg;
  find_node_observer_strage _find_node_obs_strg;

  _strage["ping"] = std::move(_ping_obs_strg);
  _strage["find_node"] = std::move(_find_node_obs_strg);
}

template <typename T>
std::optional< observer<T> >& k_observer_strage::find_observer( base_observer::id id )
{
  observer<T> ret = _strage.end();

  if constexpr (std::is_same_v<T, ping>)
  {
	observer_strage_idv &strage_entry_v/*variant*/ = _strage["ping"];
	ping_observer_strage &strage_entry = std::get<observer<T>>(strage_entry_v);
	for( auto &itr : strage_entry ){
	  if( itr.get_id() == id ) return itr;
	}
  }

  if constexpr (std::is_same_v<T, find_node>)
  {
	observer_strage_idv &strage_entry_v/*variant*/ = _strage["find_node"];
	find_node_observer_strage &strage_entry = std::get<observer<T>>(strage_entry_v);
	for( auto &itr : strage_entry ){
	  if( itr.get_id() == id ) return itr;
	}
  }

  return std::nullopt;
}

template < typename T >
void k_observer_strage::add_observer( observer<T> obs )
{
  if constexpr (std::is_same_v<T, ping>)
  {
	observer_strage_idv &strage_entry_v/*variant*/ = _strage["ping"];
	ping_observer_strage &strage_entry = std::get<observer<T>>(strage_entry_v);
	strage_entry.insert(obs); // 要素の追加
  }
  if constexpr (std::is_same_v<T, find_node>)
  {
	observer_strage_idv &strage_entry_v/*variant*/ = _strage["find_node"];
	find_node_observer_strage &strage_entry = std::get<observer<T>>(strage_entry_v);
	strage_entry.insert(obs); 
  }
  return;
}


};
};
