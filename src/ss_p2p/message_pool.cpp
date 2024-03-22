#include <ss_p2p/message_pool.hpp>


#ifdef __LINUX__
namespace std
{
  template<>
  struct hash<boost::asio::ip::address_v4> {
	std::size_t operator()( const boost::asio::ip::address_v4 &addr ) const {
	  return static_cast<size_t>(addr.to_long());
	}
  };
};
#endif


namespace ss
{


message_peer_entry::message_peer_entry() : ref_count(0)
{
  return;
}

bool message_peer_entry::push( std::shared_ptr<message> msg )
{
  std::unique_lock<std::mutex> lock(this->guard.mtx); // ロックの獲得
  msg_queue.push_back( std::make_pair( msg, std::time(nullptr)) );
  this->guard.cv.notify_all(); // 待機しているスレッドあがれば起こす
  return true;
}
std::shared_ptr<message> message_peer_entry::pop( unsigned int idx, pop_flag flag )
{
  std::unique_lock<std::mutex> lock(this->guard.mtx);

  if( msg_queue.empty() ) return nullptr;
  if( msg_queue.size() <= idx ) return nullptr;
  if( flag == pop_flag::none )
  {
	std::shared_ptr<message> ret = msg_queue.at(idx).first;
	msg_queue.erase( msg_queue.begin() + idx );
	return ret;
  }
  else if( flag == pop_flag::peek )
  {
	std::shared_ptr<message> ret = msg_queue.at(idx).first;
	return ret;
  }
  return nullptr;
}

void message_peer_entry::clear()
{
  msg_queue.clear();
}

std::size_t message_peer_entry::calc_id( ip::udp::endpoint &ep )
{
  return std::hash<ip::address_v4>()(ep.address().to_v4()) + static_cast<std::size_t>(ep.port()) ; // とりあえず簡易的に
}


message_pool::message_pool( io_context &io_ctx,  bool requires_refresh ) :
  _io_ctx( io_ctx ) ,
  _refresh_tick_timer( io_ctx ) ,
  _requires_refresh( requires_refresh )
{
  #if SS_VERBOSE
  std::cout << "[\x1b[32m start \x1b[39m] message pool " << "\n";
  #endif

  return;
}

message_pool::symbolic message_pool::find_entry( ip::udp::udp::endpoint &ep )
{
  std::size_t id = message_peer_entry::calc_id(ep);
  return _pool.find(id)->second;
}
message_pool::entry message_pool::allocate_entry( ip::udp::endpoint &ep )
{
  std::size_t id = message_peer_entry::calc_id(ep);
  std::shared_ptr<message_peer_entry> new_entry = std::make_shared<message_peer_entry>();

  auto ret = _pool.insert( {id, new_entry} );
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
	std::unique_lock<std::mutex> lock(value->guard.mtx); // ロックの獲得 ロックフラグは立てない
	for( auto itr = value->msg_queue.begin(); itr != value->msg_queue.end(); )
	{
	  const auto diff_t_s = std::abs( std::time(nullptr) - itr->second );
	  if( diff_t_s >= (DEFAULT_MESSAGE_LIFETIME_M*60) ) itr = value->msg_queue.erase(itr); // メッセージの到着が指定時間より前だったら削除
	  else ++itr;
	}
  }
  if( _requires_refresh ) this->call_refresh_tick();
}

bool message_pool::store( std::shared_ptr<message> msg, ip::udp::endpoint &ep )
{
  std::size_t id = message_peer_entry::calc_id(ep); // idの算出
  message_pool::entry entry = _pool.end();

  if( entry = _pool.find(id); entry == _pool.end() )  
  { // 新たなendpointの場合
	std::shared_ptr<message_peer_entry> new_entry = std::make_shared<message_peer_entry>();
	if( entry = this->allocate_entry( ep ); entry == _pool.end() ) return false; // 失敗
  }
  
  #if SS_VERBOSE
  std::cout << "(message pool) new message store" << "\n";
  #endif

  return entry->second->push( msg ); // データの追加
}

message_pool::symbolic message_pool::get_symbolic( ip::udp::endpoint &ep, bool requires_clear )
{
  std::size_t id = message_peer_entry::calc_id(ep);
  message_pool::entry entry = _pool.end();

  if( entry = _pool.find(id); entry == _pool.end() ) entry = allocate_entry(ep); // 新たにエントリーを作成
  if( entry == _pool.end() )  return nullptr;

  if( requires_clear ) entry->second->clear();
  entry->second->ref_count += 1;
  return entry->second;
}

void message_pool::release_symbolic( ip::udp::endpoint &ep )
{
  std::size_t id = message_peer_entry::calc_id(ep);
  message_pool::entry entry = _pool.end();
  if( entry = _pool.find(id); entry == _pool.end() ) return;
  if( entry->second->ref_count > 0 ) entry->second->ref_count -= 1;
}


};
