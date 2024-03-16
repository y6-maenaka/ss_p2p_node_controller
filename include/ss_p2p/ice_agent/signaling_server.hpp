#ifndef BD51C051_D379_4330_8C9D_0B9E15DFEFC9
#define BD51C051_D379_4330_8C9D_0B9E15DFEFC9


#include <functional>
#include <memory>
#include <string>
#include <array>
#include <span>

#include <ss_p2p/message.hpp>
#include <ss_p2p/observer.hpp>
#include <ss_p2p/socket_manager.hpp>
#include <ss_p2p/ice_agent/ice_agent.hpp>
#include <ss_p2p/kademlia/k_routing_table.hpp>
#include <ss_p2p/kademlia/direct_routing_table_controller.hpp>
#include <json.hpp>
#include "./ice_observer.hpp"

#include "boost/asio.hpp"


using namespace boost::asio;
using namespace ss::kademlia;
using json = nlohmann::json;


namespace ss
{
namespace ice
{


class signaling_server
{
private:
  signaling_server( io_context &io_ctx, deadline_timer &d_timer, ice_agent::ice_sender& ice_sender, direct_routing_table_controller &d_routing_table_controller );
  void signaling_send( ip::udp::endpoint &dest_ep, std::string root_param, const json payload, const boost::system::error_code &ec );
  void set_signaling_open_observer( const boost::system::error_code &ec );
  void set_signaling_relay_observer( const boost::system::error_code &ec );

  void async_hello( const boost::system::error_code &ec ); // debug
  void send_done( const boost::system::error_code &ec );
  ice_message format_relay_msg( ice_message &base_msg );

public:
  using s_send_func = std::function<void(ip::udp::endpoint &dest_ep, std::string, const json payload, const boost::system::error_code &ec )>;
  s_send_func get_signaling_send_func();

  void handle_message( std::shared_ptr<ss::message> msg );
  
  // members
  io_context &_io_ctx;
  deadline_timer &_d_timer ;
  direct_routing_table_controller &_d_routing_table_controller;
  ice_agent::ice_sender &_ice_sender;
};


}; // namespace ice
}; // namespace ss


#endif 


