#include <ss_p2p/ice_agent/ice_observer.hpp>
#include <ss_p2p/ice_agent/ice_agent.hpp>


namespace ss
{
namespace ice
{


signaling_observer::signaling_observer( io_context &io_ctx, ice_sender &ice_sender, ip::udp::endpoint &glob_self_ep, ss::kademlia::direct_routing_table_controller &d_routing_table_controller ) :
  base_observer( io_ctx )
  , _ice_sender(ice_sender)
  , _glob_self_ep( glob_self_ep )
  , _d_routing_table_controller( d_routing_table_controller )
{
  return;
}


signaling_request::signaling_request( io_context &io_ctx, ice_sender &ice_sender, ip::udp::endpoint &glob_self_ep, ss::kademlia::direct_routing_table_controller &d_routing_table_controller ) :
  signaling_observer( io_ctx, ice_sender, glob_self_ep, d_routing_table_controller )
{
  return;
}

void signaling_request::init( ip::udp::endpoint &dest_ep, std::string param, json payload ) // ホスト->ピア方向へのNatを開通させるためのダミーメッセージを送信する
{
  std::string dummy_msg_str = "signaling dummy"; json dummy_msg_json = dummy_msg_str;

  _ice_sender.async_send( dest_ep, "dummy", dummy_msg_json
	  , std::bind( &signaling_request::on_send_done, this, std::placeholders::_1 )
  ); // nat開通用にダミーメッセージを送信する

  // キャッシュの登録
  _msg_cache.ep = dest_ep;
  _msg_cache.param = param;
  _msg_cache.payload = payload;

  ice_message ice_msg = ice_message::_signaling_(); // シグナリングメッセージの作成
  ice_msg.set_observer_id( signaling_observer::get_id() );
  auto msg_controller = ice_msg.get_signaling_message_controller(); // ice_messageからsignaling_mes_controllerを取得
  msg_controller.set_sub_protocol( ice_message::signaling::sub_protocol_t::request ); // サブプロトコルの設定
  msg_controller.set_dest_endpoint( dest_ep );
  msg_controller.set_src_endpoint( _glob_self_ep );

  std::vector<ip::udp::endpoint> forward_eps = _d_routing_table_controller.collect_node( dest_ep, 3/*適当*/ );
  for( auto itr : forward_eps )
  {
	_ice_sender.async_ice_send( itr, ice_msg
	  , std::bind( &signaling_request::on_send_done
		, this
		, std::placeholders::_1 )
	  );

	#if SS_VERBOSE
	std::cout << "(init observer)[signaling request] send -> " << itr << "\n";
	#endif
  }
  return;
}

void signaling_request::on_send_done( const boost::system::error_code &ec )
{
  #if SS_VERBOSE
  if( !ec ) std::cout << "signaling_request::send success" << "\n";
  else std::cout << "signaling_request::send error" << "\n";
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

int signaling_request::income_message( message &msg )
{
  if( _done ) return 0; // 処理済みであれば特に何もしない

  ice_message ice_msg( *(msg.get_param("ice_agent")) ); // ここでエラーが発生することは多分ない
  auto msg_controller = ice_msg.get_signaling_message_controller();

  if( msg_controller.get_sub_protocol() == ice_message::signaling::sub_protocol_t::response ) // シグナリング成功レスポンスの場合
  {
	// NAT解決
	_ice_sender.async_send( _msg_cache.ep, _msg_cache.param, _msg_cache.payload
		, std::bind( &signaling_request::on_traversal_done, this, std::placeholders::_1) ); // 疎通後メッセージを送信する
	return 0;
  }

  // ice_messageからip, portの取得
  ip::udp::endpoint dest_ep = msg_controller.get_dest_endpoint();
  if( dest_ep != _ice_sender.get_self_endpoint() ) return 0; // 最終検証 恐らくここで相違が発生することはない

  ice_message ice_res_msg = ice_message::_signaling_();
  auto msg_res_controller = ice_res_msg.get_signaling_message_controller();
  msg_res_controller.set_sub_protocol( ice_message::signaling::sub_protocol_t::response ); // サブプロトコルの設定

  _ice_sender.async_send( _msg_cache.ep, _msg_cache.param,_msg_cache.payload
	  , std::bind( &signaling_request::on_traversal_done, this , std::placeholders::_1 )
	);
  return 0;
}

void signaling_request::print() const
{
  std::cout << "[observer] (signaling-request) " << "<" << _id << ">";
  std::cout << " [ at: "<< signaling_observer::get_expire_time_left() <<" ]";
}


signaling_response::signaling_response( io_context &io_ctx, ice_sender &ice_sender, ip::udp::endpoint &glob_self_ep, direct_routing_table_controller &d_routing_table_controller ) :
  signaling_observer( io_ctx, ice_sender, glob_self_ep, d_routing_table_controller )
{
  return;
}

void signaling_response::init( const boost::system::error_code &ec )
{
  #if SS_VERBOSE
  if( !ec ) std::cout << "signaling_response::init success" << "\n";
  else std::cout << "signaling_response::init error" << "\n";
  #endif

  if( !ec ) extend_expire_at( 30 ); // 有効期限を30秒延長する
}

int signaling_response::income_message( ss::message &msg )
{
  // 一度処理しているため特に処理しない
  extend_expire_at( 20 );
  return 0;
}

void signaling_response::print() const
{
  std::cout << "[observer] (signaling-response) " << "<" << _id << ">";
  std::cout << " [ at: "<< signaling_observer::get_expire_time_left() <<" ]";
}


signaling_relay::signaling_relay( io_context &io_ctx, ice_sender &ice_sender, ip::udp::endpoint &glob_self_ep, direct_routing_table_controller &d_routing_table_controller ) :
  signaling_observer( io_ctx, ice_sender, glob_self_ep, d_routing_table_controller )
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

int signaling_relay::income_message( message &msg )
{
  // 一度処理しているため特に処理しない
  extend_expire_at( 20 );
  return 0;
}

void signaling_relay::print() const
{
  std::cout << "[observer] (signaling-relay) " << "<" << _id << ">";
  std::cout << " [ at: "<< signaling_observer::get_expire_time_left() <<" ]";
}


stun_observer::stun_observer( io_context &io_ctx, ice_sender &ice_sender, ss::kademlia::direct_routing_table_controller &d_routing_table_controller ) :
  base_observer( io_ctx )
  , _ice_sender( ice_sender )
  , _d_routing_table_controller( d_routing_table_controller )
{
  return;
}

binding_request::binding_request( io_context &io_ctx, ice_sender &ice_sender, ss::kademlia::direct_routing_table_controller &d_routing_table_controller ) :
  stun_observer( io_ctx, ice_sender, d_routing_table_controller )
  , _timer( io_ctx )
  , _timeout_count( 0 )
  , _reliability( MAXIMUM_BINDING_REQUEST_RELIABILITY/2 )
{
  return;
}

void binding_request::init( stun_server::sr_object &sr )
{
  _sr = &sr; // sr objectのセット
}

void binding_request::add_requested_ep( ip::udp::endpoint ep )
{
  ip::udp::endpoint init_ep( ip::address::from_string("0.0.0.0"), 0 );
  _responses.push_back( std::make_pair(ep, init_ep) );
}

void binding_request::add_response( ip::udp::endpoint &src_ep, ip::udp::endpoint response_ep )
{
  auto itr = std::find_if( _responses.begin(), _responses.end(), [src_ep]( const std::pair<ip::udp::endpoint, ip::udp::endpoint> entry ){
		  return entry.first == src_ep;
	  });
  if( itr == _responses.end() ) return;
  (*itr).second = response_ep;
}

int binding_request::income_message( message &msg )
{
  // 送信したリクエストのうち3/1が集まって多数決を取る
  ice_message ice_msg( *(msg.get_param("ice_agent")) );
  auto msg_controller = ice_msg.get_stun_message_controller();
  ip::udp::endpoint global_ep = msg_controller.get_global_ep();

  _sr->update_state( stun_server::sr_object::state_t::done, global_ep );

  #if SS_VERBOSE
  std::cout << "(stun observer) update state" << "\n";
  #endif

  return 0;
}

void binding_request::timeout()
{
  _sr->update_state( stun_server::sr_object::state_t::notfound ); // 失敗
}

void binding_request::print() const
{
  std::cout << "[observer] (binding-request) " << "<" << _id << ">";
  std::cout << " [ at: "<< stun_observer::get_expire_time_left() <<" ]";
}


}; // namespace ice
}; // namespace ss
