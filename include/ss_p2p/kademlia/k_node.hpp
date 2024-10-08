#ifndef F11E57F6_B4E1_4C58_BCD4_EF7EB00504DB
#define F11E57F6_B4E1_4C58_BCD4_EF7EB00504DB


#include <array>
#include <string>
#include <memory>

#include "./node_id.hpp"
#include <utils.hpp>

#include "boost/asio.hpp"


using namespace boost::asio;


namespace ss
{
namespace kademlia
{


class k_node : public std::enable_shared_from_this<class k_node>
{
public:
  using ref = std::shared_ptr<k_node>;

  k_node();
  k_node( const k_node &kn );
  k_node( ip::udp::endpoint ep );

  bool operator==( const k_node &kn ) const;
  bool operator!=( const k_node &kn ) const;

  ip::udp::endpoint get_endpoint() const;
  node_id& get_id();

  std::string to_str();
  ref to_ref();
  void print() const;

  static k_node (blank)();

private: 
  ip::udp::endpoint _ep;
  node_id _id;
};

k_node str_to_k_node( std::string from );


};
};


#endif 


