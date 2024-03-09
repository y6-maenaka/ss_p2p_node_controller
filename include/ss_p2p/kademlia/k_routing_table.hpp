#ifndef FC8A9287_3DDA_400E_83E2_D9FAFC8FF135
#define FC8A9287_3DDA_400E_83E2_D9FAFC8FF135

#include "openssl/evp.h"
#include "boost/asio.hpp"


namespace ss
{
namespace kademlia
{




class k_routing_table
{
public:
  int num = 10;
  void hello();

  enum add_state
  {
	overflow	
  };

  add_state add_node();

};


};
};


#endif 


