#include <ss_p2p/ice_agent/stun_server.hpp>
#include <ss_p2p/ice_agent/ice_observer.hpp>
#include <ss_p2p/ice_agent/ice_observer_strage.hpp>
#include <ss_p2p/observer.hpp>
#include <ss_p2p/ice_agent/ice_sender.hpp>

#include <algorithm>


namespace ss
{
namespace ice
{


stun_server::stun_server( io_context &io_ctx, ice_sender &ice_sender, ss::kademlia::direct_routing_table_controller &d_routing_table_controller, ice_observer_strage &obs_strage ) :
  _io_ctx( io_ctx )
  , _ice_sender( ice_sender )
  , _d_routing_table_controller( d_routing_table_controller )
  , _obs_strage( obs_strage )
{
  return;
}

void stun_server::on_send_done( const boost::system::error_code &ec )
{
  #if SS_VERBOSE
  if( !ec ) std::cout << "(stun server) send done" << "\n";
  else std::cout << "(stun server) send error" << "\n";
  #endif
}

[[nodiscard]] stun_server::sr_object stun_server::sync_binding_request( std::vector<ip::udp::endpoint> target_nodes , unsigned short reliability )
{
  std::cout << "-- 0" << "\n";
  std::vector<ip::udp::endpoint> request_nodes = std::vector<ip::udp::endpoint>(); 
  request_nodes.reserve( target_nodes.size() );
  std::cout << "-- 0.1" << "\n";
  std::copy( target_nodes.begin(), target_nodes.end(), std::back_inserter(request_nodes) );

  std::cout << "-- 1" << "\n";
  std::cout << "src :: " << target_nodes.size() << "\n";
  std::cout << "dest :: " << request_nodes.size() << "\n";
  std::size_t request_node_count = std::max( (std::size_t)1, request_nodes.size() );
  request_node_count = std::max( request_node_count, (std::size_t)(reliability/10) );

  if( request_node_count > request_nodes.size() ) // 
  {
	std::cout << "-- 2" << "\n";
	ip::udp::endpoint root_ep( ip::address::from_string("0.0.0.0") , 0 );
	auto collected_nodes = _d_routing_table_controller.collect_node( root_ep, request_node_count - request_nodes.size(), request_nodes );
  }
   
  std::cout << "-- 3" << "\n";
  if( request_nodes.empty() ) return stun_server::sr_object::_error_();

  std::cout << "-- 4" << "\n";
  sr_object sr; // 予約オブジェクトの作成
  observer<class binding_request> binding_req_obs( _io_ctx, _ice_sender, _d_routing_table_controller );
  binding_req_obs.init( sr );
 
  ice_message ice_msg = ice_message::_stun_();
  ice_msg.set_observer_id( binding_req_obs.get_id() ); // observer_idのセット
  auto msg_controller = ice_msg.get_stun_message_controller();
  msg_controller.set_sub_protocol( ice_message::stun::sub_protocol_t::binding_request );
  for( auto itr : request_nodes ){
	std::cout << "-- 5" << "\n";
	if( bool flag = _ice_sender.sync_ice_send( itr, ice_msg ); flag ){
	  binding_req_obs.get_raw()->add_requested_ep(itr); // 送信先済みリストに登録
	}
  }
  
  std::cout << "-- 6" << "\n";
  _obs_strage.add_observer<class binding_request>(binding_req_obs); // observerを追加
  return stun_server::sr_object::_pending_();
}


int stun_server::income_message( std::shared_ptr<message> msg, ip::udp::endpoint &ep )
{
  ice_message ice_msg( *(msg->get_param("ice_agent")));
  auto stun_message_controller = ice_msg.get_stun_message_controller();

  #if SS_DEBUG
  std::cout << "(stun_server) income message" << "\n";
  ice_msg.print();
  #endif

  if( stun_message_controller.get_sub_protocol() == ice_message::stun::sub_protocol_t::binding_request )
  { // observerは特に作成する必要はない
	auto msg_controller = ice_msg.get_stun_message_controller(); // リクエストメッセージをそのまま使う
	msg_controller.set_sub_protocol( ice_message::stun::sub_protocol_t::binding_response );
	msg_controller.set_global_ep(ep);

	_ice_sender.async_ice_send( ep, ice_msg
		, std::bind( &stun_server::on_send_done, this, std::placeholders::_1 ) );

	return 0;
  }
  return 0;
}


stun_server::sr_object::sr_object() :
  _state( stun_server::sr_object::state_t::pending )
  , _mtx( std::make_shared<std::mutex>() )
  , _cv( std::make_shared<std::condition_variable>() )
{
  _global_ep = ip::udp::endpoint( ip::address::from_string("0.0.0.0"), 0 ); // 一応無効なアドレスとして
}

std::pair< ip::udp::endpoint, bool > stun_server::sr_object::sync_get()
{
  std::unique_lock<std::mutex> lock(*_mtx);
  if( _state != stun_server::sr_object::state_t::pending )
	_cv->wait( lock );

  if( _state == stun_server::sr_object::state_t::done ) return std::make_pair( _global_ep, true );
  else return std::make_pair( ip::udp::endpoint( ip::address::from_string("0.0.0.0"), 0 ), false );
}

std::pair< ip::udp::endpoint, bool > stun_server::sr_object::async_get()
{
  if( _state == stun_server::sr_object::state_t::done ) return std::make_pair( _global_ep, true );
  else return std::make_pair( ip::udp::endpoint( ip::address::from_string("0.0.0.0"), 0 ), false );
}

void stun_server::sr_object::update_state( state_t s, std::optional<ip::udp::endpoint> ep )
{
  if( s == stun_server::sr_object::state_t::done && ep != std::nullopt ){
	_state = stun_server::sr_object::state_t::done;
	_global_ep = *ep;
  }
  else
  {
	_state = stun_server::sr_object::state_t::notfound;
  }
  _cv->notify_all(); // 成功でも失敗でも起こす
}

stun_server::sr_object::state_t stun_server::sr_object::get_state() const
{
  return _state;
}

stun_server::sr_object stun_server::sr_object::_error_()
{
  sr_object ret;
  ret._state = stun_server::sr_object::state_t::notfound;
  return ret;
}

stun_server::sr_object stun_server::sr_object::_pending_()
{
  sr_object ret;
  ret._state = stun_server::sr_object::state_t::pending;
  return ret;
}


};
};
