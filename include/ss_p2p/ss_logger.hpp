#ifndef D8FB837F_FA5A_462D_B238_4ECAEDF22DE5
#define D8FB837F_FA5A_462D_B238_4ECAEDF22DE5


#include <logger/logger.hpp>

#include "boost/asio.hpp"


#if !defined(SS_LOGGING_DISABLE) \
  || !defined(SS_SYSTEM_LOG_OUTPUT_FILENAME) \
  || !defined(SS_PACKET_LOG_OUTPUT_FILENAME)

#define SS_LOGGER_SYSTEM_OUTFILE_NAME "ss_system"
#define SS_LOGGER_PACKET_OUTFILE_NAME "ss_pakcet"

#endif


using namespace boost::asio;


namespace ss
{


class ss_logger : public logger
{
public:
  enum packet_direction
  {
	INCOMING
	  , OUTGOING
  };

  template <typename... Args > void log_packet( const logger::log_level &ll, const packet_direction &pd, const ip::udp::endpoint &ep, Args&& ... args );
};



}; // namespace ss


#include "./ss_logger.impl.hpp"


#endif
