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

#include <ss_p2p/observer.hpp>
#include "./k_node.hpp"
#include "./k_routing_table.hpp"
#include "./k_message.hpp"


using namespace boost::asio;
using namespace boost::uuids;


namespace ss
{
namespace kademlia
{

constexpr unsigned int DEFAULT_PING_RESPONSE_TIMEOUT_s = 5; // デフォルトのpong待機時間 


class k_observer : public base_observer
{
public:
  k_observer( io_context &io_ctx, deadline_timer &d_timer, k_routing_table &routing_table );

protected:
  k_routing_table &_routing_table;
};


class ping : public k_observer
{
public:
  ping( k_routing_table &routing_table, io_context &io_ctx, deadline_timer &d_timer, k_node host_node, k_node swap_node );

  void update_observer( k_routing_table &routing_table );
  void handle_response( std::shared_ptr<k_message> msg ); // このメソッドをタイマーセットしてio_ctxにポスト
  void timeout( const boost::system::error_code &ec );
  void init();

private:
  bool _is_pong_arrived;

  k_node _host_node;
  k_node _swap_node;
};

class find_node : public k_observer
{
public:
  find_node( k_routing_table &routing_table, io_context &io_ctx, deadline_timer &d_timer );
  void init();
  void handle_response( std::shared_ptr<k_message> msg );
};


using base_observer_ptr = std::shared_ptr<base_observer>;
const std::string generate_h_id();
  

};
};



#endif 

