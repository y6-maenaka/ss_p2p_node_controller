#ifndef AD49EA0D_879C_4CCE_A3D2_1552E1E2CEE3
#define AD49EA0D_879C_4CCE_A3D2_1552E1E2CEE3


#include <memory>
#include <string>

#include "./observer.hpp"

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


struct _union_observer_get_id
{
  observer::observer_id operator()( const std::shared_ptr<ping_observer> &obs );
};
struct _union_observer_is_expired
{
  bool operator()( const std::shared_ptr<ping_observer> &obs );
};


using observers = std::variant< std::shared_ptr<ping_observer> >;
template <typename T>
concept AllowedObservers = std::is_same_v<T, ping_observer>;

class union_observer
{
  friend class observer;
  friend class ping_observer;

private:
  observers _obs;

public:
  struct Hash;

  enum type
  {
	ping,
	find_node,
	get_node
  };

  template < AllowedObservers T > 
  union_observer( T obs_from )
  {
	_obs = obs_from;
  };

  template < typename ... Args >
  union_observer( std::string type, k_routing_table &routing_table, Args ... args )
  {
	if( type == "ping" ){
	  _obs = std::make_shared<ping_observer>( routing_table, std::forward<Args>(args)... );
	}
	else if( type == "find_node" ){
	  return;
	}
	return;
  };

  void init( io_context &io_ctx );
  bool is_expired() const;
  observer::observer_id get_id() const;
  void print() const;

  bool operator==(const union_observer &obs) const;
  bool operator!=(const union_observer &obs) const;
};
struct union_observer::Hash
{
  std::size_t operator()( const union_observer &obs ) const;
};


using union_observer_ptr = std::shared_ptr<union_observer>;


}; // namespace kademlia 
}; // namespace ss


#endif 


