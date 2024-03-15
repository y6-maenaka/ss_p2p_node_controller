#ifndef F2019B32_E459_439D_BBAC_70BE5D390E55
#define F2019B32_E459_439D_BBAC_70BE5D390E55


#include <memory>

#include <ss_p2p/kademlia/k_routing_table.hpp>


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

  std::vector< std::shared_ptr<k_node> > collect_nodes( k_node& root_node, std::size_t max_count, const std::vector<k_node*> &ignore_nodes );
  bool is_exist( k_node &kn ) const;
};


};
};


#endif 


