#include <ss_p2p/message_pool.hpp>


namespace ss
{


ss_message::ss_message( peer_message_buffer::received_message::ref msg_from, const ip::udp::endpoint &src_ep )
{
  body = msg_from->msg->_body;
  meta.src_endpoint = src_ep;
  meta.timestamp = msg_from->time;

  // 本来はここで, relay_entpoindsも展開する
}

const json ss_message::get( std::string app_name ) const 
{
  if( body.contains(app_name) ) return body[app_name];
  return json({});
}

bool ss_message::is_invalid( const json &j )
{
  return j.empty();
}

peer_message_buffer::received_message::received_message( message::ref msg_from ) : 
  msg( msg_from )
  , time( std::time(nullptr) ) // 現在の時刻を受信時刻として挿入
  , id( random_generator()() ) // generate message_id(random)
{
  return;
}

/* peer_message_buffer::received_message::received_message( std::time_t timestamp ) : 
  time( timestamp  )
  , id( random_generator()() )
  , msg( nullptr )
{
  return;
} */ 

peer_message_buffer::received_message::message_id peer_message_buffer::received_message::invalid_message_id()
{
  uuid nil = nil_generator{}();
  return nil;
}

bool peer_message_buffer::received_message::is_invalid() const
{
  return this->id == peer_message_buffer::received_message::invalid_message_id();
}

void peer_message_buffer::received_message::print() const
{
  std::cout << id << " : " << time;
}


peer_message_buffer::peer_message_buffer( ip::udp::endpoint ep ) : // generate message buffer binding peer 
  _binding_ep( ep )
  , _last_received_at( 0 )
  , _last_binded_at( 0 )
{
  return;
}

peer_message_buffer::peer_message_buffer( const peer_message_buffer &from ) :
  _msg_queue( from._msg_queue )
  , _binding_ep( from._binding_ep )
  , _dynamic_mem_usage_bytes( from._dynamic_mem_usage_bytes )
  , _last_binded_at( from._last_binded_at )
  , _last_received_at( from._last_received_at )
{
  return;
}

peer_message_buffer::received_message::message_id peer_message_buffer::push( message::ref msg_ref )
{
  // std::unique_lock<std::mutex> lock(this->guard.mtx); // ロックの獲得
  std::unique_lock<boost::recursive_mutex> lock(this->_rmtx);

  received_message::ref entry_ref = std::make_shared<received_message>(msg_ref);
  _msg_queue.push_back( entry_ref );
  this->_bcv.notify_all(); // 待機しているスレッドあがれば起こす

  _dynamic_mem_usage_bytes += 1; // 一旦仮(本来はメッセージサイズを計算する)
  _last_received_at = std::time(nullptr); // received_atの更新

  return entry_ref->id;
}

peer_message_buffer::received_message::ref peer_message_buffer::pop( unsigned int idx, pop_flag flag )
{
  // std::unique_lock<std::mutex> lock(this->guard.mtx);
  if( _msg_queue.empty() ) return nullptr;
  if( _msg_queue.size() <= idx ) return nullptr;
  if( flag == pop_flag::none )
  {
	auto ret = _msg_queue.at(idx);
	_msg_queue.erase( _msg_queue.begin() + idx );

	_dynamic_mem_usage_bytes -= 1; // 一旦仮(本来はメッセージサイズを計算する)
	return ret;
  }
  else if( flag == pop_flag::peek )
  {
	auto ret = _msg_queue.at(idx);
	return ret;
  }
  return nullptr;
}

peer_message_buffer::received_message::ref peer_message_buffer::pop_by_id( const peer_message_buffer::received_message::message_id &id, pop_flag )
{
  return nullptr;
}

peer_message_buffer::received_message::ref peer_message_buffer::pop_since( std::time_t since, pop_flag )
{
  std::unique_lock<boost::recursive_mutex> lock(_rmtx);

  if( since == 0 ) since = _last_binded_at;

  if( auto ret = std::upper_bound( _msg_queue.begin(), _msg_queue.end(), since, message_pool_entry::compare_received_at ); ret != _msg_queue.end() ) return *ret;
  return nullptr;
}

bool message_pool_entry::compare_received_at( const std::time_t &base_time, const peer_message_buffer::received_message::ref msg_ref )
{
  return base_time < msg_ref->time;
}

void peer_message_buffer::clear()
{
  _msg_queue.clear();
}

ip::udp::endpoint peer_message_buffer::get_binding_endpoint() const
{
  return _binding_ep;
}

const std::time_t peer_message_buffer::get_last_received_at() const
{
  return _last_received_at;
}

void peer_message_buffer::update_last_binded_at()
{
  _last_binded_at = std::time(nullptr);
}

/* std::size_t peer_message_buffer::calc_id( ip::udp::endpoint &ep )
{
  return std::hash<ip::address_v4>()(ep.address().to_v4()) + static_cast<std::size_t>(ep.port()) ; // とりあえず簡易的に
} */

void peer_message_buffer::print() const
{
  int count = 0;
  for( auto itr : _msg_queue )
  {
	std::cout << "(" << count << ") "; itr->print();
	std::cout << "\n";
	count++;
  }
}

peer_message_buffer generate_peer_message_buffer( ip::udp::endpoint ep )
{
  return peer_message_buffer( ep );
}


message_pool::message_pool( io_context &io_ctx, const message_pool::endpoint_to_peer_func &ep_to_peer_func, ss_logger *logger, bool requires_refresh ) :
  _io_ctx( io_ctx )
  , _refresh_tick_timer( io_ctx )
  , _ep_to_peer_func( ep_to_peer_func )
  , _requires_refresh( requires_refresh )
  , _logger(logger)
  , _msg_hub( ep_to_peer_func )
{
  return;
}

message_pool::entry message_pool::allocate_new_entry( const ip::udp::endpoint &ep )
{
  if( auto ret = _pool.emplace( ep ); ret.second ) {
	return ret.first;
  }
  return _pool.end();
}

peer_message_buffer::ref message_pool::allocate_new_buffer( const ip::udp::endpoint &ep )
{
  message_pool::entry new_entry = this->allocate_new_entry(ep);
  if( new_entry == _pool.end() ) return nullptr;

  return std::make_shared<peer_message_buffer>(*new_entry);
}

peer_message_buffer::ref message_pool::get_peer_message_buffer( const peer::id &pid ) const
{
  auto &indexed_pool = _pool.get<by_peer_id>();
  auto ret = indexed_pool.find(pid);

  if( ret != _pool.end() ) return std::make_shared<peer_message_buffer>(*ret);
  return nullptr;
}

void message_pool::requires_refresh( bool b )
{
  if( b )
  {
	_requires_refresh = true;
	this->call_refresh_tick();
	return;
  }

  _requires_refresh = false;
}

void message_pool::call_refresh_tick()
{
  _refresh_tick_timer.expires_from_now(boost::posix_time::seconds(DEFAULT_MEMPOOL_REFRESH_TICK_TIME_S));
  _refresh_tick_timer.async_wait( std::bind(&message_pool::refresh_tick, this , std::placeholders::_1) ); // node_controller::tickの起動
}

void message_pool::refresh_tick( const boost::system::error_code &ec ) // 有効期限が切れているメッセージを削除する
{
  #ifndef SS_LOGGING_DISABLE
  _logger->log( logger::log_level::INFO, "(@message pool)", "refresh tick start" );
  #endif

  // 有効期限切れのメッセージを全て削除する
  auto &idx_pool = _pool.get<by_received_at>(); 
  for( auto itr = idx_pool.begin(); itr != idx_pool.end(); ++itr )
  {
	idx_pool.modify( itr, []( message_pool_entry &entry ) 
	{
	  auto lower_ritr = std::make_reverse_iterator( std::upper_bound( entry._msg_queue.begin(), entry._msg_queue.end(), std::time(nullptr) - (DEFAULT_MESSAGE_LIFETIME_M*60), message_pool_entry::compare_received_at ) ); // 指定した時間より前のメッセージ先頭を取得
	  entry.drop( lower_ritr, entry._msg_queue.rend() ); // 指定時間より前に到着したメッセージを全部削除する
	});
  }

  if( _requires_refresh ) this->call_refresh_tick();
}

void message_pool::store( message::ref msg, const ip::udp::endpoint &ep )
{
  /*
	- 基本的にpeer単体でreceiveしているスレッドが優先的にreceiveできるようになる
	- メッセージを直接渡さないのは, peer.receive()しているスレッドとの間でメッセージのコピーが発生しないようにする為
  */
  const peer::id peer_id = peer::calc_peer_id(ep); // peer_id == message_pool key
  auto &idx_pool = _pool.get<by_peer_id>();
  auto entry = idx_pool.find( peer_id ); // 指定したpeerにバインドされているpeer_message_bufferを検索
  
  if( entry == _pool.end() ) // 新規にエントリを作成する
  {  
	auto new_entry = this->allocate_new_entry(ep);
	if( new_entry == _pool.end() ) return; // failure
	entry = new_entry;
  }
  
  message_pool_entry::received_message::message_id msg_id;
  idx_pool.modify( entry, [&msg, &msg_id]( message_pool_entry &entry ){
	msg_id = entry.push( msg ); // push内部でcondition_variableでwaitしているスレッドは起こされる
	  });

 if( _msg_hub.is_active() ) // pool_observerが監視状態であれば
  { // 基本的にpeer単体でreceiveしているスレッドが優先されるようになる
	// auto pop_func = std::bind( &message_pool_entry::pop_by_id, std::ref(entry), std::ref(msg_id) ); 
	message_pool_entry temp_entry = *entry;
	std::function<peer_message_buffer::received_message::ref(void)> pop_func = std::bind( &message_pool_entry::pop_by_id, std::ref(temp_entry), std::cref(msg_id), message_pool_entry::pop_flag::none );
	// あえてmessage_idでpopする関数を指定しているのは, peer.receive()で既にメッセージがpopされていた場合,その次に到着したメッセージを誤ってpopしてしまうことを避ける為
	_msg_hub.on_receive_message( pop_func, ep ); // メッセージを直接渡さないのは,peer.recv()しているスレッドとの間でメッセージコピーが発生しないようにするため
  }
}


void message_pool::print() const
{
  std::for_each( _pool.begin(), _pool.end(), []( const message_pool_entry &entry )
	  {
		for( int i=0; i < get_console_width() / 2; i++) printf("-");
		printf("\n");
		std::cout << "(msg_entry) :" << endpoint_to_str(entry.get_binding_endpoint()) << "\n";
		entry.print();
	  });
}

void message_pool::print_by_peer_id() const
{
  auto &indexed_pool = _pool.get<by_peer_id>();
  std::for_each( indexed_pool.begin(), indexed_pool.end(), []( const message_pool_entry &entry )
	  {
		for( int i=0; i < get_console_width() / 2; i++) printf("-");
		printf("\n");
		std::cout << "(msg_entry) :" << endpoint_to_str(entry.get_binding_endpoint()) << "\n";
		entry.print();
	  });
}

void message_pool::print_by_received_at() const
{
  auto &indexed_pool = _pool.get<by_received_at>();
  std::for_each( indexed_pool.begin(), indexed_pool.end(), []( const message_pool_entry &entry )
	  {
		for( int i=0; i < get_console_width() / 2; i++) printf("-");
		printf("\n");
		std::cout << "(msg_entry) :" << endpoint_to_str(entry.get_binding_endpoint()) << "\n";
		entry.print();
	  });

}


message_pool::message_hub::message_hub( const message_pool::endpoint_to_peer_func &ep_to_peer_func ) : 
  _is_active(false)
  , _ep_to_peer_func( ep_to_peer_func )
{
  return;
}

message_pool::message_hub::~message_hub()
{
  _is_active = false;
}

message_pool::message_hub &message_pool::get_message_hub()
{
  return _msg_hub;
}

void message_pool::message_hub::start( std::function<void(peer::ref, ss_message::ref)> f )
{
  std::unique_lock<boost::recursive_mutex> lock(_rmtx);
  _msg_handler = f;
  _is_active = true;
}

void message_pool::message_hub::stop()
{
  std::unique_lock<boost::recursive_mutex> lock(_rmtx);
  _is_active = false;
}

void message_pool::message_hub::on_receive_message( std::function<peer_message_buffer::received_message::ref(void)> pop_func, ip::udp::endpoint src_ep )
{
  auto received_msg_ref = pop_func(); // メッセージの取得
  if( received_msg_ref == nullptr ) return;
 
  ss_message::ref msg_ref = std::make_shared<ss_message>( received_msg_ref, src_ep ); // ss_message::refに変換
  peer::ref src_peer_ref = _ep_to_peer_func(src_ep);
  _msg_handler( src_peer_ref, msg_ref ); // message_hubに設定されている受信時イベントハンドラを起動
}

bool message_pool::message_hub::is_active() const
{
  std::unique_lock<boost::recursive_mutex> lock(_rmtx);
  return _is_active;
}


};
