#include <ss_p2p/message_pool.hpp>
#include <ss_p2p/ss_logger.hpp>
#include <ss_p2p/message.hpp>
#include <iostream>
#include <memory>
#include <array>
#include <thread>
#include <chrono>
#include "boost/asio.hpp"


using namespace boost::asio;


int setup_message_pool()
{
  std::cout << "setup_message_pool start" << "\n";

  std::shared_ptr<boost::asio::io_context> io_ctx = std::make_shared<boost::asio::io_context>();
  ss::ss_logger logger;
  ss::message_pool msg_pool( *io_ctx, &logger );


  std::array<char, 8> app_id = {'a','b','c','d','e','f','g','h'};


  auto msg_1 = std::make_shared<ss::message>(app_id);
  auto msg_2 = std::make_shared<ss::message>(app_id);
  auto msg_3 = std::make_shared<ss::message>(app_id);
  auto msg_4 = std::make_shared<ss::message>(app_id);
  auto msg_5 = std::make_shared<ss::message>(app_id);
  
  ip::udp::endpoint ep_1( ip::address::from_string("127.0.0.1"), 1000 );
  ip::udp::endpoint ep_2( ip::address::from_string("127.0.0.1"), 1000 );
  ip::udp::endpoint ep_3( ip::address::from_string("127.0.0.1"), 1000 );
  ip::udp::endpoint ep_4( ip::address::from_string("127.0.0.1"), 1000 );
  ip::udp::endpoint ep_5( ip::address::from_string("127.0.0.2"), 1000 );


  msg_pool.store( msg_1, ep_1 );
  std::this_thread::sleep_for( std::chrono::seconds(1) );
  msg_pool.store( msg_2, ep_2 );
  std::this_thread::sleep_for( std::chrono::seconds(1) );
  msg_pool.store( msg_3, ep_3 );
  std::this_thread::sleep_for( std::chrono::seconds(1) );
  msg_pool.store( msg_4, ep_4 );
  std::this_thread::sleep_for( std::chrono::seconds(1) );
  msg_pool.store( msg_5, ep_5 );





  msg_pool.print_by_peer_id();



  msg_pool.refresh_tick( boost::system::error_code{} );


  msg_pool.print_by_peer_id();


  std::cout << "setup_message_pool end" << "\n";
  return 0;
}
