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
#include <iostream>

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
  using s_send_func = std::function<void(ip::udp::endpoint&, std::string, json /*高レイヤアプリケーションのダンプされた完全なメッセージ*/)>;
  using transport_id = std::array< std::uint8_t, PEER_ID_LENGTH_BYTES >; // this is made from peer( ip + port )
  struct id {
	using value_type = peer::transport_id::value_type;
	peer::transport_id _body;
	peer::transport_id::const_iterator cbegin() const { return _body.cbegin(); };
	peer::transport_id::const_iterator cend() const { return _body.cend(); };
	std::string to_str() const { return std::string( _body.cbegin(), _body.cend() ); };
	std::size_t to_size_t() const {  // 一旦hash形式でstd::size_tを得るようにしておく
	  // 255.255.255.255:0000 -> std::size_t
	  return std::hash<std::string>()( this->to_str() );
	}
	std::size_t get_hash() const{
	  return std::hash<std::string>()(this->to_str());
	}

	id(){};
	id( transport_id from ) : _body(from){};
	static id (none)(){
	  peer::id ret;
	  ret._body.fill(0x00);
	  return ret;
	};

	bool operator ==( const id &other ) const{
	  return std::equal( _body.cbegin(), _body.cend(), other.cbegin() );
	}
	bool operator<(const peer::id &other ){
	  return this->to_size_t() < other.to_size_t();
	};
	bool operator>(const peer::id &other){
	  return this->to_size_t() > other.to_size_t();
	}
  };
  
  ip::udp::endpoint _ep;
  const id _id; // node_controllerからの逆引きで使用するため必須
  std::time_t keep_alive = 1;

  // bool operator ==(const peer& pr) const;

  /* void send( std::span<char> msg );
  void send( std::string msg ); */
  template <typename Container> void send( const Container &payload_c );
  ss_message_ref receive( std::time_t timeout = -1 );
  // -1: データが到着するまでブロッキングする
  // 0 : すぐに戻す
  // n : n秒間データの到着を待つ
  const bool ping() const; // peerが生きているか調べる

  peer( ip::udp::endpoint ep, peer_message_buffer_ref _msg_pool_entry_ref/*message_poolで管理される自身の受信バッファ*/, s_send_func send_func );
  ~peer();

  ip::udp::endpoint get_endpoint() const;
  const peer::id get_id() const;
  static peer::id calc_peer_id( const ip::udp::endpoint &ep );

  void print() const;

  struct Hash;
private:
  peer_message_buffer_ref _msg_pool_entry_ref; // 自身の受信バッファ
  s_send_func _s_send_func;
};

struct peer::Hash
{
  std::size_t operator()( const peer &p ) const;
  std::size_t operator()( const peer::ref &p_ref ) const;
};


template <typename Container> void peer::send( const Container &payload_c /*payload_container*/ )
{
  std::cout << "## 1" << "\n";
  std::string payload_s( payload_c.begin(), payload_c.end() );
  std::cout << "## 2" << "\n";
  json msg_j = payload_s;
  std::cout << "## 3" << "\n";
  _s_send_func( _ep, std::string("messenger"), msg_j ); // 実環境では,任意のアプリケーション名を指定する
  std::cout << "## 4" << "\n";
}


}; // namespace ss


namespace boost{
  inline std::size_t hash_value(const ss::peer::id &id){
	return id.get_hash();
  }
};


#endif 
