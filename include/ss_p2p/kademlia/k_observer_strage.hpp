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
#include "./k_observer.hpp"

#include "boost/asio.hpp"
#include "boost/uuid/uuid.hpp"
#include "boost/uuid/uuid_io.hpp"
#include "boost/uuid/uuid_generators.hpp"
#include "boost/lexical_cast.hpp"


using namespace boost::asio;
using namespace boost::uuids;

/*
namespace std
{
  template<>
  struct hash<boost::uuids::uuid> {
	std::size_t operator()(const boost::uuids::uuid &uuid) const {
	  return std::hash<std::string>()(boost::uuids::to_string(uuid));
	}
  };
}; // namespace std
*/

namespace ss
{
namespace kademlia
{


class k_observer_strage
{
private:
  using ping_observer_strage = std::unordered_set< observer<ping>, observer<ping>::Hash >;
  using find_node_observer_strage = std::unordered_set< observer<find_node>, observer<find_node>::Hash >;

  using observer_strage_idv = std::variant< ping_observer_strage, find_node_observer_strage >;
  using union_observer_strage = std::map< std::string, observer_strage_idv >;
  
  union_observer_strage _strage;

public:
  k_observer_strage();

  template < typename T >
  std::optional< observer<T> >& find_observer( base_observer::id id );

  template < typename T >
  void add_observer( observer<T> obs );
};


};
};


#endif 





