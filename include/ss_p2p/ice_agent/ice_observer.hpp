#ifndef F0CCAA63_1708_4F32_AE3B_810C8652A7B4
#define F0CCAA63_1708_4F32_AE3B_810C8652A7B4


#include <memory>
#include <functional>

#include <ss_p2p/observer.hpp>
#include <ss_p2p/kademlia/direct_routing_table_controller.hpp>
#include <ss_p2p/message.hpp>
#include <utils.hpp>
#include "./ice_agent.hpp"
#include "./ice_message.hpp"

#include "boost/asio.hpp"

using namespace boost::asio;
using namespace ss::kademlia;


namespace ss
{
namespace ice
{


constexpr unsigned short DEFAULT_SIGNALING_OPEN_TTL = 5;


class ice_observer : public ss::base_observer
{
public:
  ice_observer( io_context &io_ctx, deadline_timer &d_timer, ice_agent::ice_sender &ice_sender, ss::kademlia::direct_routing_table_controller &d_routing_table_controller );
  virtual void init() = 0;
protected:
  ss::kademlia::direct_routing_table_controller &_d_routing_table_controller;
  ice_agent::ice_sender &_ice_sender;
};

class signaling_request : public ice_observer
{
public:
  void init( ip::udp::endpoint &glob_self_ep ); 
  void timeout();
  void income_message( ss::message &msg );
  void on_traversal_done( const boost::system::error_code &ec );

  struct msg_cache
  {
	ip::udp::endpoint &ep;
	std::string param;
	json payload;

	msg_cache( ip::udp::endpoint &ep, std::string param, std::span<char> payload );
  } _msg_cache;

  signaling_request( io_context &io_ctx
	  , deadline_timer &d_timer
	  , ice_agent::ice_sender &ice_sender
	  , direct_routing_table_controller &d_routing_table_controller
	  // , ip::udp::endpoint &glob_self_ep
	  , ip::udp::endpoint &dest_ep
	  , std::string param 
	  , std::span<char> payload );
private:
  json format_request_msg( ip::udp::endpoint &src_ep, ip::udp::endpoint &dest_ep ); // リクエストメッセージのフォーマット

  const std::function<void(ip::udp::endpoint &ep, std::string, const json payload )> _send_func;
  // const ip::udp::endpoint &_glob_self_ep;

  enum state_t
  {
	requestes
	, responsed
	, relayed
  };
  state_t _state;
};

class signaling_response : public ice_observer
{
public:
  signaling_response( io_context &io_ctx, deadline_timer &d_timer, ice_agent::ice_sender &ice_sender, direct_routing_table_controller &d_routing_table_controller );
  void init(); // 有効期限を設定する
  void income_message( ss::message &msg );
};

class signaling_relay : public ice_observer
{
public:
  signaling_relay( io_context &io_ctx, deadline_timer &d_timer, ice_agent::ice_sender &ice_sender, direct_routing_table_controller &d_routing_table_controller );
  void init();
  void income_message( ss::message &msg );
};

class stun : public ice_observer
{
public:
  void init( ip::udp::endpoint &glob_self_ep );
  void income_message( message &msg );
};


}; // namespace ice
}; // namespace ss


#endif


