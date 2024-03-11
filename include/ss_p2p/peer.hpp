#ifndef F2EBFFE1_1F20_4856_915B_0B15079C07E8
#define F2EBFFE1_1F20_4856_915B_0B15079C07E8


#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <span>
#include <chrono>

#include "./endpoint.hpp"
#include "./message.hpp"
#include "./message_pool.hpp"

#include "boost/asio.hpp"


namespace ss
{


class peer
{
public:
  ip::udp::endpoint _ep;

  std::time_t keep_alive = 1;

  bool operator ==(const peer& pr) const;

  std::shared_ptr<message> receive( std::time_t timeout = -1 );
  peer( ip::udp::endpoint &ep, message_pool::symbolic msg_pool_symbolic );
  ~peer();
  void print() const;
private:
  void send( const std::span<unsigned char> msg );
  message_pool::symbolic _msg_pool_symbolic;
  // -1: データが到着するまでブロッキングする
  // 0 : すぐに戻す
  // n : n秒間データの到着を待つ
};

using peer_ptr = std::shared_ptr<peer>;


}; // namespace ss


#endif 
