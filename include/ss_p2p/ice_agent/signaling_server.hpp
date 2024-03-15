#ifndef BD51C051_D379_4330_8C9D_0B9E15DFEFC9
#define BD51C051_D379_4330_8C9D_0B9E15DFEFC9


#include <functional>
#include <memory>
#include <string>
#include <array>
#include <span>

#include <ss_p2p/socket_manager.hpp>
#include <ss_p2p/message.hpp>
#include <ss_p2p/observer.hpp>
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
  signaling_server( io_context &io_ctx, deadline_timer &d_timer, udp_socket_manager &sock_manager, direct_routing_table_controller &d_routing_table_controller );
  void signaling_send( ip::udp::endpoint &dest_ep, std::string root_param, const json payload, const boost::system::error_code &ec );
  udp_socket_manager &_sock_manager;
  void set_observer( const boost::system::error_code &ec );

  void async_hello( const boost::system::error_code &ec ); // debug

public:
  using s_send_func = std::function<void(ip::udp::endpoint &dest_ep, std::string, const json payload, const boost::system::error_code &ec )>;
  s_send_func get_signaling_send_func();

  io_context &_io_ctx;
  deadline_timer &_d_timer ;
  direct_routing_table_controller &_d_routing_table_controller;
};


}; // namespace ice
}; // namespace ss


#endif 


