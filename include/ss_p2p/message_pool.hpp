#ifndef CD82DC15_71A2_4534_A194_33DF2E3A0011
#define CD82DC15_71A2_4534_A194_33DF2E3A0011


#include <unordered_map>
#include <vector>
#include <functional>
#include <memory>
#include <iostream>
#include <chrono>
#include <condition_variable>

#include "boost/asio.hpp"

#include "./message.hpp"

using namespace boost::asio;


namespace ss
{


constexpr std::time_t DEFAULT_MEMPOOL_REFRESH_TICK_TIME_S = 200/*[second]*/; // 時間周期処理
constexpr std::time_t DEFAULT_MESSAGE_LIFETIME_M = 20/*[minute]*/;


class message_peer_entry 
{
public:
  using message_entry = std::pair< std::shared_ptr<message>, std::time_t >;
  std::vector< message_entry > msg_queue;
  unsigned short ref_count;
  struct {
	std::mutex mtx;
	std::condition_variable cv;
  } guard;

  bool push( std::shared_ptr<message> msg );
  enum pop_flag
  {
	none,
	peek
  };
  std::shared_ptr<message> pop( unsigned int idx = 0, pop_flag = none ); // 存在しない場合でも即座に返す
  void clear();

  bool operator ==( const message_peer_entry &pe ) const;
  bool operator !=( const message_peer_entry &pe ) const;
  static std::size_t calc_id( ip::udp::endpoint &ep );
  message_peer_entry();
};

class message_pool
{
private:
  using pool = std::unordered_map< std::size_t, std::shared_ptr<message_peer_entry> >;
public:
  using entry = pool::iterator;
  using symbolic = std::shared_ptr<message_peer_entry>;
private:
  pool _pool;

  io_context& _io_ctx;
  deadline_timer _refresh_tick_timer;
  bool _requires_refresh;

  void call_refresh_tick();
  void refresh_tick( const boost::system::error_code& ec ); // エントリーごと削除するか検討する
  symbolic find_entry( ip::udp::endpoint &ep );
  entry allocate_entry( ip::udp::endpoint &ep ); // 空のmessage_peer_entryを作成する

public:
  message_pool( io_context &io_ctx ,bool requires_refresh = true );

  void requires_refresh( bool b );
  bool store( std::shared_ptr<message> msg, ip::udp::endpoint &ep ); // 追加
  symbolic get_symbolic( ip::udp::endpoint &ep, bool requires_clear = false /*バッファのクリアをするか否か*/); // 無い場合は新規作成する
  void release_symbolic( ip::udp::endpoint &ep );
};




};


#endif 


