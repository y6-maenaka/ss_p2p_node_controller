#include <ss_p2p/peer.hpp>


namespace ss
{


peer::peer( ip::udp::endpoint &ep, message_pool::symbolic msg_pool_symbolic /*,s_send_func send_func*/ ) :
  _ep( ep ) 
  , _msg_pool_symbolic( msg_pool_symbolic )
  // , _s_send_func( send_func)
{
  return;
}

peer::~peer()
{
  return;
}

std::pair< std::shared_ptr<message>, std::time_t > peer::receive( std::time_t timeout )
{
  // エントリが作成されていない場合はどうする, またリフレッシュにより削除された場合は   
  auto msg_entry = _msg_pool_symbolic->pop();
  if( msg_entry == std::nullopt && timeout != 0 )
  {
	if( timeout == -1 )
	{
	   std::unique_lock<std::mutex> lock(_msg_pool_symbolic->guard.mtx);
	   _msg_pool_symbolic->guard.cv.wait( lock );
	   lock.unlock(); // ロックの開放 ※ ここで解放しないとデッドロックになる
	}
	else
	{
	  std::unique_lock<std::mutex> lock(_msg_pool_symbolic->guard.mtx);
	  _msg_pool_symbolic->guard.cv.wait_for( lock, std::chrono::seconds(timeout) );
	  lock.unlock();
	}
  }
  if( msg_entry = _msg_pool_symbolic->pop(); msg_entry != std::nullopt ) return std::make_pair( (*msg_entry).msg, (*msg_entry).time );
  return std::make_pair( nullptr, std::time(nullptr) );
}

void peer::send( const std::span<unsigned char> msg )
{
  return;
}

void peer::print() const
{
  std::cout << _ep << " : ";
}


};
