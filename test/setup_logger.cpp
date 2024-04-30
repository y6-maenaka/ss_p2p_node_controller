// #include <static_logger/logger.hpp>
#include <logger/logger.hpp>
#include <ss_p2p/ss_logger.hpp>
#include "boost/asio.hpp"


using namespace boost::asio;


int setup_logger()
{
  ip::udp::endpoint dest_ep;


  logger lg;
  // ss::ss_logger slg;

  lg.set_custom_header( "[SS_P2P]" );
  lg.log( logger::log_level::ERROR, "Apple", "ばなな", "いちご" );
  // slg.log_packet( logger::log_level::ERROR, ss::ss_logger::packet_direction::OUTGOING, dest_ep, "HelloWorld" );

  return 0;
}
