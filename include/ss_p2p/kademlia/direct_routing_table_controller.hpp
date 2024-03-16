#ifndef F2019B32_E459_439D_BBAC_70BE5D390E55
#define F2019B32_E459_439D_BBAC_70BE5D390E55


#include <memory>

#include <ss_p2p/kademlia/k_routing_table.hpp>

#include "boost/asio.hpp"


using namespace boost::asio;


namespace ss
{
namespace kademlia
{


class direct_routing_table_controller
{
private:
  k_routing_table &_routing_table;

public:
  direct_routing_table_controller( k_routing_table &routing_table );

  std::vector<k_node> collect_nodes( k_node& root_node, std::size_t max_count = 2/*要検討*/, const std::vector<k_node> &ignore_nodes = std::vector<k_node>() );
  std::vector<ip::udp::endpoint> collect_nodes( ip::udp::endpoint& root_ep, std::size_t max_count = 2, const std::vector<ip::udp::endpoint> &ignore_eps = std::vector<ip::udp::endpoint>() );
  bool is_exist( ip::udp::endpoint &ep ) const;
  bool is_exist( k_node &kn ) const;
};


};
};


#endif 

