#ifndef FE95B122_6933_464A_8DF3_16040813D3D9
#define FE95B122_6933_464A_8DF3_16040813D3D9


#include <string>

#include <json.hpp>
#include <utils.hpp>
#include <ss_p2p/observer.hpp>

#include "boost/asio.hpp"
#include "boost/uuid/uuid.hpp"


using json = nlohmann::json;
using namespace boost::asio;
using namespace boost::uuids;


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
	signaling
	, stun
	, none
  };
  protocol_t protocol;

  static ice_message (_stun_)();
  static ice_message (_signaling_)();

  protocol_t get_protocol() const;
  struct signaling_message_controller
  {
	enum sub_protocol_t
	{
		request
	  , response
	  , relay
	};
	sub_protocol_t sub_protocol;

	signaling_message_controller( json &body );

	void add_relay_endpoint( ip::udp::endpoint ep ); // 中継したノード(endpoint)を追加
	std::vector< ip::udp::endpoint > get_relay_endpoints();
	ip::udp::endpoint get_dest_endpoint() const;
	void set_sub_protocol( signaling_message_controller::sub_protocol_t t ); // request, response, relay
	sub_protocol_t get_sub_protocol();
	int get_ttl() const;
	void decrement_ttl();
	private:		
	  json &_body;
  };
  signaling_message_controller get_sgnl_msg_controller();

  template <typename T>
  T get_param( std::string key )
  {
	
	auto value = _body[key];
	if constexpr (std::is_same_v<T, observer_id>)
	{
	  return str_to_observer_id( value.get<std::string>() );
	}
	
	return T{};
  }

  const json encode();
};


};
};


#endif 


