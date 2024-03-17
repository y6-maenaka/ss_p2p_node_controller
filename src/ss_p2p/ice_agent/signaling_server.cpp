#include <ss_p2p/ice_agent/signaling_server.hpp>
#include <ss_p2p/ice_agent/ice_observer_strage.hpp>


namespace ss
{
namespace ice
{


signaling_server::signaling_server( io_context &io_ctx, deadline_timer &d_timer, ice_agent::ice_sender &ice_sender, direct_routing_table_controller &d_routing_table_controller, ice_observer_strage *obs_strage ) : 
   _io_ctx( io_ctx ) 
  , _d_timer( d_timer ) 
  , _ice_sender( ice_sender )
  , _d_routing_table_controller( d_routing_table_controller )
  , _obs_strage( obs_strage )
{
  return;
}

void signaling_server::set_signaling_open_observer( const boost::system::error_code &ec )
{
  // signaling_open s_org_obs( _io_ctx, _d_timer, _d_routing_table_controller );
  // observer<signaling_open> s_obs( s_org_obs );
  observer<signaling_open> s_obs( _io_ctx, _d_timer, _d_routing_table_controller );
  return;
}

void signaling_server::set_signaling_relay_observer( const boost::system::error_code &ec )
{
  return;
}

void signaling_server::income_message( std::shared_ptr<message> msg )
{
  ice_message ice_msg( *(msg->get_param("ice_agent")) );

  auto sgnl_msg_controller = ice_msg.get_sgnl_msg_controller();
  
  // 1. destが自身であれば -> signling_ok メッセージを送信 
  ip::udp::endpoint dest_ep = sgnl_msg_controller.get_dest_endpoint();
  if( dest_ep == _ice_sender.get_self_endpoint() )
  {
	// signaling_responseメッセージを送信する
	ice_message ice_res_mes = ice_message::_signaling_();
	auto msg_controller = ice_res_mes.get_sgnl_msg_controller();
	msg_controller.set_sub_protocol( ice_message::signaling_message_controller::sub_protocol_t::response );

	observer<signaling_response> sgnl_response_obs( _io_ctx, _d_timer, _ice_sender, _d_routing_table_controller );

	_ice_sender.ice_send( dest_ep, ice_res_mes // レスポンスの送信
		, std::bind(&observer<signaling_response>::init
		, sgnl_response_obs ) 
	  );
  
	_obs_strage->add_observer<signaling_response>( sgnl_response_obs ); // ストレージに追加する
	return;
  }

  std::vector<ip::udp::endpoint> relay_eps = sgnl_msg_controller.get_relay_endpoints(); // リレーしたノードを無視するようにする

  // 2. destが自身でなく,ttlが有効であれば他ノードに転送してobserverをセットする
  int ttl = sgnl_msg_controller.get_ttl();
  if( ttl <= 0 ) return; // ttlが0以下だったらメッセージを破棄する
	
  auto forward_nodes = _d_routing_table_controller.collect_nodes( dest_ep, 3, relay_eps ); // 転送先
  for( auto itr : forward_nodes )	
	sgnl_msg_controller.add_relay_endpoint(itr);

  observer<signaling_relay> sgnl_relay_obs( _io_ctx, _d_timer, _ice_sender, _d_routing_table_controller );
  _ice_sender.ice_send( dest_ep, ice_msg
	  , std::bind( &observer<signaling_relay>::init
		, sgnl_relay_obs ) 
	  );

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

void signaling_server::signaling_send( ip::udp::endpoint &dest_ep, std::string root_param, const json payload, const boost::system::error_code &ec )
{
  // ice_message ice_msg;
  // ss::message::encoded_message enc_msg = ss::message::encode( msg );

  #if SS_CAPTURE_PACKET
  std::cout << dest_ep << " : ";
  // for( auto itr : enc_msg ) std::cout << itr;
  std::cout << "\n";
  #endif

  // _ice_sender.send( dest_ep, ice_msg, std::bind( &signaling_server::set_signaling_open_observer, this, std::placeholders::_1) );
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
		, std::placeholders::_4
	  );

  return ret;
}



}; // namespace ice
}; // namespace ss 
