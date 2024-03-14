#ifndef CD87A151_55B7_427F_969A_75CA759E9C4A
#define CD87A151_55B7_427F_969A_75CA759E9C4A


#include <iostream>
#include <variant>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "./rpc_manager.hpp"
#include "./k_routing_table.hpp"
#include "./k_bucket.hpp"
#include "./observer.hpp"

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


class observer_strage
{
private:
  using ping_observer_strage = std::unordered_set< observer<ping_observer>, observer<ping_observer>::Hash >;
  using find_node_observer_strage = std::unordered_set< observer<find_node_observer>, observer<find_node_observer>::Hash >;

  using observer_strage_idv = std::variant< ping_observer_strage, find_node_observer_strage >;
  using union_observer_strage = std::map< std::string, observer_strage_idv >;
  
  union_observer_strage _strage;

public:
  observer_strage();

  template < typename T >
  observer<T> get_observer( T key, base_observer::observer_id id );

  template < typename T >
  void add_observer( observer<T> obs );
};


};
};


#endif 





