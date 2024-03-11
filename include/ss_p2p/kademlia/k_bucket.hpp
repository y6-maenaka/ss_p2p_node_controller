#ifndef AF9B0283_5634_4772_8A61_13FD01658E4E
#define AF9B0283_5634_4772_8A61_13FD01658E4E


#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>

#include "./k_node.hpp"


namespace ss
{
namespace kademlia
{


constexpr unsigned short DEFAULT_K = 20;


class k_bucket
{
private:
  std::mutex _mtx;
  std::condition_variable _cv;

  std::vector< std::shared_ptr<k_node> > _nodes;

public:
  k_bucket();

};

  
};
};


#endif


