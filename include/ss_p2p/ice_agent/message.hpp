#ifndef FE95B122_6933_464A_8DF3_16040813D3D9
#define FE95B122_6933_464A_8DF3_16040813D3D9


#include <string>

#include <json.hpp>


using json = nlohmann::json;


namespace ss
{
namespace ice
{


class message
{
private:
  json _body;

public:
  std::string get_param( std::string key );
};


};
};


#endif 


