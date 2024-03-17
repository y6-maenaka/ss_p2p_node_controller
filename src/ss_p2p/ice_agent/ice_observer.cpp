#include <ss_p2p/ice_agent/ice_observer.hpp>


namespace ss
{
namespace ice
{


ice_observer::ice_observer( io_context &io_ctx, deadline_timer &d_timer, ice_agent::ice_sender &ice_sender, direct_routing_table_controller &d_routing_table_controller ) :
  base_observer( io_ctx, d_timer ) 
  , _ice_sender(ice_sender)
  , _d_routing_table_controller( d_routing_table_controller )
{
  return;
}


signaling_request::signaling_request( io_context &io_ctx
	, deadline_timer &d_timer
	, ice_agent::ice_sender &ice_sender
	, direct_routing_table_controller &d_routing_table_controller
	// , ip::udp::endpoint &glob_self_ep
	, ip::udp::endpoint &dest_ep 
	, std::string param
	, std::span<char> payload ) :
  ice_observer( io_ctx, d_timer, ice_sender, d_routing_table_controller ) 
  , _msg_cache( dest_ep, param, payload )
{
  return;
}

signaling_request::msg_cache::msg_cache( ip::udp::endpoint &ep, std::string param, std::span<char> payload ) :
  ep(ep)
  , param(param)
  , payload(payload)
{
  return;
}

void signaling_request::init( ip::udp::endpoint &glob_elf_ep ) // ホスト->ピア方向へのNatを開通させるためのダミーメッセージを送信する
{
  if( _d_routing_table_controller.is_exist(_msg_cache.ep) )
  { // ルーティングテーブルに相手のアドレスが存在していれば普通に送信する
	_send_func( _msg_cache.ep, "ice_agent", _msg_cache.payload );
  }

  // nat開通用にダミーメッセージを送信する
  std::string msg_str = "signaling dummy"; std::span msg( msg_str.data(), msg_str.size() );
  _send_func( _msg_cache.ep, "ice_agent", msg );

  #if SS_VERBOSE
  std::cout << "(init observer) signaling_request" << "\n";
  #endif

}

json signaling_request::format_request_msg( ip::udp::endpoint &src_ep, ip::udp::endpoint &dest_ep )
{
  json ret;
  ret["protocol"] = "signaling";
  ret["type"] = "request";

  auto exs_dest_ep = extract_endpoint( dest_ep );
  auto exs_src_ep = extract_endpoint( src_ep );
  ret["dest_ipv4"] = exs_dest_ep.first;
  ret["dest_port"] = exs_dest_ep.second;

  ret["src_ipv4"] = exs_src_ep.first;
  ret["src_port"] = exs_src_ep.second;

  ret["ttl"] = DEFAULT_SIGNALING_OPEN_TTL;

  return ret;
}

void signaling_request::income_message( message &msg )
{
  ice_message ice_msg( *(msg.get_param("ice_agent")) ); // ここでエラーが発生することは多分ない
  auto msg_controller = ice_msg.get_sgnl_msg_controller();

  if( msg_controller.get_sub_protocol() == ice_message::signaling_message_controller::sub_protocol_t::response ) // シグナリング成功レスポンスの場合
  {
	_ice_sender.send( _msg_cache.ep, _msg_cache.param, _msg_cache.payload
		, std::bind( &signaling_request::on_traversal_done, this, std::placeholders::_1) ); // 疎通後メッセージを送信する
  }

  // relay だったらそれ以上転送しない : 破棄する

  // ice_messageからip, portの取得
  ip::udp::endpoint dest_ep = addr_pair_to_endpoint( ice_msg.get_param<std::string>("dest_ipv4"), ice_msg.get_param<std::uint16_t>("dest_port") );
  
  /* signaling_openの目的アドレスが自身の場合
     1. 到達したメッセージをリクエスト元に送信する
	 2. signaling_done(observer)を設定する
  */
  if( dest_ep == _ice_sender.get_self_endpoint() ) // 
  {
	// signaling_doneオブザーバーを設置する
	ice_message ice_res_msg = ice_message::_signaling_();
	auto msg_controller = ice_res_msg.get_sgnl_msg_controller();
	// msg_controller.
	return;
  }


  // メッセージの検証
  // ip::udp::endpoint dest_ip = addr_pair_to_endpoint( ice_msg->get_param<std::string>("src_ipv4"), (ice_msg->get_param<std::uint16_t>("src_port") );

  // _send_func( _msg_cache.ep, _msg_cache.param, _msg_cache.payload );


  // 1. ターゲットが自身の場合 -> 応答を送信する
   

  // 2. ターゲットが自身でない場合 -> routing_tableから数個コードを取り出してそこに転送する
  // 2.1. 有効期限が切れていれば削除する 

  // _send_func( ep, msg );
}



signaling_response::signaling_response( io_context &io_ctx, deadline_timer &d_timer, ice_agent::ice_sender &ice_sender, direct_routing_table_controller &d_routing_table_controller ) :
  ice_observer( io_ctx, d_timer, ice_sender, d_routing_table_controller )
{
  return;
}

void signaling_response::init()
{
  #if SS_VERBOSE
  std::cout << "(init observer) signaling_response" << "\n";
  #endif
  
  extend_expire_at( 30 ); // 有効期限を30秒延長する
}


signaling_relay::signaling_relay( io_context &io_ctx, deadline_timer &d_timer, ice_agent::ice_sender &ice_sender, direct_routing_table_controller &d_routing_table_controller ) :
  ice_observer( io_ctx, d_timer, ice_sender, d_routing_table_controller )
{
  return;
}

void signaling_relay::init()
{
  #if SS_VERBOSE
  std::cout << "(init observer) signaling_relay" << "\n";
  #endif
}


}; // namespace ice
}; // namespace ss
