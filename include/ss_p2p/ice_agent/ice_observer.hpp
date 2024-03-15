#ifndef F0CCAA63_1708_4F32_AE3B_810C8652A7B4
#define F0CCAA63_1708_4F32_AE3B_810C8652A7B4


#include <memory>
#include <functional>

#include <ss_p2p/observer.hpp>
#include <ss_p2p/kademlia/direct_routing_table_controller.hpp>
#include "./message.hpp"

#include "boost/asio.hpp"

using namespace boost::asio;


namespace ss
{
namespace ice
{


class ice_observer : base_observer
{
public:
  ice_observer( io_context &io_ctx, deadline_timer &d_timer, ss::kademlia::direct_routing_table_controller &d_routing_table_controller );
  virtual void init() = 0;
private:
  ss::kademlia::direct_routing_table_controller &_d_routing_table_controller;
};

class signaling_open : public ice_observer
{
public:
  void init(); 
  void timeout();
  void handle_response( std::shared_ptr<message> msg );

  signaling_open( io_context &io_ctx, deadline_timer &d_timer, ss::kademlia::direct_routing_table_controller &d_routing_table_controller );
private:
  const std::function<void(ip::udp::endpoint &ep, std::string, const json payload )> _send_func;

};


}; // namespace ice
}; // namespace ss


#endif


