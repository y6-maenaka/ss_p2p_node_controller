#ifndef C93919D3_F914_40AA_9B56_FA0F3445F739
#define C93919D3_F914_40AA_9B56_FA0F3445F739


#include <functional>
#include <memory>

#include <ss_p2p/message.hpp>
#include <hash.hpp>
#include "./observer.hpp"
#include "./message.hpp"
#include "./node_id.hpp"
#include "./k_routing_table.hpp"

#include "boost/asio.hpp"


using namespace boost::asio;


namespace ss
{
namespace kademlia
{


class rpc_manager
{
public:
  observer_ptr ping();
  observer_ptr find_node();
  observer_ptr join();

  observer_ptr handle_observer( observer_ptr op );
  rpc_manager( node_id self_id );

private:
  // std::function<void(ip::udp::endpoint &ep, std::shared_ptr<message>)> send_func;
  std::shared_ptr<k_routing_table> _routing_table;
  node_id _self_id;
};


};
};


#endif
