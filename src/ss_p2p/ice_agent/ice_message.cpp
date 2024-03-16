#include <ss_p2p/ice_agent/ice_message.hpp>
#include <ss_p2p/observer.hpp>


namespace ss
{
namespace ice
{


ice_message::signaling_message_controller::signaling_message_controller( json &body ) : _body(body)
{
  return;
}

void ice_message::signaling_message_controller::add_relay_endpoint( ip::udp::endpoint ep )
{
  _body["relay_eps"].push_back( endpoint_to_str(ep) );
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


ice_message::ice_message( std::string protocol )
{
  if( protocol == "signaling_open" ){
	protocol = protocol_t::req_signaling_open;
  }
  else if( protocol == "stun_request" ){
	protocol = protocol_t::req_stun;
  }
  else if( protocol == "stun_response" ){
	protocol = protocol_t::res_stun;
  }
  else{
	protocol = protocol_t::none;
  }


  // 必須項目
  json relay_eps = json::array();
  _body["relay_eps"] = relay_eps;
}

ice_message ice_message::_signaling_open_()
{
  ice_message ret("signaling_open");
  return ret;
}

ice_message ice_message::_stun_request_()
{
  ice_message ret("stun_request");
  return ret;
}

ice_message ice_message::_stun_response_()
{
  ice_message ret("stun_response");
  return ret;
}

ice_message::ice_message( json &from )
{
  _body = from;

  if( _body["protocol"] == "signaling_open" ) protocol = protocol_t::req_signaling_open;
}

ice_message::protocol_t ice_message::get_protocol() const
{
  return protocol;
}

ice_message::signaling_message_controller ice_message::get_sgnl_msg_controller()
{
  return ice_message::signaling_message_controller(_body);
}

template <typename T>
T ice_message::get_param( std::string key )
{
  auto value = _body[key];
  return value.get<T>();

  if constexpr (std::is_same_v<T, std::string>){
	return value.get<std::string>();
  }
  else if constexpr (std::is_same_v<T, observer_id>)
  {
	return str_to_observer_id( value.get<std::string>() );
  }
  else if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool> )
  {
	return value.get<T>();
  }
  return T{};
}

const json ice_message::encode()
{
  return _body;
}



}; // namespace ice
}; // namespace ss
