#ifndef FE95B122_6933_464A_8DF3_16040813D3D9
#define FE95B122_6933_464A_8DF3_16040813D3D9


#include <string>

#include <json.hpp>
#include <utils.hpp>

#include "boost/asio.hpp"


using json = nlohmann::json;
using namespace boost::asio;


namespace ss
{
namespace ice
{


class ice_message
{
private:
  json _body;

public:
  ice_message( std::string protocol ); // ホスト作成
  ice_message( json &from );
  
  enum protocol_t
  {
	req_signaling_open 
	, req_stun
	, res_stun
	, none
  };
  protocol_t protocol;

  static ice_message (_stun_request_)();
  static ice_message (_stun_response_)();
  static ice_message (_signaling_open_)();

  protocol_t get_protocol() const;
  struct signaling_message_controller
  {
	signaling_message_controller( json &body );
	void add_relay_endpoint( ip::udp::endpoint ep );
	std::vector< ip::udp::endpoint > get_relay_endpoints();
	private:		
	  json &_body;
  };
  signaling_message_controller get_sgnl_msg_controller();

  template <typename T>
  T get_param( std::string key );

  const json encode();
};


};
};


#endif 


