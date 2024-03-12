#include <ss_p2p/kademlia/k_routing_table.hpp>


namespace ss
{
namespace kademlia
{


k_routing_table::k_routing_table( node_id self_id ) :
  _self_id( self_id )
{
  #if SS_VERBOSE 
  std::cout << "k routing table hosting with :: "; _self_id.print(); std::cout << "\n";
  #endif
  return;
}

unsigned short k_routing_table::calc_branch( ip::udp::endpoint &ep ) // オーバヘッドが多少大きい
{
  node_id self_node_id = _self_id;
  node_id peer_node_id = calc_node_id( ep );
  unsigned short node_xor_distance = calc_node_xor_distance( self_node_id, peer_node_id );
  /* 
  #if SS_VERBOSE
  std::cout << "calculated relative branch :: ";
  std::cout << ep;
  std::cout << " \x1b[32m(" << (K_NODE_ID_LENGTH*8) - node_xor_distance << ")\x1b[39m" << "\n";
  #endif
  */ 
  return (K_NODE_ID_LENGTH*8) - node_xor_distance;
}

k_bucket::update_state k_routing_table::auto_update( ip::udp::endpoint &ep )
{
  k_node target_node( ep );
  unsigned short branch_idx = calc_branch_index( ep );
  k_bucket& target_bucket = _table[branch_idx];

  auto ret = target_bucket.auto_update( target_node );

  #if SS_VERBOSE
  std::cout << "\x1b[31m" << "[ routing table update ]" << "\x1b[39m ";
  std::cout << "(endpoint): " << ep;
  std::cout << "| (branch): " << branch_idx + 1;
  std::cout << "| (state): ";
  if( ret == k_bucket::update_state::added_back ) std::cout << "\x1b[32m" << "add back" << "\x1b[39m" << "\n";
  else if( ret == k_bucket::update_state::moved_back ) std::cout << "\x1b[34m" << "move back" << "\x1b[39m" << "\n";
  else if( ret == k_bucket::update_state::not_found ) std::cout << "not found" << "\n";
  else if ( ret == k_bucket::update_state::overflow ) std::cout << "\x1b[31m" << "overflow" << "\x1b[39m" << "\n";
  else std::cout << "error" << "\n";
  #endif

  return ret;
}

unsigned short k_routing_table::calc_branch_index( ip::udp::endpoint &ep )
{
  const auto branch = this->calc_branch(ep);
  /*
  #if SS_VERBOSE
  std::cout << "branch *index* :: ";
  std::cout << " \x1b[32m(" << std::max(branch-1, 0) << ")\x1b[39m" << "\n";
  #endif 
  */
  return std::max( branch -1 , 0 );
}

k_bucket& k_routing_table::get_bucket( ip::udp::endpoint &ep )
{
  const auto branch_idx = calc_branch_index(ep);
  return _table[branch_idx];
}

k_bucket& k_routing_table::get_bucket( unsigned short branch )
{
  return _table[ std::max(branch-1,0) ];
}


};
};
