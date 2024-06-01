#include <ss_p2p/peer.hpp>
#include <crypto_utils/crypto_utils.hpp>
#include <ss_p2p/message_pool.hpp>
#include "boost/thread/recursive_mutex.hpp"
#include "boost/thread/condition_variable.hpp"


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
  auto poped_msg = _msg_pool_entry_ref->pop();
  if( (poped_msg == nullptr) && timeout != 0 )
  {
	if( timeout == -1 )
	{
	   std::unique_lock<boost::recursive_mutex> lock(_msg_pool_entry_ref->_rmtx);
	   _msg_pool_entry_ref->_bcv.wait( lock, [&poped_msg, this](){
			  poped_msg = _msg_pool_entry_ref->pop();
			  return poped_msg != nullptr;
		   });
	   lock.unlock(); // ロックの開放 ※ ここで解放しないとデッドロックになる
	}
	else
	{
	  std::unique_lock<boost::recursive_mutex> lock(_msg_pool_entry_ref->_rmtx);
	  _msg_pool_entry_ref->_bcv.wait_for( lock, boost::chrono::seconds(timeout), [&poped_msg, this](){
			poped_msg = _msg_pool_entry_ref->pop();
			return poped_msg != nullptr;
		  });
	  lock.unlock();
	}
  }

  return std::make_shared<ss_message>(poped_msg, this->get_endpoint() /*自身のendpointを埋め込む*/ );
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
