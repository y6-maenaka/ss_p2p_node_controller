#ifndef CD82DC15_71A2_4534_A194_33DF2E3A0011
#define CD82DC15_71A2_4534_A194_33DF2E3A0011


#include <vector>
#include <functional>
#include <memory>
#include <optional>
#include <iostream>
#include <chrono>
#include <condition_variable>
#include <unordered_map>

#include "boost/asio.hpp"
#include "boost/uuid/uuid.hpp"
#include "boost/uuid/uuid_io.hpp"
#include "boost/uuid/uuid_generators.hpp"
#include "boost/lexical_cast.hpp"


#include <utils.hpp>
#include "./message.hpp"

using namespace boost::asio;
using namespace boost::uuids;


namespace ss
{


constexpr std::time_t DEFAULT_MEMPOOL_REFRESH_TICK_TIME_S = 200/*[second]*/; // 時間周期処理
constexpr std::time_t DEFAULT_MESSAGE_LIFETIME_M = 20/*[minute]*/;


class message_peer_entry 
{
public:
  struct _message_entry_
  {
	using message_id = uuid;
	_message_entry_( std::shared_ptr<message> msg_from );

	message_id id;
	std::shared_ptr<message> msg;
	std::time_t time;
	void print() const;
  };
  using message_entry = std::optional<struct _message_entry_>;
  using message_entry_id = _message_entry_::message_id;
  // using message_entry = std::pair< std::shared_ptr<message>, std::time_t >;
  using message_queue = std::vector< _message_entry_ >;
  message_queue _msg_queue;

  ip::udp::endpoint _ep;

  unsigned short ref_count;
  struct {
	std::mutex mtx;
	std::condition_variable cv;
  } guard;

  message_entry_id push( std::shared_ptr<message> msg );
  enum pop_flag
  {
	none,
	peek
  };
  message_entry pop( unsigned int idx = 0, pop_flag = none ); // 存在しない場合でも即座に返す
  message_entry pop_by_id( message_entry_id id );
  void clear();
  ip::udp::endpoint get_endpoint() const;
  void hello();

  bool operator ==( const message_peer_entry &pe ) const;
  bool operator !=( const message_peer_entry &pe ) const;
  static std::size_t calc_id( ip::udp::endpoint &ep );
  message_peer_entry( ip::udp::endpoint ep );
};

class message_pool
{
private:
  using pool = std::unordered_map< std::size_t, class message_peer_entry >;
public:
  using entry = pool::iterator;
  using symbolic = message_peer_entry*;
private:
  pool _pool;

  std::mutex _mtx;
  std::condition_variable _cv; 

  io_context &_io_ctx;
  deadline_timer _refresh_tick_timer;
  bool _requires_refresh;

  void call_refresh_tick();
  void refresh_tick( const boost::system::error_code& ec ); // エントリーごと削除するか検討する
  entry allocate_entry( ip::udp::endpoint &ep ); // 空のmessage_peer_entryを作成する

protected:
  struct observer
  {
	std::vector< std::pair< std::function<message_peer_entry::message_entry(void)>, ip::udp::endpoint> > _msg_entry_points;

	bool _is_active = false;
	std::mutex _mtx;
	std::condition_variable _cv;
  } _observer;

  class message_pool_observer;

public:
  message_pool( io_context &io_ctx ,bool requires_refresh = true );

  message_pool_observer get_message_pool_observer();
  void requires_refresh( bool b );
  void store( std::shared_ptr<message> msg, ip::udp::endpoint &ep ); // 追加
  
  struct _message_
  {
	bool valid:1;
	ip::udp::endpoint src_ep;
	std::shared_ptr<message> msg;
	std::time_t time;

	_message_( class message_peer_entry::_message_entry_ &from, ip::udp::endpoint src_ep );
  };

  symbolic get_symbolic( ip::udp::endpoint &ep, bool requires_clear = false /*バッファのクリアをするか否か*/); // 無い場合は新規作成する
  void release_symbolic( ip::udp::endpoint &ep );
  void hello();
  
  void print() const;
};

class message_pool::message_pool_observer
{
public:
  message_pool_observer( message_pool::observer &observer );
  ~message_pool_observer();

  void start( std::function<void(message_pool::_message_)> f );
  void stop();

private:
  message_pool::observer &_obs;
};


};


#endif 


