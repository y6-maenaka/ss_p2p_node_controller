#include <ss_p2p/kademlia/k_routing_table.hpp>


namespace ss
{
namespace kademlia
{


k_routing_table::k_routing_table( ip::udp::endpoint &ep ) :
  _self_ep(ep)
{
  return;
}

std::pair< std::shared_ptr<unsigned char>, std::size_t > k_routing_table::endpoint_to_binary( ip::udp::endpoint &ep ) noexcept
{
  const auto addr_str = ep.address().to_string();
  const auto port = ep.port();

  std::shared_ptr<unsigned char> addr_binary = std::shared_ptr<unsigned char>( new unsigned char[addr_str.size() + sizeof(port)] );
  std::size_t cpyOffset = 0;
  std::memcpy( addr_binary.get() + cpyOffset, addr_str.data(), addr_str.size() ); cpyOffset += addr_str.size();
  std::memcpy( addr_binary.get() + cpyOffset, &port, sizeof(port) ); cpyOffset += sizeof(port);

  return std::make_pair( addr_binary, cpyOffset );
}

unsigned int k_routing_table::calc_branch( ip::udp::endpoint &ep )
{

  auto self_ep_bin = this->endpoint_to_binary( _self_ep );
  auto peer_ep_bin = this->endpoint_to_binary( ep );

  std::vector<unsigned char> self_ep_md = sha1_hash( self_ep_bin.first.get(), self_ep_bin.second );
  std::vector<unsigned char> peer_ep_md = sha1_hash( peer_ep_bin.first.get(), peer_ep_bin.second );

  unsigned char xorDistance[ K_NODE_ID_LENGTH ];
  for( int i=0; i<K_NODE_ID_LENGTH; i++ )
	xorDistance[i] = (self_ep_md.at(i)) ^ (peer_ep_md.at(i));

  unsigned short prefixZeroCount = 0;
  for( int i=0; i<K_NODE_ID_LENGTH * 8; i++ ){
	if( ((xorDistance[i/8] >> (i%8)) & 1) == 0x01 ) break;
	prefixZeroCount++;
  }

  #if SS_VERBOSE
  std::cout << "calculated relative branch :: ";
  std::cout << "\x1b[32m" << (K_NODE_ID_LENGTH*8) - prefixZeroCount << "\x1b[39m" << "\n";
  #endif
  
  return (K_NODE_ID_LENGTH*8) - prefixZeroCount; 
}


};
};
