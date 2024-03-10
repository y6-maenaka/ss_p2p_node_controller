#ifndef E1AEB5FF_5603_4904_95D1_8CFA1AC889F4
#define E1AEB5FF_5603_4904_95D1_8CFA1AC889F4


#include <string>
#include <iostream>
#include <vector>
#include <algorithm>

#include <json.hpp>


using json = nlohmann::json;


namespace ss
{
namespace kademlia
{


const std::vector<const std::string> kademlia_message_params = {"rpc","k_node"};


class message_format
{
private:
  json _body;
  void hello();

public:
  message_format( std::string msg_type );

  bool set_param( std::string key, std::string value );

  static message_format (request)();
  static message_format (response)();

  void print();
};


};
};


#endif 


