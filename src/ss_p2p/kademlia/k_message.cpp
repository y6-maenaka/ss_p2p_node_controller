#include <ss_p2p/kademlia/k_message.hpp>


namespace ss
{
namespace kademlia
{


k_message::k_message( std::string query_type )
{
  if( query_type == "request" )
  {
	_body["query_type"] = "request";
	_body["rpc"] = json::object();
  }
  if( query_type == "response" )
  {
	_body["query_type"] = "response";
	_body["rpc"] = json::object();
  }

  return;
}

k_message::k_message( json& k_msg )
{
  _body = k_msg;
}

bool k_message::set_param( std::string key, std::string value )
{
  auto itr = std::find( kademlia_message_params.begin(), kademlia_message_params.end(), key );
  if( itr == kademlia_message_params.end() ) return false;
  
  _body[key] = value;
  return true;
}

k_message k_message::request()
{
  k_message ret("request");
  return ret;
}

k_message k_message::response()
{
  k_message ret("response");
  return ret;
}

bool k_message::is_request() const
{
  // 必ず検証済みであること
  return _body["query_type"] == "request";
}

bool k_message::validate() const
{ 
  if( !(_body.contains("query_type")) ) return false;
  if( !(_body.contains("rpc")) ) return false;

  if( _body["rpc"] != "request" || _body["rpc"] != "response" ) return false;
  return true;
}

json k_message::export_json() const
{
  return _body;
}

void k_message::print()
{
  std::cout << _body << "\n";
}


}; // namesapce kademlia
}; // namesapce ss
