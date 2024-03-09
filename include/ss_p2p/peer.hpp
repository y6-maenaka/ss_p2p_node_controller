#ifndef F2EBFFE1_1F20_4856_915B_0B15079C07E8
#define F2EBFFE1_1F20_4856_915B_0B15079C07E8


#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <span>

#include "./endpoint.hpp"
#include "./message.hpp"
#include "./message_pool.hpp"


namespace ss
{


class peer
{
public:
  endpoint ep;

  message receive( std::time_t timeout );
  std::time_t keep_alive = 1;

  bool operator ==(const peer& pr) const;

  peer();
  ~peer();
 
private:
  using delete_msg_pool_symbolic = std::function<void(long long index)>;
  std::vector< message_pool::index > _msg_queue; // buffer

  std::mutex _mtx;
  std::condition_variable _cv;

  void add_message( message_pool::index msg_idx ); // add from message_poo;
  void send( const std::span<unsigned char> msg );
  std::shared_ptr<message> receive( int timeout = -1 );
  // -1: データが存在しない場合はすぐに戻す
  // 0 : データが到着するまでロッキングする
  // n : n秒間データの到着を待つ
};

using peer_ptr = std::shared_ptr<peer>;


}; // namespace ss


#endif 
