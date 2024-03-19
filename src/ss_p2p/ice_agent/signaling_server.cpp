#include <ss_p2p/ice_agent/signaling_server.hpp>
#include <ss_p2p/ice_agent/ice_observer.hpp>
#include <ss_p2p/ice_agent/ice_agent.hpp>


namespace ss
{
namespace ice
{


signaling_server::signaling_server( io_context &io_ctx, ice_sender &ice_sender, direct_routing_table_controller &d_routing_table_controller, ice_observer_strage &obs_strage ) : 
   _io_ctx( io_ctx ) 
  , _ice_sender( ice_sender )
  , _d_routing_table_controller( d_routing_table_controller )
  , _obs_strage( obs_strage )
{
  return;
}

void signaling_server::income_message( std::shared_ptr<message> msg )
{
  ice_message ice_msg( *(msg->get_param("ice_agent")) );

  auto sgnl_msg_controller = ice_msg.get_sgnl_msg_controller();
  
  // 1. destが自身であれば -> signling_ok メッセージを送信 
  ip::udp::endpoint dest_ep = sgnl_msg_controller.get_dest_endpoint(); // メッセージから目的ipを取り出し

  if( dest_ep == _ice_sender.get_self_endpoint() )
  {
	// signaling_responseメッセージを送信する
	ice_message ice_res_mes = ice_message::_signaling_();
	auto msg_controller = ice_res_mes.get_sgnl_msg_controller();
	msg_controller.set_sub_protocol( ice_message::signaling_message_controller::sub_protocol_t::response );

	observer<signaling_response> sgnl_response_obs( _io_ctx, _ice_sender, _d_routing_table_controller );

	std::cout << "このさき危険" << "\n";
	_ice_sender.ice_send( dest_ep, ice_res_mes // レスポンスの送信
		, std::bind( &signaling_response::init
		, *(sgnl_response_obs.get_raw())
		, std::placeholders::_1 
		) 
	  );

	_obs_strage.add_observer<signaling_response>( sgnl_response_obs ); // ストレージに追加する
	return;
  }


  observer<signaling_relay> sgnl_relay_obs( _io_ctx, _ice_sender, _d_routing_table_controller );

  if( _d_routing_table_controller.is_exist(dest_ep) )  // signaling転送先が自身のルーティングテーブルに存在する場合
  {
	/*
	_ice_sender.ice_send( dest_ep, ice_msg, // 直接相手アドレスに転送する
		std::bind( &signaling_relay::init
		  , sgnl_relay_obs ) 
		);
	*/
	return;
  }

  std::vector<ip::udp::endpoint> relay_eps = sgnl_msg_controller.get_relay_endpoints(); // リレーしたノードを無視するようにする

  // 2. destが自身でなく,ttlが有効であれば他ノードに転送してobserverをセットする
  int ttl = sgnl_msg_controller.get_ttl();
  if( ttl <= 0 ) return; // ttlが0以下だったらメッセージを破棄する
	
  auto forward_eps = _d_routing_table_controller.collect_node( dest_ep, 3, relay_eps ); // 転送先
  for( auto itr : forward_eps )	
	sgnl_msg_controller.add_relay_endpoint(itr); // 転送先をリレーノードとして追加する
  sgnl_msg_controller.add_relay_endpoint( _ice_sender.get_self_endpoint() ); // 自身も中継ノードに追加する
 
  // 転送s
  for( auto itr : forward_eps )
  {
	/*
	_ice_sender.ice_send( itr, ice_msg
	  , std::bind( &signaling_relay::init
		, sgnl_relay_obs ) 
	  );
	*/
  }

  return;
}

void signaling_server::on_send_done( const boost::system::error_code &ec )
{
  return;
}

ice_message signaling_server::format_relay_msg( ice_message &base_msg )
{
  auto msg_controller = base_msg.get_sgnl_msg_controller();
  msg_controller.add_relay_endpoint( _ice_sender.get_self_endpoint() );

  return base_msg;
}

void signaling_server::async_hello( const boost::system::error_code &ec )
{
  std::cout << "非同期deこんにちは" << "\n";
}

void signaling_server::signaling_send( ip::udp::endpoint &dest_ep, std::string root_param, json payload )
{
  if( _d_routing_table_controller.is_exist(dest_ep) )
  {
	std::cout << "-- ** --" << "\n";
	_ice_sender.send( dest_ep, root_param, payload
		, std::bind( &signaling_server::on_send_done, this, std::placeholders::_1)
	  );
	return;
  }

  observer<signaling_request> sgnl_req_obs( _io_ctx, _ice_sender, _d_routing_table_controller );
  sgnl_req_obs.init( dest_ep, root_param, payload );

  _obs_strage.add_observer<signaling_request>( sgnl_req_obs ); // store
}

signaling_server::s_send_func signaling_server::get_signaling_send_func()
{
  std::function<void(ip::udp::endpoint &dest_ep, std::string, const json payload, const boost::system::error_code &ec)> ret =
	std::bind( 
		&signaling_server::signaling_send
		, this
		, std::placeholders::_1
		, std::placeholders::_2
		, std::placeholders::_3
	  );

  return ret;
}



}; // namespace ice
}; // namespace ss 
