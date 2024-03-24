#include <ss_p2p/message_pool.hpp>


namespace ss
{


message_peer_entry::_message_entry_::_message_entry_( std::shared_ptr<message> msg_from ) :
  msg( msg_from )
  , time( std::time_t(nullptr) )
  , id( random_generator()() ) 
{
  return;
}

void message_peer_entry::_message_entry_::print() const
{
  return;
}


message_peer_entry::message_peer_entry( ip::udp::endpoint ep ) : 
  ref_count(0)
  , _ep(ep)
{
  return;
}

message_peer_entry::message_entry_id message_peer_entry::push( std::shared_ptr<message> msg )
{
  std::unique_lock<std::mutex> lock(this->guard.mtx); // ロックの獲得
  
  _message_entry_ entry(msg);
  _msg_queue.push_back( entry );
  this->guard.cv.notify_all(); // 待機しているスレッドあがれば起こす
  // return true;
  
  return entry.id;
}

message_peer_entry::message_entry message_peer_entry::pop( unsigned int idx, pop_flag flag )
{
  std::unique_lock<std::mutex> lock(this->guard.mtx);

  if( _msg_queue.empty() ) return std::nullopt;
  if( _msg_queue.size() <= idx ) return std::nullopt;
  if( flag == pop_flag::none )
  {
	_message_entry_ ret = _msg_queue.at(idx);
	_msg_queue.erase( _msg_queue.begin() + idx );
	return std::optional<message_peer_entry::_message_entry_>(ret);
  }
  else if( flag == pop_flag::peek )
  {
	_message_entry_ ret = _msg_queue.at(idx);
	return std::optional<message_peer_entry::_message_entry_>(ret);
  }
  return std::nullopt;
}

message_peer_entry::message_entry message_peer_entry::pop_by_id( message_peer_entry::message_entry_id id )
{
  auto itr = std::find_if( _msg_queue.begin(), _msg_queue.end(), [id]( const _message_entry_ &entry ){
		  return entry.id == id;
	  });
  if( itr == _msg_queue.end() ) return std::nullopt;

  return *itr;
}

void message_peer_entry::clear()
{
  _msg_queue.clear();
}

ip::udp::endpoint message_peer_entry::get_endpoint() const
{
  return _ep;
}

std::size_t message_peer_entry::calc_id( ip::udp::endpoint &ep )
{
  return std::hash<ip::address_v4>()(ep.address().to_v4()) + static_cast<std::size_t>(ep.port()) ; // とりあえず簡易的に
}


message_pool::message_pool( io_context &io_ctx,  bool requires_refresh ) :
  _io_ctx( io_ctx )
  , _refresh_tick_timer( io_ctx )
  , _requires_refresh( requires_refresh )
{
  #if SS_VERBOSE
  std::cout << "[\x1b[32m start \x1b[39m] message pool " << "\n";
  #endif

  return;
}

message_pool::entry message_pool::allocate_entry( ip::udp::endpoint &ep )
{
  auto ret = _pool.emplace( message_peer_entry::calc_id(ep), ep ); // 直接構築
  return (ret.first);
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
  #if SS_VERBOSE
  std::cout << "message pool refresh tick start" << "\n";
  #endif

  for( auto &[key, value] : _pool ) 
  {
	std::unique_lock<std::mutex> lock(value.guard.mtx); // ロックの獲得 ロックフラグは立てない
	for( auto itr = value._msg_queue.begin(); itr != value._msg_queue.end(); )
	{
	  const auto diff_t_s = std::abs( std::time(nullptr) - itr->time );
	  if( diff_t_s >= (DEFAULT_MESSAGE_LIFETIME_M*60) ) itr = value._msg_queue.erase(itr); // メッセージの到着が指定時間より前だったら削除
	  else ++itr;
	}
  }
  if( _requires_refresh ) this->call_refresh_tick();
}

void message_pool::store( std::shared_ptr<message> msg, ip::udp::endpoint &ep )
{
  std::size_t id = message_peer_entry::calc_id(ep); // idの算出
  message_pool::entry entry = _pool.end();

  if( entry = _pool.find(id); entry == _pool.end() )  
  { // 新たなendpointの場合
	if( entry = this->allocate_entry(ep); entry == _pool.end() ) return; // 失敗
  }
  
  #if SS_VERBOSE
  std::cout << "(message pool) new message s tore" << "\n";
  #endif

  message_peer_entry::message_entry_id msg_id = entry->second.push( msg ); // データの追加
  _cv.notify_all();

  if( _observer._is_active ) // pool_observerが監視状態であれば
  {
	auto pop_func = std::bind( &message_peer_entry::pop_by_id, std::ref(entry->second), msg_id );
	_observer._msg_entry_points.push_back( std::make_pair( pop_func, entry->second.get_endpoint() ) );
	_observer._cv.notify_all();
  }
  return;
}

message_pool::symbolic message_pool::get_symbolic( ip::udp::endpoint &ep, bool requires_clear )
{
  std::size_t id = message_peer_entry::calc_id(ep);
  message_pool::entry entry = _pool.end();

  if( entry = _pool.find(id); entry == _pool.end() ) entry = allocate_entry(ep); // 新たにエントリーを作成
  if( entry == _pool.end() )  return nullptr;

  if( requires_clear ) entry->second.clear();
  entry->second.ref_count += 1;
  return &(entry->second);
}

void message_pool::release_symbolic( ip::udp::endpoint &ep )
{
  std::size_t id = message_peer_entry::calc_id(ep);
  message_pool::entry entry = _pool.end();
  if( entry = _pool.find(id); entry == _pool.end() ) return;
  if( entry->second.ref_count > 0 ) entry->second.ref_count -= 1;
}

message_pool::_message_::_message_( class message_peer_entry::_message_entry_ &from, ip::udp::endpoint src_ep )
{
  this->src_ep = src_ep;
  this->msg = from.msg;
  this->time = from.time;
  this->valid = true;
}


message_pool::message_pool_observer::message_pool_observer( message_pool::observer &obs ) :
  _obs( obs )
{
  return;
}

message_pool::message_pool_observer::~message_pool_observer()
{
  _obs._is_active = false;
  _obs._msg_entry_points.clear();
}

message_pool::message_pool_observer message_pool::get_message_pool_observer()
{
  message_pool_observer ret(_observer);
  return ret;
}

void message_pool::message_pool_observer::start( std::function<void(message_pool::_message_)> f )
{
  _obs._msg_entry_points.clear();

  while( _obs._is_active )
  {
	if( _obs._msg_entry_points.empty() )
	{
	  std::unique_lock<std::mutex> lock(_obs._mtx);
	  _obs._cv.wait( lock );
	}
	auto msg_entry_point = _obs._msg_entry_points.front();

	auto msg_entry = msg_entry_point.first();
	if( msg_entry == std::nullopt )	continue;
	auto msg_src_ep = msg_entry_point.second;

	_message_ msg( *msg_entry, msg_src_ep ); 
	f( msg ); // ハンドラの呼び出し
  }
  return;
}

void message_pool::message_pool_observer::stop()
{
  _obs._is_active = false;
  _obs._msg_entry_points.clear();
}


};
