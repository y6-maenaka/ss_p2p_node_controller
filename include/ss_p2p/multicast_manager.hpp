#ifndef F3E546BD_6C47_421E_AA93_1BDB088BA9F0
#define F3E546BD_6C47_421E_AA93_1BDB088BA9F0


#include <vector>
#include <memory>
#include <random>

#include <ss_p2p/kademlia/k_routing_table.hpp>
#include <ss_p2p/kademlia/k_node.hpp>
#include <ss_p2p/peer.hpp>

#include "boost/range/irange.hpp"
#include "boost/range/algorithm_ext.hpp"


namespace ss
{


class multicast_manager
{
  // 基本的にcontextを初期化しなi限り一度pickされたpeerは選出されなi

public:
  using peers = std::vector<peer::ref>;
  using endpoint_to_peer_func = std::function<peer::ref(const ip::udp::endpoint)>;

private:
  kademlia::k_routing_table &_routing_table;
  const endpoint_to_peer_func &_ep_to_peer_func; // k_nodeをpeerに変換する関数

  struct context
  {
	peers _picked_peers; //(≒ignore_peers) // 本multicast_managerからpickされたpeers
	
	std::vector< kademlia::k_node > convert_peers_to_k_nodes( peers from ) const;
  } _context;

public:
  enum cast_type
  {
	breath_first // 幅優先: 複数のバケットにまたがってmulticast先peerを取得する
	, depth_first // 深さ優先: 単一のバケットを集中的にmulticast先peerを取得する(今の所未対応)
  };

  multicast_manager( kademlia::k_routing_table &routing_table, const endpoint_to_peer_func &ep_to_peer_func );

  peers get_multicast_target( std::size_t n, cast_type type = cast_type::breath_first ); // 今のところ幅優先typeのみ
  peer::ref get_random(); // 内部に持つcontextから,マルチキャスト先の次のpeerを選ぶ(基本的にすでにpickされたpeerは選択されない) 
						  // ※ 内部処理的に結構重いので極力使用は避ける
 
  void update_context( peer::ref picked_peer );
  void update_context( peers picked_peers );
  void clear_context();
};


};


#endif 


