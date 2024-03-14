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

constexpr unsigned short DEFAULT_EXPIRE_TIME_s = 10/*[seconds]*/;


class observer
{
public:
  using observer_id = uuid;
  observer_id get_id() const;
  bool is_expired() const;

protected:
  observer( k_routing_table &routing_table );
  virtual void on_call( io_context &io_ctx ) = 0;

  void destruct_self(); // 本オブザーバーの破棄を許可する
  void extend_expire_at( std::time_t t = DEFAULT_EXPIRE_TIME_s );
  std::time_t _expire_at; // このオブザーバーを破棄する時間
  k_routing_table &_routing_table;

private:
  const observer_id _obs_id;
};

class ping_observer : public observer
{
public:
  ping_observer( k_routing_table &routing_table, k_node host_node, k_node swap_node );

  void update_observer( k_routing_table &routing_table );
  void on_call( io_context &io_ctx ) override; // このメソッドをタイマーセットしてio_ctxにポスト
  void init( io_context &io_ctx );

private:
  bool _is_pong_arrived;

  k_node _host_node;
  k_node _swap_node;
};


using observer_ptr = std::shared_ptr<observer>;
const std::string generate_h_id();


};
};



#endif 


