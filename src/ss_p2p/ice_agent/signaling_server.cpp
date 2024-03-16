#include <ss_p2p/ice_agent/signaling_server.hpp>


namespace ss
{
namespace ice
{


signaling_server::signaling_server( io_context &io_ctx, deadline_timer &d_timer, ice_agent::ice_sender &ice_sender, direct_routing_table_controller &d_routing_table_controller ) : 
  _io_ctx( io_ctx ) , 
  _d_timer( d_timer ) ,
  _ice_sender( ice_sender ) ,
  _d_routing_table_controller( d_routing_table_controller )
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

void signaling_server::handle_message( std::shared_ptr<message> msg )
{
  ice_message ice_msg( *(msg->get_param("ice_agent")) );
  
  // 1. destが自身であれば -> signling_ok メッセージを送信 
  ip::udp::endpoint dest_ep = addr_pair_to_endpoint( ice_msg.get_param<std::string>("dest_ipv4"), ice_msg.get_param<std::uint16_t>("dest_port") );
  if( dest_ep == _ice_sender.get_host_endpoint() )
  {
	// signaling_doneオブザーバーを設置する
	return;
  }

  auto sgnl_msg_controller = ice_msg.get_sgnl_msg_controller();
  std::vector<ip::udp::endpoint> relay_eps = sgnl_msg_controller.get_relay_endpoints(); // リレーしたノードを無視するようにする

  // 2. destが自身でなく,ttlが有効であれば他ノードに転送してobserverをセットする
  int ttl = ice_msg.get_param<int>("ttl");
  if( ttl > 0 )
  {
	auto forward_nodes = _d_routing_table_controller.collect_nodes( dest_ep, 3, relay_eps ); // 転送先
	for( auto itr : forward_nodes )	
	  sgnl_msg_controller.add_relay_endpoint(itr);
  }

  ss::message::app_id id = {'a','b','c','d','e','f','g','h'}; // 仮
  ss::message _msg(id);
  ss::message::encoded_message enc_msg = ss::message::encode( _msg );

  _ice_sender.ice_send( dest_ep, ice_msg, std::bind( &signaling_server::set_signaling_relay_observer, this, std::placeholders::_1) );
}

ice_message signaling_server::format_relay_msg( ice_message &base_msg )
{
  auto msg_controller = base_msg.get_sgnl_msg_controller();
  msg_controller.add_relay_endpoint( _ice_sender.get_host_endpoint() );

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
