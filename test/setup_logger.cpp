#include <ss_p2p/logger/logger.hpp>


int setup_logger()
{
  S_LOG_EMERGENCY.log("hello");
  S_LOG_ALERT.log("hello");
  S_LOG_CRITICAL.log("hello");
  S_LOG_ERROR.log("hello");
  S_LOG_WARNING.log("hello");
  S_LOG_NOTICE.log("hello");
  S_LOG_INFO.log("hello");
  S_LOG_DEBUG.log("hello");
  return 0;
}
