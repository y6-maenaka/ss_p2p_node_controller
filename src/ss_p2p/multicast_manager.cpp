#include <ss_p2p/multicast_manager.hpp>


namespace ss
{


multicast_manager::multicast_manager( kademlia::k_routing_table &routing_table, const multicast_manager::endpoint_to_peer_func &ep_to_peer_func ) :
  _routing_table( routing_table )
  , _ep_to_peer_func( ep_to_peer_func )
{
  return ;
}

multicast_manager::peers multicast_manager::get_multicast_target( std::size_t n, cast_type type )
{
  using namespace kademlia;
  // 今のところ, typeは幅優先のみ
  
  peers ret;
  peer::ref ret_peer = this->get_random();
  ret.push_back( ret_peer );

  if( n > 1 ) return ret;

  std::vector< k_node > ignore_k_nodes = _context.convert_peers_to_k_nodes( _context._picked_peers );

  k_node _ = k_node( ret_peer->get_endpoint() );
  auto k_nodes = _routing_table.collect_node( _, n-1, ignore_k_nodes );
  
  std::transform( k_nodes.begin(), k_nodes.end(), ret.begin(), [this](const k_node& n){
	return _ep_to_peer_func( n.get_endpoint() );
	  });

  return ret;
}

peer::ref multicast_manager::get_random()
{
  using namespace kademlia;

  std::vector< std::size_t > bucket_indexes;
  boost::range::push_back( bucket_indexes, boost::irange(k_routing_table::bucket_first_index, (k_routing_table::bucket_end_index) + 1) );

  std::random_device rd;
  std::mt19937 gen(rd());
  std::shuffle( bucket_indexes.begin(), bucket_indexes.end(), gen );

  for( auto itr : bucket_indexes )
  {
	if( auto& target_bucket = _routing_table.get_bucket( itr ); target_bucket.get_node_count() > 0 )
	{
	  std::vector< k_node > ignore_k_nodes = _context.convert_peers_to_k_nodes(_context._picked_peers);
	  /* std::transform( _context._picked_peers.begin(), _context._picked_peers.end(), ignore_k_nodes.begin(), []( const peer::ref p ){
		  return k_node( p->get_endpoint() );
		  }); */

	  if( auto k_nodes = target_bucket.get_node_front(1, ignore_k_nodes ); !(k_nodes.empty()) )
	  {
		auto ret = _ep_to_peer_func(k_nodes.begin()->get_endpoint());
		this->update_context( ret ); 
		return ret;
	  } 
	}
  }

  return nullptr;
}

void multicast_manager::update_context( peer::ref picked_peer )
{
  _context._picked_peers.push_back( picked_peer );
}

void multicast_manager::update_context( peers picked_peers )
{
  for( auto itr : picked_peers ) _context._picked_peers.push_back( itr );
}

void multicast_manager::clear_context()
{
  _context._picked_peers.clear();
}


std::vector< kademlia::k_node > multicast_manager::context::convert_peers_to_k_nodes( peers from ) const
{
  std::vector< kademlia::k_node > ret;
  std::transform( from.begin(), from.end(), ret.begin(), [](const peer::ref &p){
	  return p->get_endpoint();
	  });

  return ret;
};



};
