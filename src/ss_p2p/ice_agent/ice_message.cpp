#include <ss_p2p/ice_agent/ice_message.hpp>
#include <ss_p2p/observer.hpp>


namespace ss
{
namespace ice
{


ice_message::signaling_message_controller::signaling_message_controller( json &body ) : _body(body)
{
  sub_protocol = _body["sub_protocol"];
}

void ice_message::signaling_message_controller::add_relay_endpoint( ip::udp::endpoint ep )
{
  _body["relay_eps"].push_back( endpoint_to_str(ep) );
}

void ice_message::signaling_message_controller::set_sub_protocol( signaling_message_controller::sub_protocol_t p )
{
  _body["sub_protocol"] = p;
}

ice_message::signaling_message_controller::sub_protocol_t ice_message::signaling_message_controller::get_sub_protocol()
{
  return _body["sub_protocol"];
}

std::vector<ip::udp::endpoint> ice_message::signaling_message_controller::get_relay_endpoints()
{
  std::vector<ip::udp::endpoint> ret;
  for( int i=0; i<_body["relay_eps"].size(); i++ ){
	std::string ep_str = _body["relay_eps"][i].get<std::string>();
	ret.push_back( str_to_endpoint( ep_str ) );
  }
  return ret;
}

ip::udp::endpoint ice_message::signaling_message_controller::get_dest_endpoint() const
{
  std::string dest_ip = _body["dest_ip"];
  unsigned short dest_port = _body["dest_port"];

  return ss::addr_pair_to_endpoint( dest_ip, dest_port );
}

int ice_message::signaling_message_controller::get_ttl() const
{
  return _body["ttl"].get<int>();
}

void ice_message::signaling_message_controller::decrement_ttl()
{
  _body["tt"] = _body["ttl"].get<int>() - 1;
}


ice_message::ice_message( std::string protocol )
{
  if( protocol == "signaling" ){
	protocol = protocol_t::signaling;
  }
  else if( protocol == "stun" ){
	protocol = protocol_t::stun;
  }
  else{
	protocol = protocol_t::none;
  }


  // 必須項目
  json relay_eps = json::array();
  _body["relay_eps"] = relay_eps;
}

ice_message ice_message::_signaling_()
{
  ice_message ret("signaling");
  return ret;
}

ice_message ice_message::_stun_()
{
  ice_message ret("stun");
  return ret;
}

ice_message::ice_message( json &from )
{
  _body = from;

  if( _body["protocol"] == "signaling" ) protocol = protocol_t::signaling;
  else if( _body["protocol"] == "stun" ) protocol = protocol_t::stun;
  else protocol = protocol_t::none;
}

ice_message::protocol_t ice_message::get_protocol() const
{
  return protocol;
}

ice_message::signaling_message_controller ice_message::get_sgnl_msg_controller()
{
  return ice_message::signaling_message_controller(_body);
}

const json ice_message::encode()
{
  return _body;
}



}; // namespace ice
}; // namespace ss
