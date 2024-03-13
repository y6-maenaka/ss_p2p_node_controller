#ifndef CD87A151_55B7_427F_969A_75CA759E9C4A
#define CD87A151_55B7_427F_969A_75CA759E9C4A


#include <iostream>
#include <unordered_map>

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


namespace std
{
  template<>
  struct hash<boost::uuids::uuid> {
	std::size_t operator()(const boost::uuids::uuid &uuid) const {
	  return std::hash<std::string>()(boost::uuids::to_string(uuid));
	}
  };
}; // namespace std


namespace ss
{
namespace kademlia
{


class observer_strage
{
private:
  using observer_ptr_strage = std::unordered_map< observer::observer_id const , union_observer , std::hash<boost::uuids::uuid> >;
  observer_ptr_strage _strage;

public:
  void add_obs( observer_ptr obs_ptr );
  observer_ptr get_obs( observer::observer_id &id );
};


};
};


#endif 





