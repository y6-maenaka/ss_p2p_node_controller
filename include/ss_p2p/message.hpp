#ifndef A2B382DF_97D4_4089_9882_F64F2CFC3E58
#define A2B382DF_97D4_4089_9882_F64F2CFC3E58


#include <span>
#include <json.hpp>


using json = nlohmann::json;


namespace ss
{


using encoded_message = std::span<char const>;

  
class message
{
public:
  std::span<char const> encode();

  message( std::span<const char> from_raw ); // decode
  message();

  // std::span<char const> encode();
  // std::span<char const> decode();
private:
  json _body;

};


};


#endif 
