#ifndef A2B382DF_97D4_4089_9882_F64F2CFC3E58
#define A2B382DF_97D4_4089_9882_F64F2CFC3E58


#include <span>
#include <algorithm>
#include <vector>

#include <json.hpp>


using json = nlohmann::json;


namespace ss
{


using encoded_message = std::span<char const>;

  
class message
{
public:
  message( json from );

  [[ nodiscard ]] bool is_contain_param( std::string param );

  static std::span<char> encode( json from ); 
  static message decode( std::span<char> from ); // 生メッセージのデコード
  void print();
private:
  json _body;

};


};


#endif 
