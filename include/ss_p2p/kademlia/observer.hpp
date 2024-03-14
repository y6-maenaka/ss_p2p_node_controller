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
#include "./message.hpp"


using namespace boost::asio;
using namespace boost::uuids;


namespace ss
{
namespace kademlia
{

constexpr unsigned short DEFAULT_EXPIRE_TIME_s = 10/*[seconds]*/;
constexpr unsigned int DEFAULT_PING_RESPONSE_TIMEOUT_s = 5; // デフォルトのpong待機時間 


class base_observer
{
public:
  using observer_id = uuid;
  observer_id get_id() const;
  bool is_expired() const;
  virtual void handle_response( std::shared_ptr<message> msg ) = 0;

protected:
  base_observer( k_routing_table &routing_table, io_context &io_ctx, deadline_timer &d_timer );

  void destruct_self(); // 本オブザーバーの破棄を許可する
  void extend_expire_at( std::time_t t = DEFAULT_EXPIRE_TIME_s );
  std::time_t _expire_at; // このオブザーバーを破棄する時間
  k_routing_table &_routing_table;
  io_context &_io_ctx;
  deadline_timer &_d_timer;
  const observer_id _obs_id;
};


class ping_observer : public base_observer
{
public:
  ping_observer( k_routing_table &routing_table, io_context &io_ctx, deadline_timer &d_timer, k_node host_node, k_node swap_node );

  void update_observer( k_routing_table &routing_table );
  void handle_response( std::shared_ptr<message> msg ); // このメソッドをタイマーセットしてio_ctxにポスト
  void timeout( const boost::system::error_code &ec );
  void init();

private:
  bool _is_pong_arrived;

  k_node _host_node;
  k_node _swap_node;
};

class find_node_observer : public base_observer
{
public:
  find_node_observer( k_routing_table &routing_table, io_context &io_ctx, deadline_timer &d_timer, int _ );
  void init();
  void handle_response( std::shared_ptr<message> msg );
};


template < typename T >
// concept AllowedObserver = std::is_same_v<T, ping_observer> || std::is_same_v<T, find_node_observer>;
class observer // 実質wrapperクラス
{
private:
  std::shared_ptr<T> _body;

public:
  template < typename ... Args >
  observer( Args ... args ){
	_body = std::make_shared<T>( std::forward<Args>(args)... );
  };

  void init();
  bool is_expired() const;
  base_observer::observer_id get_id() const;
  void print() const;

  bool operator==(const observer<T> &obs ) const;
  bool operator!=(const observer<T> &obs ) const;
  struct Hash;
};
template < typename T >
struct observer<T>::Hash
{
  std::size_t operator()( const observer<T> &obs ) const;
};



using base_observer_ptr = std::shared_ptr<base_observer>;
const std::string generate_h_id();


};
};



#endif 


