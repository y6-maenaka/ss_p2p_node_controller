#include <ss_p2p/message.hpp>


namespace ss
{


message::message( json from )
{
  this->_body = from;
}

bool message::is_contain_param( std::string param )
{
  return _body.contains(param);
}

std::span<char> message::encode( json from )
{
  std::vector<std::uint8_t> u8_bson;
  u8_bson = nlohmann::json::to_bson(from);

  std::span<char> ret( reinterpret_cast<char*>(u8_bson.data()), u8_bson.size() );
  return ret;
}

message message::decode( std::span<char> from )
{
  json ret_json = nlohmann::json::from_bson( from );
  return message(ret_json);
}


}; // namespace ss
