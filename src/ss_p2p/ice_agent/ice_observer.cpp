#include <ss_p2p/ice_agent/ice_observer.hpp>


namespace ss
{
namespace ice
{


ice_observer::ice_observer( io_context &io_ctx, deadline_timer &d_timer, direct_routing_table_controller &d_routing_table_controller ) :
  base_observer( io_ctx, d_timer ) ,
  _d_routing_table_controller( d_routing_table_controller )
{
  return;
}


signaling_open::signaling_open( io_context &io_ctx
	, deadline_timer &d_timer
	, direct_routing_table_controller &d_routing_table_controller
	// , ip::udp::endpoint &glob_self_ep
	, ip::udp::endpoint &dest_ep 
	, std::string param
	, std::span<char> payload ) :
  ice_observer( io_ctx, d_timer, d_routing_table_controller ) 
  , _msg_cache( dest_ep, param, payload )
{
  return;
}

signaling_open::msg_cache::msg_cache( ip::udp::endpoint &ep, std::string param, std::span<char> payload ) :
  ep(ep)
  , param(param)
  , payload(payload)
{
  return;
}

void signaling_open::init( ip::udp::endpoint &glob_elf_ep ) // ホスト->ピア方向へのNatを開通させるためのダミーメッセージを送信する
{
  if( _d_routing_table_controller.is_exist(_msg_cache.ep) )
  { // ルーティングテーブルに相手のアドレスが存在していれば普通に送信する
	_send_func( _msg_cache.ep, "ice_agent", _msg_cache.payload );
  }

  // nat開通用にダミーメッセージを送信する
  std::string msg_str = "signaling dummy"; std::span msg( msg_str.data(), msg_str.size() );
  _send_func( _msg_cache.ep, "ice_agent", msg );

}

json signaling_open::format_request_msg( ip::udp::endpoint &src_ep, ip::udp::endpoint &dest_ep )
{
  json ret;
  ret["protocol"] = "signaling_open";

  auto exs_dest_ep = extract_endpoint( dest_ep );
  auto exs_src_ep = extract_endpoint( src_ep );
  ret["dest_ipv4"] = exs_dest_ep.first;
  ret["dest_port"] = exs_dest_ep.second;

  ret["src_ipv4"] = exs_src_ep.first;
  ret["src_port"] = exs_src_ep.second;

  ret["ttl"] = DEFAULT_SIGNALING_OPEN_TTL;

  return ret;
}

void signaling_open::handle_response( std::shared_ptr<ss::message> msg )
{
  ice_message ice_msg( *(msg->get_param("ice_agent")) );
  std::string ip_v4 = ice_msg.get_param<std::string>("src_ipv4");

  // メッセージの検証
  // ip::udp::endpoint dest_ip = addr_pair_to_endpoint( ice_msg->get_param<std::string>("src_ipv4"), (ice_msg->get_param<std::uint16_t>("src_port") );

  // _send_func( _msg_cache.ep, _msg_cache.param, _msg_cache.payload );


  // 1. ターゲットが自身の場合 -> 応答を送信する
   

  // 2. ターゲットが自身でない場合 -> routing_tableから数個コードを取り出してそこに転送する
  // 2.1. 有効期限が切れていれば削除する 

  // _send_func( ep, msg );
}


}; // namespace ice
}; // namespace ss
