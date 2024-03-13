#ifndef AF9B0283_5634_4772_8A61_13FD01658E4E
#define AF9B0283_5634_4772_8A61_13FD01658E4E


#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <algorithm>

#include "./k_node.hpp"


namespace ss
{
namespace kademlia
{


constexpr unsigned short DEFAULT_K = 2;


class k_bucket
{
private:
  std::mutex _mtx;
  std::condition_variable _cv;

  std::vector< k_node > _nodes;

public:
  k_bucket();

  enum update_state
  {
	added_back , 
	moved_back ,
	not_found ,
	overflow ,
	error
  };
  update_state auto_update( k_node kn );

  std::shared_ptr<k_node> get_front() const;
  bool add_back( k_node kn );
  bool move_back( k_node &kn );
  bool delete_node( k_node &kn );
  bool swap_node( k_node &node_src, k_node node_dest );
  bool is_exist( k_node &kn ) const;
  bool is_full() const;
  std::size_t count() const;

  void print() const;
};

  
};
};


#endif


