#ifndef FC8A9287_3DDA_400E_83E2_D9FAFC8FF135
#define FC8A9287_3DDA_400E_83E2_D9FAFC8FF135


#include <array>
#include <span>
#include <string>
#include <vector>
#include <iostream>

#include "openssl/evp.h"
#include "boost/asio.hpp"

#include <hash.hpp>
#include "./k_bucket.hpp"
#include "./k_node.hpp"
#include "./node_id.hpp"


using namespace boost::asio;


namespace ss
{
namespace kademlia
{


constexpr unsigned short K_BUCKET_COUNT = 160;


class k_routing_table
{
private:
  using routing_table = std::array< k_bucket, K_BUCKET_COUNT >;
  routing_table _table;

  node_id _self_id;
  unsigned short calc_branch( ip::udp::endpoint &ep );
public:
  k_routing_table( node_id self_id );

  k_bucket::update_state auto_update( ip::udp::endpoint &ep );

  k_bucket& get_bucket( ip::udp::endpoint &ep );
  k_bucket& get_bucket( unsigned short branch );
  unsigned short calc_branch_index( ip::udp::endpoint &ep );
};


};
};


#endif 


