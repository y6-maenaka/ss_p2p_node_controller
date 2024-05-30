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


peer_message_buffer::received_message::received_message( message::ref msg_from ) :
  msg( msg_from )
  , time( std::time(nullptr) ) // 現在の時刻を受信時刻として挿入
  , id( random_generator()() )
{
  return;
}

void peer_message_buffer::received_message::print() const
{
  std::cout << id << " : " << time;
}


peer_message_buffer::peer_message_buffer( ip::udp::endpoint ep ) :
  _binding_ep( ep )
  , _last_binded_at( 0 )
{
  return;
}

peer_message_buffer::received_message::message_id peer_message_buffer::push( message::ref msg_ref )
{
  // std::unique_lock<std::mutex> lock(this->guard.mtx); // ロックの獲得
  std::unique_lock<boost::recursive_mutex> lock(this->_rmtx);

  received_message::ref entry_ref = std::make_shared<received_message>(msg_ref);
  _msg_queue.push_back( entry_ref );
  this->_cv.notify_all(); // 待機しているスレッドあがれば起こす

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
	return ret;
  }
  else if( flag == pop_flag::peek )
  {
	auto ret = _msg_queue.at(idx);
	return ret;
  }
  return nullptr;
}

/* message_pool_entry::message_entry_ref message_pool_entry::pop_by_id( message_pool_entry::message_entry_id id )
{
  // std::unique_lock<std::mutex> lock(this->guard.mtx);

  auto itr = std::find_if( _msg_queue.begin(), _msg_queue.end(), [id]( const received_message &entry ){
		  return entry.id == id;
	  });
  if( itr == _msg_queue.end() ) return nullptr;

  received_message ret = *itr;
  _msg_queue.erase(itr);

  return std::make_shared<message_entry>(ret);
} */

void peer_message_buffer::clear()
{
  _msg_queue.clear();
}

ip::udp::endpoint peer_message_buffer::get_endpoint() const
{
  return _binding_ep;
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


message_pool::message_pool( io_context &io_ctx, ss_logger *logger, bool requires_refresh ) :
  _io_ctx( io_ctx )
  , _refresh_tick_timer( io_ctx )
  , _requires_refresh( requires_refresh )
  , _logger(logger)
{
  return;
}

peer_message_buffer::ref message_pool::allocate_new_buffer( const ip::udp::endpoint &ep )
{
  if( auto ret = _pool.emplace( ep ); ret.second ) {
	return std::make_shared<peer_message_buffer>( *(ret.first) );
  }
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

void message_pool::refresh_tick( const boost::system::error_code &ec )
{
  #ifndef SS_LOGGING_DISABLE
  _logger->log( logger::log_level::INFO, "(@message pool)", "refresh tick start" );
  #endif

  /* for( auto &[key, value] : _pool )
  {
	std::unique_lock<std::mutex> lock(value.guard.mtx); // ロックの獲得 ロックフラグは立てない
	for( auto itr = value._msg_queue.begin(); itr != value._msg_queue.end(); )
	{
	  const auto diff_t_s = std::abs( std::time(nullptr) - itr->time );
	  if( diff_t_s >= (DEFAULT_MESSAGE_LIFETIME_M*60) ) itr = value._msg_queue.erase(itr); // メッセージの到着が指定時間より前だったら削除
	  else ++itr;
	}
  } */
  if( _requires_refresh ) this->call_refresh_tick();
}

void message_pool::store( message::ref msg, const ip::udp::endpoint &ep )
{
  /* std::size_t id = message_pool_entry::calc_id(ep); // idの算出
  message_pool::entry entry = _pool.end();

  if( entry = _pool.find(id); entry == _pool.end() )
  { // 新たなendpointの場合
	if( entry = this->allocate_entry(ep); entry == _pool.end() ) return; // 失敗
  }

  message_pool_entry::message_entry_id msg_id = entry->second.push( msg ); // データの追加
  // _cv.notify_all();

  if( _msg_hub._is_active ) // pool_observerが監視状態であれば
  { // 基本的にpeer単体でreceiveしているスレッドが優先されるようになる
	auto pop_func = std::bind( &message_pool_entry::pop_by_id, std::ref(entry->second), msg_id );
	_msg_hub.on_receive_message( pop_func, ep ); // メッセージを直接渡さないのは,peer.recv()しているスレッドとの間でメッセージコピーが発生しないようにするため
  }
  return; */
}

/* message_pool::symbolic message_pool::get_symbolic( ip::udp::endpoint &ep, bool requires_clear )
{
  std::size_t id = message_pool_entry::calc_id(ep);
  message_pool::entry entry = _pool.end();

  if( entry = _pool.find(id); entry == _pool.end() ) entry = allocate_entry(ep); // 新たにエントリーを作成
  if( entry == _pool.end() )  return nullptr;

  if( requires_clear ) entry->second.clear();
  entry->second.ref_count += 1;
  return &(entry->second);
} */

/* void message_pool::release_symbolic( ip::udp::endpoint &ep )
{
  std::size_t id = message_pool_entry::calc_id(ep);
  message_pool::entry entry = _pool.end();
  if( entry = _pool.find(id); entry == _pool.end() ) return;
  if( entry->second.ref_count > 0 ) entry->second.ref_count -= 1;
} */

void message_pool::print() const
{
  /* for( auto &[key, value] : _pool ) 
  {
	for( int i=0; i < get_console_width() / 2; i++) printf("-");
	printf("\n");
	std::cout << "(msg_entry) :" << key << "\n";
	value.print();
  } */
}



message_pool::message_hub::~message_hub()
{
  _is_active = false;
}

message_pool::message_hub &message_pool::get_message_hub()
{
  return _msg_hub;
}

void message_pool::message_hub::start( std::function<peer::ref(ss_message::ref)> f )
{
  std::unique_lock<std::mutex> lock(_mtx);
  _msg_handler = f;
  _is_active = true;
}
 
void message_pool::message_hub::stop()
{
  std::unique_lock<std::mutex> lock(_mtx);
  _is_active = false;
}

void message_pool::message_hub::on_receive_message( std::function<peer_message_buffer::received_message::ref(void)> pop_func, ip::udp::endpoint src_ep )
{
  auto received_msg_ref = pop_func();
  if( received_msg_ref == nullptr ) return;
 
  ss_message::ref msg_ref = std::make_shared<ss_message>( received_msg_ref, src_ep );
  _msg_handler( msg_ref );
}


};
