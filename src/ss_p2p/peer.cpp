#include <ss_p2p/peer.hpp>


namespace ss
{


peer::peer( ip::udp::endpoint &ep, message_pool::symbolic msg_pool_symbolic ) :
  _ep( ep ) ,
  _msg_pool_symbolic( msg_pool_symbolic )
{
  return;
}

peer::~peer()
{
  return;
}

std::shared_ptr<message> peer::receive( std::time_t timeout )
{
  // エントリが作成されていない場合はどうする, またリフレッシュにより削除された場合は   
  std::shared_ptr<message> ret = _msg_pool_symbolic->pop();
  if( ret == nullptr && timeout != 0 )
  {
	if( timeout == -1 )
	{
	   std::unique_lock<std::mutex> lock(_msg_pool_symbolic->guard.mtx);
	   _msg_pool_symbolic->guard.cv.wait( lock );
	   lock.unlock(); // ロックの開放 ※ ここで解放しないとデッドロックになる

	   return _msg_pool_symbolic->pop();
	}
	else
	{
	  std::unique_lock<std::mutex> lock(_msg_pool_symbolic->guard.mtx);
	  _msg_pool_symbolic->guard.cv.wait_for( lock, std::chrono::seconds(timeout) );
	  lock.unlock();

	  return _msg_pool_symbolic->pop();
	}
  }
  return ret;
}

void peer::print() const
{
  std::cout << _ep << " : ";
}


};
