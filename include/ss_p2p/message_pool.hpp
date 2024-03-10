#ifndef CD82DC15_71A2_4534_A194_33DF2E3A0011
#define CD82DC15_71A2_4534_A194_33DF2E3A0011

#include <unordered_set>
#include <vector>
#include <functional>
#include <memory>

#include "boost/asio.hpp"

#include "./message.hpp"

using namespace boost::asio;


namespace ss
{


constexpr std::time_t DEFAULT_MEMPOOL_REFRECH_TICK_TIME_S = 200/*[s]*/; // 時間周期処理


class message_entry
{
  std::shared_ptr<message const> body;
  std::time_t arrived_at;
};

class message_peer_entry
{
  std::uint16_t entry_id;
  std::vector< message_entry > msg_queue;
};

class message_pool
{
private:
  using pool = std::unordered_multiset< std::uint16_t , std::shared_ptr<message_peer_entry> >;
  pool _pool;

  io_context& _io_ctx;
  deadline_timer _d_timer;
  bool _requires_refresh;
  void requires_refresh( bool b );
  
  void call_refresh_tick();
  void refresh_tick( const boost::system::error_code& ec );

public:
  message_pool( io_context &io_ctx ,bool requires_refresh = true );
  using index = pool::iterator;
  using message_retrive_func = std::function< message(index idx) >;
  message_retrive_func message_retriver();

  void retrive(); // 取得
  index store( std::shared_ptr<message> msg, ip::udp::endpoint &ep ); // 追加
};




};


#endif 


