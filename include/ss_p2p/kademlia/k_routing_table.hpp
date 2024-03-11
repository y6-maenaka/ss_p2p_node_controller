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
  ip::udp::endpoint &_self_ep;
  using routing_table = std::array< k_bucket, K_BUCKET_COUNT >;
  routing_table _table;

  std::pair< std::shared_ptr<unsigned char>, std::size_t > endpoint_to_binary( ip::udp::endpoint &ep ) noexcept;
  node_id clac_node_id( ip::udp::endpoint &ep );

public:
  unsigned int calc_branch( ip::udp::endpoint &ep );
  k_routing_table( ip::udp::endpoint &ep );
  enum add_state
  {
	success,
	overflow	
  };
};


};
};


#endif 


