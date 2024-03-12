#include <ss_p2p/kademlia/node_id.hpp>


namespace ss
{
namespace kademlia
{


node_id::node_id()
{
  return;
}

node_id::node_id( std::vector<unsigned char> id_from )
{
  std::copy( id_from.begin(), id_from.end(), _id.begin() );
  return; 
}

bool node_id::operator ==( const node_id &nid ) const
{
  return _id == nid._id;
}

node_id::id node_id::operator()() const
{
  return _id;
}

unsigned char node_id::operator[](unsigned short idx) const
{
  return static_cast<unsigned char>(_id[idx]);
}

void node_id::print() const
{
  for( int i=0; i<_id.size(); i++ )
	printf("%02X", _id[i] );
}


unsigned short calc_node_xor_distance( const node_id &nid_1, const node_id &nid_2 )
{
  unsigned char xorDistance[ K_NODE_ID_LENGTH ];
  for( int i=0; i<K_NODE_ID_LENGTH; i++ )
	xorDistance[i] = (nid_1[i]) ^ (nid_2[i]);

  unsigned short prefixZeroCount = 0;
  for( int i=0; i<K_NODE_ID_LENGTH * 8; i++ ){
	if( ((xorDistance[i/8] >> (i%8)) & 1) == 0x01 ) break;
	prefixZeroCount++;
  }
  return prefixZeroCount;
}

node_id calc_node_id( ip::udp::endpoint &ep )
{
  auto ep_bin = endpoint_to_binary( ep );
  return calc_node_id( ep_bin.first.get(), ep_bin.second );
}



};
};
