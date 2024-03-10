#ifndef C93919D3_F914_40AA_9B56_FA0F3445F739
#define C93919D3_F914_40AA_9B56_FA0F3445F739


#include "./observer.hpp"


namespace ss
{
namespace kademlia
{


class rpc_manager
{
public:
  observer_ptr ping();
  observer_ptr find_node();
};


};
};


#endif
