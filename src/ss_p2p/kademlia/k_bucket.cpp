#include <ss_p2p/kademlia/k_bucket.hpp>


namespace ss
{
namespace kademlia
{


k_bucket::k_bucket()
{
  _nodes.clear();
  return;
}

k_bucket::update_state k_bucket::auto_update( k_node kn )
{
  if( this->is_exist(kn) )
  {
	this->move_back( kn );
	return update_state::moved_back;
  }
  else
  {
	if( this->is_full() )
	{
	  return update_state::overflow;	
	}
	else
	{
	  this->add_back( kn );
	  return update_state::added_back;
	}
  }
  return update_state::error;
}

std::shared_ptr<k_node> k_bucket::get_front() const
{
  if( _nodes.empty() ) return nullptr;
  return std::make_shared<k_node>( _nodes.at(0) );
}

bool k_bucket::add_back( k_node kn )
{
  std::unique_lock<std::mutex> lock(_mtx);
  if( bool is_full = this->is_full(); is_full ) return false;
  if( bool is_exist = this->is_exist(kn); is_exist  ) return false;

  _nodes.push_back( kn );
  return true;
}

bool k_bucket::move_back( k_node &kn )
{
  std::unique_lock<std::mutex> lock(_mtx);
  auto itr = std::find_if( _nodes.begin(), _nodes.end(), [kn]( const k_node & _ ){
		return kn == _;
	  });
  if( itr == _nodes.end() ) return false;

  k_node target_node = *itr; // temp
  auto erase_itr = _nodes.erase(itr);
  _nodes.push_back( target_node );
  return true;
}

bool k_bucket::delete_node( k_node &kn )
{
  auto itr = std::find( _nodes.begin(), _nodes.end(), kn );
  if( itr == _nodes.end() ) return false;

  #if SS_VERBOSE
  std::cout << "(delete node)" << kn.get_endpoint() << "\n";
  #endif
  _nodes.erase(itr);
  return true;
}

bool k_bucket::swap_node( k_node &node_src, k_node node_dest )
{
  auto src_itr = std::find( _nodes.begin(), _nodes.end(), node_src );
  if( src_itr == _nodes.end() ) return false;

  auto dest_itr = std::find( _nodes.begin(), _nodes.end(), node_dest );
  if( dest_itr != _nodes.end() ) return false;

  #if SS_VERBOSE
  std::cout << "(swap node) " << node_src.get_endpoint() << " -> " << node_dest.get_endpoint() << "\n";
  #endif

  auto itr = _nodes.erase( src_itr );
  _nodes.insert( itr, node_dest );

  return true;
}

bool k_bucket::is_exist( k_node &kn ) const
{
  auto ret = std::find_if( _nodes.begin(), _nodes.end(), [kn]( const k_node& _ ){
		return kn == _;
	  });
  return ret != std::end(_nodes);
}

bool k_bucket::is_full() const
{
  return _nodes.size() >= DEFAULT_K;
}

std::size_t k_bucket::count() const
{
  return _nodes.size();
}

void k_bucket::print() const
{
  for( int i=0; i<_nodes.size(); i++ ){
	std::cout << "node (" << i << ") ";
	_nodes.at(i).print(); std::cout << "\n";
  } std::cout << "\n";
}


};
};
