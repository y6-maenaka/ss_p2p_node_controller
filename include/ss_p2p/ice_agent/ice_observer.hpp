#ifndef F0CCAA63_1708_4F32_AE3B_810C8652A7B4
#define F0CCAA63_1708_4F32_AE3B_810C8652A7B4


#include <memory>
#include <functional>

#include <ss_p2p/observer.hpp>
#include <ss_p2p/kademlia/direct_routing_table_controller.hpp>
#include <ss_p2p/message.hpp>
#include <utils.hpp>
#include "./ice_message.hpp"

#include "boost/asio.hpp"

using namespace boost::asio;
using namespace ss::kademlia;


namespace ss
{
namespace ice
{

class ice_sender;
constexpr unsigned short DEFAULT_SIGNALING_OPEN_TTL = 5;


class ice_observer : public ss::base_observer
{
public:
  ice_observer( io_context &io_ctx, ice_sender &ice_sender, ip::udp::endpoint &glob_self_ep, ss::kademlia::direct_routing_table_controller &d_routing_table_controller );
protected:
  ss::kademlia::direct_routing_table_controller &_d_routing_table_controller;
  ice_sender &_ice_sender;
  ip::udp::endpoint &_glob_self_ep;
};

class signaling_request : public ice_observer
{
public:
  void init( ip::udp::endpoint &dest_ep, std::string parma, json payload );
  void timeout();
  void income_message( ss::message &msg );
  void print() const;

  struct msg_cache
  {
	ip::udp::endpoint ep;
	std::string param;
	json payload;

  } _msg_cache;

  signaling_request( io_context &io_ctx, ice_sender &ice_sender, ip::udp::endpoint &glob_self_ep, ss::kademlia::direct_routing_table_controller &d_routing_table_controller );
  void on_send_done( const boost::system::error_code &ec );
private:
  json format_request_msg( ip::udp::endpoint &src_ep, ip::udp::endpoint &dest_ep ); // リクエストメッセージのフォーマット

  void on_traversal_done( const boost::system::error_code &ec );
  // const ip::udp::endpoint &_glob_self_ep;
  
  bool _done = false;
};

class signaling_response : public ice_observer
{
public:
  signaling_response( io_context &io_ctx, ice_sender &ice_sender, ip::udp::endpoint &glob_self_ep, direct_routing_table_controller &d_routing_table_controller );
  void init( const boost::system::error_code &ec ); // 有効期限を設定する
  void income_message( message &msg );
  void on_send_done( const boost::system::error_code &ec );
  void print() const;
};

class signaling_relay : public ice_observer
{
public:
  signaling_relay( io_context &io_ctx, ice_sender &ice_sender, ip::udp::endpoint &glob_self_ep, direct_routing_table_controller &d_routing_table_controller );
  void init();
  void income_message( message &msg );
  void on_send_done( const boost::system::error_code &ec );
  void print() const;
};

class stun : public ice_observer
{
public:
  void init( ip::udp::endpoint &glob_self_ep );
  void income_message( message &msg );
  void print() const;
};


}; // namespace ice
}; // namespace ss


#endif


