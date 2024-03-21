#ifndef DF2CC8B7_BABA_4168_BC62_6B54504BC58F
#define DF2CC8B7_BABA_4168_BC62_6B54504BC58F


#include <iostream>
#include <memory>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <optional>

#include <ss_p2p/message.hpp>

#include "boost/asio.hpp"


using namespace boost::asio;


namespace ss
{


namespace kademlia
{
  class direct_routing_table_controller;
}


namespace ice
{


class ice_sender;
class ice_observer_strage;


class stun_server
{
public:
  stun_server( io_context &io_ctx, ice_sender &ice_sender, ss::kademlia::direct_routing_table_controller &d_routing_table_controller, ice_observer_strage &obs_strage );
  int income_message( std::shared_ptr<message> msg, ip::udp::endpoint &ep );
  void on_send_done( const boost::system::error_code &ec );

  struct sr_object // reserved object
  {
	enum state_t
	{
	  done = 0
	  , pending
	  , notfound
	};
	std::pair< ip::udp::endpoint, bool > sync_get();
	std::pair< ip::udp::endpoint, bool > async_get();
	static sr_object (_error_)();
	static sr_object (_pending_)();
  
	sr_object();
	void update_state( state_t s, std::optional<ip::udp::endpoint> ep = std::nullopt );
	state_t get_state() const;
	private:
	  std::shared_ptr<std::mutex> _mtx; // ナンセンスかも
	  std::shared_ptr<std::condition_variable> _cv; 
	  ip::udp::endpoint _global_ep;
	  state_t _state;
  };
  [[nodiscard]] sr_object sync_binding_request( std::vector<ip::udp::endpoint> target_nodes = std::vector<ip::udp::endpoint>(), unsigned short rib = 50 ); // 信頼度(0-100):問い合わせるノード数が変わる // get global addr
  // binding_request_state async_binding_request( std::time_t timeout = 10, std::vector<ip::udp::endpoint> target_nodes = std::vector<ip::udp::endpoint>(), unsigned short rib = 50 );

private:
  io_context &_io_ctx;
  ice_sender &_ice_sender;
  ice_observer_strage &_obs_strage;
  ss::kademlia::direct_routing_table_controller &_d_routing_table_controller;
};


};
};


#endif 


