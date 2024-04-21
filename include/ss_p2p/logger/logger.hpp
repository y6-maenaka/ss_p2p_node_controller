#ifndef A3FBC2A6_E266_4588_B378_06DDF759EA56
#define A3FBC2A6_E266_4588_B378_06DDF759EA56


#include <mutext>

namespace ss
{


class s_logger
{

  enum class log_level
  {
	FATAL
	 , ERROR
	 , ALERT
	 , WARNING
	 , INFO
	 , DEBUG
	 , TRACE
	 , INVALID
  };

  static std::mutex _mtx;
  std::lock_guard<std::mutex> _lock_{_mtx};


};


};


// [参考] (simple-cpp-logger) https://github.com/nadrino/simple-cpp-logger


#endif 
