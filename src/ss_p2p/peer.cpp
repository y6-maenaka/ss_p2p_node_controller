#include <ss_p2p/peer.hpp>
#include <crypto_utils/crypto_utils.hpp>
#include <ss_p2p/message_pool.hpp>


namespace ss
{


peer::peer( ip::udp::endpoint ep, message_pool_entry::ref msg_pool_entry_ref ,s_send_func send_func ) :
  _ep( ep ) 
  , _msg_pool_entry_ref( msg_pool_entry_ref )
  , _s_send_func( send_func )
  , _id( peer::calc_peer_id(ep) )
{
  return;
}

peer::~peer()
{
  return;
}

ss_message::ref peer::receive( std::time_t timeout )
{
  // エントリが作成されていない場合はどうする, またリフレッシュにより削除された場合は   
  auto msg_entry = _msg_pool_entry_ref->pop();
  if( (msg_entry == nullptr) && timeout != 0 )
  {
	if( timeout == -1 )
	{
	   /* std::unique_lock<std::mutex> lock(_msg_pool_entry_ref->guard.mtx);
	   _msg_pool_entry_ref->guard.cv.wait( lock, [&msg_entry, this](){
			  msg_entry = _msg_pool_entry_ref->pop();
			  return msg_entry != nullptr;
		   });
	   lock.unlock(); // ロックの開放 ※ ここで解放しないとデッドロックになる */
	}
	else
	{
	  /* std::unique_lock<std::mutex> lock(_msg_pool_entry_ref->guard.mtx);
	  _msg_pool_entry_ref->guard.cv.wait_for( lock, std::chrono::seconds(timeout), [&msg_entry, this](){
			msg_entry = _msg_pool_entry_ref->pop();
			return msg_entry != nullptr;
		  });
	  lock.unlock(); */
	}
  }
  // if( msg_entry != nullptr ) return std::make_pair( msg_entry->msg, msg_entry->time );
  // return std::make_pair( nullptr, std::time(nullptr) );
}

void peer::send( std::string msg )
{
  json j_msg = msg;
  _s_send_func( _ep, std::string("messenger"), j_msg );
} 

ip::udp::endpoint peer::get_endpoint() const
{
  return _ep;
}

peer::id calc_peer_id( const ip::udp::endpoint &ep )
{
  return (cu::sha1::hash( endpoint_to_str(ep) ).to_array< peer::id::value_type, PEER_ID_LENGTH_BYTES>());
}

void peer::print() const
{
  std::cout << _ep << " : ";
}


};
