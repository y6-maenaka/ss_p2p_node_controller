#include <ss_p2p/kademlia/message.hpp>


namespace ss
{
namespace kademlia
{


message::message( std::string query_type )
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

message::message( json& k_msg )
{
  _body = k_msg;
}

bool message::set_param( std::string key, std::string value )
{
  auto itr = std::find( kademlia_message_params.begin(), kademlia_message_params.end(), key );
  if( itr == kademlia_message_params.end() ) return false;
  
  _body[key] = value;
  return true;
}

message message::request()
{
  message ret("request");
  return ret;
}

message message::response()
{
  message ret("response");
  return ret;
}

bool message::is_request() const
{
  // 必ず検証済みであること
  return _body["query_type"] == "request";
}

bool message::validate() const
{ 
  if( !(_body.contains("query_type")) ) return false;
  if( !(_body.contains("rpc")) ) return false;

  if( _body["rpc"] != "request" || _body["rpc"] != "response" ) return false;
  return true;
}

void message::print()
{
  std::cout << _body << "\n";
}


}; // namesapce kademlia
}; // namesapce ss
