#ifndef FC8A9287_3DDA_400E_83E2_D9FAFC8FF135
#define FC8A9287_3DDA_400E_83E2_D9FAFC8FF135


#include <array>
#include <span>
#include <string>
#include <vector>
#include <iostream>
#include <chrono>
#include <optional>
#include <sys/ioctl.h>

#include "openssl/evp.h"
#include "boost/asio.hpp"

#include <utils.hpp>
#include <ss_p2p/ss_logger.hpp>
#include "./k_bucket.hpp"
#include "./k_node.hpp"
#include "./node_id.hpp"


using namespace boost::asio;


namespace ss
{
namespace kademlia
{


class k_routing_table;
class k_bucket_iterator;
constexpr unsigned short K_BUCKET_COUNT = 160;


class k_routing_table
{
  friend k_bucket;
  friend k_bucket_iterator;
  using routing_table = std::array< k_bucket, K_BUCKET_COUNT >;

private:
  routing_table _table;

  node_id _self_id;
  unsigned short calc_branch( k_node &kn );
public:
  k_routing_table( node_id self_id, ss_logger *logger );
  void swap_node( k_node &node_src, k_node node_dest );

  using update_state = k_bucket::update_state;
  update_state auto_update( k_node kn );
  bool is_exist( k_node &kn );

  std::vector<k_node> collect_node( k_node& root_node, std::size_t max_count, const std::vector<k_node> &ignore_nodes = std::vector<k_node>() );
  // root_nodeから指定個(max_count)ノードを取得する(bucketを跨ぐこともある)

  std::vector<k_node> get_node_front( k_node &root_node, std::size_t count = 1, const std::vector<k_node> &ignore_nodes = std::vector<k_node>() ); 
  // root_nodeに指定したノードが属するbucketの前方からノードを指定個数分取得する
  std::vector<k_node> get_node_back( k_node &root_node, std::size_t count = 1, const std::vector<k_node> &ignore_nodes = std::vector<k_node>() );
  // root_nodeに指定したノードが属するbucketの後方からノードを指定個数分取得する

  k_bucket& get_bucket( k_node &kn );
  k_bucket& get_bucket( unsigned short branch );
  unsigned short calc_branch_index( k_node &kn );
  std::size_t get_node_count();
  ss_logger *_logger;

  k_bucket_iterator get_begin_bucket_iterator();

  static constexpr int bucket_first_index = 0;
  static constexpr int bucket_end_index = 159;

  void print( int start_branch = 1 );
};

class k_bucket_iterator  // 若干bucketへの操作を制御する( 生のiteratorはk_routing_tableから取得する )
{
  friend k_routing_table;
public:
  k_bucket_iterator& operator++(); // 前置
  k_bucket_iterator operator++(int/*dummy*/); // 後置
  k_bucket_iterator& operator--();
  k_bucket_iterator operator--(int);
  bool operator==( const k_bucket_iterator &itr );
  k_bucket& operator*();
  k_bucket& get_raw();
  unsigned short get_branch();
  bool is_invalid() const;
  k_bucket_iterator& to_begin();
  std::vector<k_node> get_nodes();

  void _print_() const;
private: 
  k_bucket_iterator();
  k_bucket_iterator( k_routing_table *routing_table, k_routing_table::routing_table::iterator bucket_itr, unsigned short branch );
  k_routing_table *_routing_table;
  k_routing_table::routing_table::iterator _bucket_itr;
  // k_bucket *_bucket;
  int _branch;

  static k_bucket_iterator (invalid)();
}; 

std::vector<k_node> eps_to_k_nodes( std::vector<ip::udp::endpoint> eps );
k_node generate_random_k_node();


};
};


#endif 


