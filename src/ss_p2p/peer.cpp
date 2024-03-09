#include <ss_p2p/peer.hpp>


namespace ss
{


void peer::add_message( message_pool::index msg_idx )
{
  _msg_queue.push_back( msg_idx );
  _cv.notify_all();
}

std::shared_ptr<message> peer::receive( int timeout )
{
  message_pool::index target_idx;
  if( !(_msg_queue.empty()) )
  {

  }
  else
  {

  }

  // メッセージが到着するまで待つ
  // *_message_pop_func( targetEntry )
  return nullptr;
}

};
