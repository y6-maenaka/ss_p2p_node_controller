#include <ss_p2p/ice_agent/ice_observer.hpp>


namespace ss
{
namespace ice
{


ice_observer::ice_observer( io_context &io_ctx, ice_agent::ice_sender &ice_sender, direct_routing_table_controller &d_routing_table_controller ) :
  base_observer( io_ctx )
  , _ice_sender(ice_sender)
  , _d_routing_table_controller( d_routing_table_controller )
{
  return;
}


signaling_request::signaling_request( io_context &io_ctx, ice_agent::ice_sender &ice_sender, direct_routing_table_controller &d_routing_table_controller ) :
  ice_observer( io_ctx, ice_sender, d_routing_table_controller )
{
  return;
}

void signaling_request::init( ip::udp::endpoint &dest_ep, std::string param, json payload ) // ホスト->ピア方向へのNatを開通させるためのダミーメッセージを送信する
{
  // nat開通用にダミーメッセージを送信する
  std::string dummy_msg_str = "signaling dummy"; json dummy_msg_json = dummy_msg_str;
  _ice_sender.send( _msg_cache.ep, "dummy", dummy_msg_json
		, std::bind( &signaling_request::on_send_success, this, std::placeholders::_1 )
	);

  _msg_cache.ep = dest_ep;
  _msg_cache.param = param;
  _msg_cache.payload = payload;


  ice_message ice_msg = ice_message::_signaling_();
  auto msg_controller = ice_msg.get_sgnl_msg_controller();
  msg_controller.set_sub_protocol( ice_message::signaling_message_controller::sub_protocol_t::request );

  std::vector<ip::udp::endpoint> forward_eps = _d_routing_table_controller.collect_nodes( dest_ep, 3/*適当*/ );
  for( auto itr : forward_eps )
  {
	_ice_sender.ice_send( itr, ice_msg
	  , std::bind( &observer<signaling_request>::on_send_success
		, this
		, std::placeholders::_1 ) 
	  );
  }

  #if SS_VERBOSE
  std::cout << "(init observer) signaling_request" << "\n";
  #endif
}

void signaling_request::on_traversal_done( const boost::system::error_code &ec )
{
  _done = true;
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
  if( _done ) return; // 処理済みであれば特に何もしない

  ice_message ice_msg( *(msg.get_param("ice_agent")) ); // ここでエラーが発生することは多分ない
  auto msg_controller = ice_msg.get_sgnl_msg_controller();

  if( msg_controller.get_sub_protocol() == ice_message::signaling_message_controller::sub_protocol_t::response ) // シグナリング成功レスポンスの場合
  {
	_ice_sender.send( _msg_cache.ep, _msg_cache.param, _msg_cache.payload
		, std::bind( &signaling_request::on_traversal_done, this, std::placeholders::_1) ); // 疎通後メッセージを送信する
  }

  // ice_messageからip, portの取得
  ip::udp::endpoint dest_ep = msg_controller.get_dest_endpoint(); 
  if( dest_ep != _ice_sender.get_self_endpoint() ) return; // 最終検証 恐らくここで相違が発生することはない
  
  ice_message ice_res_msg = ice_message::_signaling_();
  auto msg_res_controller = ice_res_msg.get_sgnl_msg_controller();
  msg_res_controller.set_sub_protocol( ice_message::signaling_message_controller::sub_protocol_t::response ); // サブプロトコルの設定

  _ice_sender.send( _msg_cache.ep, _msg_cache.param,_msg_cache.payload
	  , std::bind( &signaling_request::on_traversal_done, this , std::placeholders::_1 )
	);
  return;
}


signaling_response::signaling_response( io_context &io_ctx, ice_agent::ice_sender &ice_sender, direct_routing_table_controller &d_routing_table_controller ) :
  ice_observer( io_ctx, ice_sender, d_routing_table_controller )
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

void signaling_response::income_message( ss::message &msg )
{
  // 一度処理しているため特に処理しない
  extend_expire_at( 20 ); 
}


signaling_relay::signaling_relay( io_context &io_ctx, ice_agent::ice_sender &ice_sender, direct_routing_table_controller &d_routing_table_controller ) :
  ice_observer( io_ctx, ice_sender, d_routing_table_controller )
{
  return;
}

void signaling_relay::init()
{
  #if SS_VERBOSE
  std::cout << "(init observer) signaling_relay" << "\n";
  #endif

  extend_expire_at( 20 );
}

void signaling_relay::income_message( message &msg )
{
  // 一度処理しているため特に処理しない
  extend_expire_at( 20 );
}


}; // namespace ice
}; // namespace ss
