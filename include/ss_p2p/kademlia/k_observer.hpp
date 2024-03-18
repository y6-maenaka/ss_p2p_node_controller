#ifndef D9FF498B_70E3_4A08_98DF_231CCB8B904D
#define D9FF498B_70E3_4A08_98DF_231CCB8B904D


#include <memory>
#include <functional>

#include <ss_p2p/observer.hpp>
#include <ss_p2p/message.hpp>
#include <utils.hpp>
#include "./k_message.hpp"
#include "./k_node.hpp"
#include "./k_routing_table.hpp"

#include "boost/asio.hpp"


using namespace boost::asio;
using namespace boost::uuids;


namespace ss
{
namespace kademlia
{

constexpr unsigned int DEFAULT_PING_RESPONSE_TIMEOUT_s = 5; // デフォルトのpong待機時間 


class k_observer : public ss::base_observer
{
public:
  k_observer( io_context &io_ctx, k_routing_table &routing_table );

protected:
  k_routing_table &_routing_table;
};


class ping : public k_observer
{
public:
  ping( k_routing_table &routing_table, io_context &io_ctx, k_node host_node, k_node swap_node );

  void update_observer( k_routing_table &routing_table );
  void handle_response( std::shared_ptr<k_message> msg ); // このメソッドをタイマーセットしてio_ctxにポスト
  void timeout( const boost::system::error_code &ec );
  void init();
  void print() const;

private:
  bool _is_pong_arrived;
  deadline_timer _timer; // 固有のものを作らないとエラーになる可能性が高い

  k_node _host_node;
  k_node _swap_node;
};

class find_node : public k_observer
{
public:
  find_node( k_routing_table &routing_table, io_context &io_ctx );
  void init();
  void handle_response( std::shared_ptr<k_message> msg );
  void print() const;
};


using base_observer_ptr = std::shared_ptr<base_observer>;
const std::string generate_h_id();
  

};
};



#endif 


