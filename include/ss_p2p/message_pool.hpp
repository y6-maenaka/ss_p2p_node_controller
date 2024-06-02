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
#include <iterator>

#include "boost/asio.hpp"
#include "boost/uuid/uuid.hpp"
#include "boost/uuid/uuid_io.hpp"
#include "boost/uuid/uuid_generators.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/thread/recursive_mutex.hpp"
#include "boost/thread/condition_variable.hpp"

// for multi_index_container
#include "boost/multi_index_container.hpp"
#include "boost/multi_index/tag.hpp"
#include "boost/multi_index/identity.hpp"
#include "boost/multi_index/indexed_by.hpp"
#include "boost/multi_index/hashed_index.hpp"
#include "boost/multi_index/ordered_index.hpp"
#include "boost/multi_index/sequenced_index.hpp"
#include "boost/chrono.hpp"

#include <ss_p2p/ss_logger.hpp>
#include <ss_p2p/peer.hpp>
#include "./message.hpp"


using namespace boost::asio;
using namespace boost::uuids;


namespace ss
{


constexpr std::time_t DEFAULT_MEMPOOL_REFRESH_TICK_TIME_S = 200/*[second]*/; // 時間周期処理
constexpr std::time_t DEFAULT_MESSAGE_LIFETIME_M = 20/*[minute]*/;
constexpr std::size_t MAX_RECEIVE_BUFFER_SIZE = 8388608; // 8メガバイト


class peer_message_buffer 
  /* Peer毎のメッセージバッファ
	 message_poolのエントリ */
{
  // friend message_pool;
  friend peer;

private:
  mutable boost::recursive_mutex _rmtx;
  boost::condition_variable_any _bcv;

public:
  using ref = std::shared_ptr<peer_message_buffer>; // as peer message buffer symbol

  struct received_message  
  {
	using ref = std::shared_ptr<struct received_message>;
	using message_id = uuid;
	received_message( const message::ref msg_from );
	// received_message( std::time_t timestampe );

	const message_id id; // for pop_by_id
	const message::ref msg;
	const std::time_t time; // 受信した時間
  
	static message_id (invalid_message_id)();
	bool is_invalid() const;

	void print() const;
  };
  using message_queue = std::vector< received_message::ref >;
  static bool compare_received_at( const std::time_t &base_time, const peer_message_buffer::received_message::ref msg_ref ); // for pop_since, drop
  message_queue _msg_queue; // メッセージバッファ本体

  const ip::udp::endpoint _binding_ep; // このバッファを所有するエンドポイント

  mutable std::size_t max_dynamic_mem_usage_bytes = MAX_RECEIVE_BUFFER_SIZE; // メッセージバッファの最大サイズ
  std::size_t _dynamic_mem_usage_bytes __attribute__((guarded_by(_rmtx))); // このメッセージのメモリ使用バイト数
  std::time_t _last_binded_at __attribute__((guarded_by(_rmtx))); // 最後にこの受信バッファを取得した時間
  std::time_t _last_received_at __attribute__((guarded_by(_rmtx)));  // このバッファにメッセージが最後に到着した時間

  enum pop_flag
  {
	none, // 取得と同時にdequeueする
	peek // 要素の取得のみ行う(dequeueしない)
  };
  received_message::ref pop( unsigned int idx = 0 /*先頭(古い)からのインデックス*/, pop_flag = pop_flag::none ); // 存在しない場合でも即座に返す
  received_message::ref pop_by_id( const received_message::message_id &id, pop_flag = pop_flag::none ); // message_idでメッセージをpopする
  received_message::ref pop_since( std::time_t since = 0, pop_flag = pop_flag::none ); // 0: binded_at以降のメッセージ指定
  /* 指定時間後に受信したメッセージ以降を取得する */ 
  received_message::message_id push( message::ref msg_ref );
  void clear(); // msg_queueを空にする
  template< typename Iterator > void drop( Iterator begin, Iterator end ); // 指定範囲を削除する

  ip::udp::endpoint get_binding_endpoint() const; 
  const std::time_t get_last_received_at() const;
  void update_last_binded_at();

  bool operator ==( const peer_message_buffer &pe ) const;
  bool operator !=( const peer_message_buffer &pe ) const;

  peer_message_buffer( ip::udp::endpoint ep ); // create from endpoint 
  peer_message_buffer( const peer_message_buffer &from ); // copy constructor

  void print() const; // for debug
};
using message_pool_entry = class peer_message_buffer; // message_poolではmessage_pool_entryの名の方が使いやすい

template< typename Iterator > void peer_message_buffer::drop( Iterator begin, Iterator end )
{
  static_assert(  std::is_same<
	  typename std::iterator_traits<Iterator>::value_type
	  , peer_message_buffer::message_queue::value_type>::value
	  , "drop can get iterator of message_queue" );

  if constexpr (std::is_same_v<Iterator, std::reverse_iterator<typename message_queue::iterator>> ||
		        std::is_same_v<Iterator, std::reverse_iterator<typename message_queue::const_iterator>>) 
  {
	_msg_queue.erase( end.base(), begin.base() );
  }
  else {
	_msg_queue.erase( begin, end );
  }
}


struct ss_message // 他アプリケーションから参照される
{
  using ref = std::shared_ptr<ss_message>;
  json body; // メッセージ本体
  
