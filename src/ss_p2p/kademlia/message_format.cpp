#include <ss_p2p/kademlia/message_format.hpp>


namespace ss
{
namespace kademlia
{


message_format::message_format( std::string msg_type )
{
  if( msg_type == "request" )
  {
	_body["msg_type"] = "request";
	_body["rpc"] = json::object();
  }
  if( msg_type == "response" )
  {
	_body["msg_type"] = "response";
	_body["rpc"] = json::object();
  }

  return;
}

void message_format::hello()
{
  std::cout << "hello world" << "\n";
}

bool message_format::set_param( std::string key, std::string value )
{
  auto itr = std::find( kademlia_message_params.begin(), kademlia_message_params.end(), key );
  if( itr == kademlia_message_params.end() ) return false;
  
  _body[key] = value;
  return true;
}

message_format message_format::request()
{
  message_format ret("request");
  return ret;
}

message_format message_format::response()
{
  message_format ret("response");
  return ret;
}

void message_format::print()
{
  std::cout << _body << "\n";
}


}; // namesapce kademlia
}; // namesapce ss
