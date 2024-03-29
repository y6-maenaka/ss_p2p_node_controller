#include <ss_p2p/kademlia/dht_manager.hpp>


namespace ss
{
namespace kademlia
{


dht_manager::connection_maintainer::connection_maintainer( class dht_manager *manager, class rpc_manager &rpc_manager, io_context &io_ctx ) :
  _d_manager( manager )
  // , _io_ctx( io_ctx )
  , _rpc_manager( rpc_manager )
  , _io_ctx( io_ctx )
  , _timer( io_ctx )
  , _requires_tick( false )
{
  return;
}

void dht_manager::connection_maintainer::tick()
{
  if( !(_requires_tick) ) return;

  #if SS_VERBOSE
  std::cout << "\x1b[34m" << "[dht_manager / connection_maintainer]" << "\x1b[39m" << " routing table update tick" << "\n";
  #endif
  
  std::time_t refresh_ping_done_time = this->send_refresh_ping(); // 生存しているノード数(実際の)をカウントする為
 
  std::cout << "ping respose timeout s -> " << refresh_ping_done_time + 1 << "\n";
  _timer.expires_from_now( boost::posix_time::seconds(refresh_ping_done_time + 1) );
  _timer.async_wait( std::bind( &dht_manager::connection_maintainer::get_remote_nodes, this ) );
}

void dht_manager::connection_maintainer::tick_done()
{
  auto &d_table_controller = _d_manager->get_direct_routing_table_controller();
  std::size_t node_count = d_table_controller.get_node_count();
  std::time_t next_tick_time_s = ( node_count < MINIMUM_NODES ) ? 10 : node_count * 20 ; /* 実装段階では雑に決定 */

  if( !(_requires_tick) ) return; // tickがstopされていたらそれ以上は繰り返さない
  d_table_controller.print();

  #if SS_VERBOSE
  std::cout << "next tick standby :: " << next_tick_time_s << "[s]" << "\n";
  #endif

  _timer.expires_from_now( boost::posix_time::seconds( next_tick_time_s/*適当*/) );
  _timer.async_wait( std::bind( &dht_manager::connection_maintainer::get_remote_nodes, this ) );
}

void dht_manager::connection_maintainer::get_remote_nodes()
{
  auto &d_table_controller = _d_manager->get_direct_routing_table_controller();
  std::size_t node_count = d_table_controller.get_node_count();

  std::cout << "-- 2" << "\n";
  if( node_count < MINIMUM_NODES )
  {
	ip::udp::endpoint root_ep( ip::address::from_string("0.0.0.0"), 0 );
	auto request_eps = d_table_controller.collect_endpoint( root_ep, 5/*適当*/ );
	_rpc_manager.find_node_request( request_eps, request_eps
		, std::bind( &direct_routing_table_controller::auto_update, d_table_controller, std::placeholders::_1 ) 
		);
  }
  this->tick_done();
}

std::time_t dht_manager::connection_maintainer::send_refresh_ping()
{
  // 全てのノードを収集する
  std::cout << "-- 4" << "\n";
  auto &d_table_controller = _d_manager->get_direct_routing_table_controller();
  for( auto bucket_itr = d_table_controller.get_begin_bucket_iterator(); !(bucket_itr.is_invalid()); bucket_itr++ )
  {
	auto bucket_nodes = bucket_itr.get_nodes();
	for( auto itr : bucket_nodes ) // 全てのノードにpingを送信する(timeoutでdelete)
	{ 
	  _rpc_manager.ping_request
		( itr.get_endpoint()  // timeout->deleteのpingを送信する
		  , std::bind( &k_bucket::move_back, std::ref(bucket_itr.get_raw()), itr )
		  , std::bind( &k_bucket::delete_node, std::ref(bucket_itr.get_raw()), itr ) 
		);
	}
  }

  return DEFAULT_PING_RESPONSE_TIMEOUT_s;
}


};
};