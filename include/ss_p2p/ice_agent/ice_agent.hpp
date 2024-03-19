#ifndef A3401654_2A6C_4D83_ACCD_0FAAE564EA70
#define A3401654_2A6C_4D83_ACCD_0FAAE564EA70


#include <functional>
#include <string>
#include <memory>

#include <ss_p2p/kademlia/direct_routing_table_controller.hpp>
#include <ss_p2p/message.hpp>
#include <ss_p2p/socket_manager.hpp>
#include "./ice_message.hpp"
#include "./ice_sender.hpp"
#include "./ice_observer_strage.hpp"
#include "./signaling_server.hpp"

#include "boost/asio.hpp"


using namespace boost::asio;


namespace ss
{
namespace ice
{


class ice_agent
{
public:
  ice_agent( io_context &io_ctx, udp_socket_manager &sock_manager, ip::udp::endpoint &glob_self_ep, message::app_id id, ss::kademlia::direct_routing_table_controller &d_routing_table_controller );
  void hello();

  void income_message( std::shared_ptr<message> msg ); // メッセージ受信
  
  signaling_server::s_send_func get_signaling_send_func();

  #if SS_DEBUG
  ice_observer_strage &get_observer_strage();
  #endif

private:
  udp_socket_manager &_sock_manager;
  ip::udp::endpoint &_glob_self_ep; // グローバルendpoing
  message::app_id _app_id;

  signaling_server _sgnl_server;
  ice_observer_strage _obs_strage;

  ice_sender _ice_sender;
  ice_sender& get_ice_sender();
};


}; // namespace ice
}; // namespace ss


#endif 


