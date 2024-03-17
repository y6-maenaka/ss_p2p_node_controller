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


class k_observer_strage : public ss::observer_strage
{
private:
  union_observer_strage<ping, find_node> _strage;

public:
  k_observer_strage();
};


};
};


#endif 





