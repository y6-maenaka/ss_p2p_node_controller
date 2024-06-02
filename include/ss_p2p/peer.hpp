#ifndef F2EBFFE1_1F20_4856_915B_0B15079C07E8
#define F2EBFFE1_1F20_4856_915B_0B15079C07E8


#include <vector>
#include <array>
#include <string>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <span>
#include <optional>
#include <chrono>

#include <json.hpp>
// #include <ss_p2p/message.hpp>
#include <ss_p2p/message_pool.fwd.hpp>

#include "boost/asio.hpp"
using json = nlohmann::json;
using namespace boost::asio;


namespace ss
{

constexpr unsigned short PEER_ID_LENGTH_BYTES = 20;


class peer
{
public:
  using ref = std::shared_ptr<class peer>;
  using s_send_func = std::function<void(ip::udp::endpoint&, std::string, json)>;
  using transport_id = std::array< std::uint8_t, PEER_ID_LENGTH_BYTES >; // this is made from peer( ip + port )
  using id = transport_id;
  
  ip::udp::endpoint _ep;
  const id _id; // node_controllerからの逆引きで使用するため必須
  std::time_t keep_alive = 1;

  bool operator ==(const peer& pr) const;

  void send( std::span<char> msg );
  void send( std::string msg );
  ss_message_ref receive( std::time_t timeout = -1 );
  // -1: データが到着するまでブロッキングする
  // 0 : すぐに戻す
  // n : n秒間データの到着を待つ

  peer( ip::udp::endpoint ep, peer_message_buffer_ref _msg_pool_entry_ref/*message_poolで管理される自身の受信バッファ*/, s_send_func send_func );
  ~peer();

  ip::udp::endpoint get_endpoint() const;
  const peer::id get_id() const;
  static peer::id calc_peer_id( const ip::udp::endpoint &ep );

  void print() const;

private:
  peer_message_buffer_ref _msg_pool_entry_ref; // 自身の受信バッファ
  s_send_func _s_send_func;
};


}; // namespace ss


#endif 
