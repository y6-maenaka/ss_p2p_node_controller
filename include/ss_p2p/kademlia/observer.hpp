#ifndef D9FF498B_70E3_4A08_98DF_231CCB8B904D
#define D9FF498B_70E3_4A08_98DF_231CCB8B904D


#include <memory>
#include <string>
#include <variant>

#include "boost/asio.hpp"
#include "boost/uuid/uuid.hpp"
#include "boost/uuid/uuid_io.hpp"
#include "boost/uuid/uuid_generators.hpp"
#include "boost/lexical_cast.hpp"

#include "./k_node.hpp"
#include "./k_routing_table.hpp"


using namespace boost::asio;
using namespace boost::uuids;


namespace ss
{
namespace kademlia
{


class observer
{
public:
  using observer_id = uuid;

  observer();
  virtual void on_call( io_context &io_ctx, k_routing_table &routing_table ) = 0;
  const observer_id get_id() const;

private:
  const observer_id _obs_id;
  std::time_t _expire_at;
};

class ping_observer : observer
{
public:
  ping_observer( k_node host_node, k_node swap_node );
  void update_observer( k_routing_table &routing_table );

private:
  k_node _host_node;
  k_node _swap_node;
};


using observers = std::variant< std::shared_ptr<ping_observer> >;
template <typename T>
concept allowed_observers = std::is_same_v<T, ping_observer>;

class union_observer
{
private:
  observers _obs;

public:
  enum type
  {
	ping,
	find_node,
	get_node
  };

  template < typename T > // あとで実装
  union_observer( T obs_from );

  template < typename ... Args >
  union_observer( std::string type, Args ... args );

  union_observer( std::string type );
};


using observer_ptr = std::shared_ptr<observer>;
using union_observer_ptr = std::shared_ptr<union_observer>;
const std::string generate_h_id();


};
};



#endif 


