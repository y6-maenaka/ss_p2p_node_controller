#include <ss_p2p/kademlia/k_message.hpp>


namespace ss
{
namespace kademlia
{


k_message::k_message( std::string message_type )
{
  if( message_type == "request" )
  {
	_body["type"] = "request";
	_body["rpc"] = json::object();
  }
  if( message_type == "response" )
  {
	_body["type"] = "response";
	_body["rpc"] = json::object();
  }

  return;
}

k_message::k_message( json& k_msg )
{
  _body = k_msg;
}

void k_message::set_param( std::string key, std::string value )
{
  _body[key] = value;
}

k_message k_message::_request_( k_message::rpc r )
{
  k_message ret("request");
  ret.set_rpc(r);
  return ret;
}

k_message k_message::_response_( k_message::rpc r )
{
  k_message ret("response");
  ret.set_rpc(r);
  return ret;
}

void k_message::set_rpc( k_message::rpc r )
{
  switch( r )
  {
	case k_message::ping :
	  {
		_body["rpc"] = "ping";
		break;
	  }
	case k_message::find_node :
	  {
		_body["rpc"] = "find_node";
		break;
	  }
  }
  return;
}

k_message::rpc k_message::get_rpc() const
{
  std::cout << "要修正" << "\n";
  if( _body["rpc"] == "ping" ) return k_message::rpc::ping;
  else return k_message::rpc::find_node;
}

k_message::message_type k_message::get_message_type() const
{
  std::cout << "要修正" << "\n";
  if( _body["type"] == "request" ) return k_message::message_type::request;
  return k_message::message_type::response;
}

void k_message::set_message_type( k_message::message_type t )
{
  switch ( t )
  {
	case k_message::message_type::request : 
	  {
		_body["type"] = "request";
	  }
	case k_message::message_type::response :
	  {
		_body["type"] = "response";
	  }
  }
  return;
}

bool k_message::is_request() const
{
  // 必ず検証済みであること
  return _body["type"] == "request";
}

bool k_message::validate() const
{ 
  if( !(_body.contains("type")) ) return false;
  if( !(_body.contains("rpc")) ) return false;

  // if( _body["rpc"] != "request" || _body["rpc"] != "response" ) return false;
  return true;
}

const json k_message::encode()
{
  return _body;
}

void k_message::print()
{
  std::cout << _body << "\n";
}

k_message::find_node_message_controller k_message::get_find_node_message_controller()
{
  return k_message::find_node_message_controller(this);
}


k_message::find_node_message_controller::find_node_message_controller( k_message *from ) :
  _body( from->_body)
{
  return;  
}

void k_message::find_node_message_controller::set_ignore_endpoint( std::vector<ip::udp::endpoint> eps )
{
  _body["ignore_eps"] = json::array();
  for( auto itr : eps ){
	_body["ignore_eps"].push_back( endpoint_to_str(itr) );
  }
}

void k_message::find_node_message_controller::set_finded_endpoint( std::vector<ip::udp::endpoint> eps )
{
  _body["finded_eps"] = json::array();
  for( auto itr : eps ){
	_body["finded_eps"].push_back( endpoint_to_str(itr) );
  }
}

observer_id k_message::get_observer_id()
{
  return this->get_param<observer_id>("observer_id");
}

void k_message::set_observer_id( const observer_id &id )
{
  this->set_param("observer_id", observer_id_to_str(id) );
}


}; // namesapce kademlia
}; // namesapce ss