  struct
  {
    // ip::udp::endpoint src_endpoint; // 送信元
	ip::udp::endpoint src_endpoint;
	std::vector< ip::udp::endpoint > relay_endpoints; // 中継ノードリスト(relay_endpointsを取り出す機能は未実装)
	std::time_t timestamp; // 受信時間
  } meta;

  ss_message( peer_message_buffer::received_message::ref msg_from, const ip::udp::endpoint &src_ep );
};


struct by_peer_id{}; // tag for multi_index_container
struct by_received_at{}; // tag for multi_index_container


class peer_message_buffer_compare_received_at // received_messageの比較関数
{
public:
  bool operator()( const peer_message_buffer &entry_1, const peer_message_buffer &entry_2 ) const 
  {
	return entry_1.get_last_received_at() < entry_2.get_last_received_at();
  }
};
using message_pool_entry_compare_received_at = class peer_message_buffer_compare_received_at;

class peer_message_buffer_peer_id
{
public:
  typedef peer::id result_type;
  result_type operator()( const peer_message_buffer &input ) const
  {
	return peer::calc_peer_id( input.get_binding_endpoint() );
  }
};
using message_pool_entry_peer_id = class peer_message_buffer_peer_id;

class peer_id_linear_hasher
{
public:
  std::size_t operator()( const peer_message_buffer_peer_id::result_type &input ) const
  { // もっといい方法ああると思う(二重ハッシュになっている)
	std::string input_str( input.begin(), input.end() );
	return std::hash<std::string>()(input_str);
  }
};


class message_pool
{
private:
  mutable boost::recursive_mutex _rmtx;
  using indexed_message_set = boost::multi_index_container<
		message_pool_entry
	  , boost::multi_index::indexed_by<
		  boost::multi_index::hashed_unique< boost::multi_index::tag<by_peer_id>, message_pool_entry_peer_id, peer_id_linear_hasher > // index by peer_id
		  , boost::multi_index::ordered_non_unique< boost::multi_index::tag<by_received_at>, boost::multi_index::identity<message_pool_entry>, message_pool_entry_compare_received_at > // index by received_at
	  > // peer_id or 最終取得が近いエントリから取得する
	>;
  indexed_message_set _pool __attribute__((guarded_by(_rmtx))); 
  using entry = indexed_message_set::iterator;

  io_context &_io_ctx;
  deadline_timer _refresh_tick_timer;
  bool _requires_refresh; // 更新を行うな否か
  ss_logger *_logger;

#if SS_DEBUG
public:
#endif 
  void call_refresh_tick();
  void refresh_tick( const boost::system::error_code& ec ); // エントリーごと削除するか検討する

  entry allocate_new_entry( const ip::udp::endpoint &ep );
  peer_message_buffer::ref allocate_new_buffer( const ip::udp::endpoint &ep ); // 空のpeer_message_bufferを作成する

public:
  message_pool( io_context &io_ctx, ss_logger *logger, bool requires_refresh = true );

  void requires_refresh( bool b );
  void store( message::ref msg_ref, const ip::udp::endpoint &ep ); // 受信したメッセージを追加

  struct message_hub
  {
	friend message_pool;
	public:
	  bool is_active() const;
	  message_hub();
	  ~message_hub();
	  void start( std::function<peer::ref(ss_message::ref)> f ); // イベント稼働型
	  void stop(); 

	private:
	  void on_receive_message( std::function<peer_message_buffer::received_message::ref(void)> pop_func, ip::udp::endpoint src_ep );
	  std::function<peer::ref(ss_message::ref)> _msg_handler;

	  mutable boost::recursive_mutex _rmtx;
	  boost::condition_variable_any _bcv;
	  bool _is_active __attribute__((guarded_by(_rmtx)));
  } _msg_hub;

  message_hub &get_message_hub();
  peer_message_buffer::ref get_peer_message_buffer( const peer::id &pid ) const;
  peer_message_buffer::ref allocate_message_buffer( const peer::id &pid );
  peer_message_buffer::ref deallocate( const peer::id &pid );
  
  void print() const;
  void print_by_peer_id() const;
  void print_by_received_at() const;
};
using message_hub = message_pool::message_hub;


};


#endif 


