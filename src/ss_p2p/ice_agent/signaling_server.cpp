#include <ss_p2p/ice_agent/signaling_server.hpp>
#include <ss_p2p/ice_agent/ice_observer.hpp>
#include <ss_p2p/ice_agent/ice_agent.hpp>


namespace ss
{
namespace ice
{


signaling_server::signaling_server( io_context &io_ctx, ice_sender &ice_sender, ip::udp::endpoint &glob_self_ep, direct_routing_table_controller &d_routing_table_controller, ice_observer_strage &obs_strage ) :
   _io_ctx( io_ctx )
  , _ice_sender( ice_sender )
  , _glob_self_ep( glob_self_ep )
  , _d_routing_table_controller( d_routing_table_controller )
  , _obs_strage( obs_strage )
{
  return;
}

int signaling_server::income_message( std::shared_ptr<message> msg, ip::udp::endpoint &ep )
{
  ice_message ice_msg( *(msg->get_param("ice_agent")) );
  auto signaling_message_controller = ice_msg.get_signaling_message_controller();

  // 1. destが自身であれば -> signling_ok メッセージを送信
  ip::udp::endpoint dest_ep = signaling_message_controller.get_dest_endpoint(); // メッセージから目的ipを取り出し

  if( dest_ep == _ice_sender.get_self_endpoint() )
  {
	ip::udp::endpoint src_ep = signaling_message_controller.get_src_endpoint();

	// signaling_responseメッセージを送信する
	auto msg_controller = ice_msg.get_signaling_message_controller();
	msg_controller.set_sub_protocol( ice_message::signaling::sub_protocol_t::response );

	observer<class signaling_response> sgnl_response_obs( _io_ctx, _ice_sender, _glob_self_ep, _d_routing_table_controller );

	std::cout << "仮 仮" << "\n";
	ip::address_v4 temp_v4 = ip::address_v4::from_string("127.0.0.1");
	src_ep.address(temp_v4);

	_ice_sender.async_ice_send( src_ep, ice_msg // レスポンスの送信
		, std::bind( &signaling_response::init
		, *(sgnl_response_obs.get_raw())
		, std::placeholders::_1 )
	  );

	#if SS_VERBOSE
	std::cout << "(init observer)[signaling response] send -> " << src_ep << "\n";
	#endif

	_obs_strage.add_observer<class signaling_response>( sgnl_response_obs ); // ストレージに追加する
	return 0;
  }

  /* この先転送系の処理 */

  observer<class signaling_relay> sgnl_relay_obs( _io_ctx, _ice_sender, _glob_self_ep, _d_routing_table_controller ); // relayオブザーバーの作成
  signaling_message_controller.add_relay_endpoint( _ice_sender.get_self_endpoint() ); // 自身も中継ノードに追加する
  signaling_message_controller.set_sub_protocol( ice_message::signaling::sub_protocol_t::relay );

  // 自身のルーティングテーブルにdest_epがあれば,特に他ノードには転送させず,直接送信する
  if( _d_routing_table_controller.is_exist(dest_ep) )  // signaling転送先が自身のルーティングテーブルに存在する場合
  {
	_ice_sender.async_ice_send( dest_ep, ice_msg, // 直接相手アドレスに転送する
		std::bind( &signaling_relay::init // 送信に失敗したらオブザーバーは早めに破棄される
		  , *(sgnl_relay_obs.get_raw()) )
		);

	#if SS_VERBOSE
	std::cout << "(observer)[signaling relay] relay fin -> " << dest_ep << "\n";
	#endif

	_obs_strage.add_observer<class signaling_relay>( sgnl_relay_obs );
	return 0;
  }

  std::vector<ip::udp::endpoint> relay_eps = signaling_message_controller.get_relay_endpoints(); // リレーしたノードを無視するようにする

  // 2. destが自身でなく,ttlが有効であれば他ノードに転送してobserverをセットする
  int ttl = signaling_message_controller.get_ttl();
  if( ttl <= 0 ) return 0; // ttlが0以下だったらメッセージを破棄する

  auto forward_eps = _d_routing_table_controller.collect_node( dest_ep, 3, relay_eps ); // 転送先
  for( auto itr : forward_eps )
	signaling_message_controller.add_relay_endpoint(itr); // 転送先をリレーノードとして追加する

  // 転送s
  for( auto itr : forward_eps )
  {
	_ice_sender.async_ice_send( itr, ice_msg
		, std::bind( &signaling_relay::init // 送信に失敗したら早めに削除される
		  , *(sgnl_relay_obs.get_raw()) )
		);

	#if SS_VERBOSE
	std::cout << "(observer)[signaling relay] relay -> " << itr << "\n";
	#endif
  }

  _obs_strage.add_observer<class signaling_relay>( sgnl_relay_obs );
  return 0;
}

void signaling_server::on_send_done( const boost::system::error_code &ec )
{
  #if SS_VERBOSE
  if( !ec ) std::cout << "(signaling server) send done" << "\n";
  else std::cout << "(signaling server) send error" << "\n";
  #endif
}

ice_message signaling_server::format_relay_msg( ice_message &base_msg )
{
  auto msg_controller = base_msg.get_signaling_message_controller();
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
	_ice_sender.async_send( dest_ep, root_param, payload
		, std::bind( &signaling_server::on_send_done, this, std::placeholders::_1)
	  );
	return;
  }

  observer<class signaling_request> sgnl_req_obs( _io_ctx, _ice_sender, _glob_self_ep, _d_routing_table_controller );
  sgnl_req_obs.init( dest_ep, root_param, payload );

  _obs_strage.add_observer<class signaling_request>( sgnl_req_obs ); // store
}

signaling_server::s_send_func signaling_server::get_signaling_send_func()
{
  return std::bind(
		&signaling_server::signaling_send
		, this
		, std::placeholders::_1
		, std::placeholders::_2
		, std::placeholders::_3
	  );
}



}; // namespace ice
}; // namespace ss
