#ifndef E1AEB5FF_5603_4904_95D1_8CFA1AC889F4
#define E1AEB5FF_5603_4904_95D1_8CFA1AC889F4


#include <string>
#include <iostream>
#include <vector>
#include <algorithm>
#include <span>
  
#include <json.hpp>


using json = nlohmann::json;


namespace ss
{
namespace kademlia
{


const std::vector<std::string> kademlia_message_params = {"rpc","k_node"};


class k_message
{
private:
  json _body;

public:
  k_message( std::string query_type );
  k_message( json &k_msg );

  bool set_param( std::string key, std::string value );
  template < typename T > 
  T get_param( std::string key )
  {
	auto value = _body[key];
	if constexpr (std::is_same_v<T, std::string>){
	  return value.get<std::string>();
	}
	else if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>){
	  return value.get<T>;
	}
	return T{};
  }

  static k_message (request)();
  static k_message (response)();

  bool is_request() const;

  [[nodiscard]] bool validate() const; // 必ずvalidateする
  
  json export_json() const; // bodyを取り出すだけ
  void print();
};


};
};


#endif 


