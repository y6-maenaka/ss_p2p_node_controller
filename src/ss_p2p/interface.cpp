#include <ss_p2p/interface.hpp>


namespace ss
{


interface::interface( io_context &io_ctx, const input_command_callback notify_func, ss_logger *logger ) :
  _io_ctx( io_ctx )
  , _notify_func( notify_func )
  , _logger(logger)
{
  _th = std::thread([this](){ this->listen_stdin(); });

  #ifndef SS_LOGGING_DISABLE
  _logger->log( logger::log_level::INFO, "(@interface)","start" );
  #endif
}

interface::~interface()
{
  _th.join();

  #ifndef SS_LOGGING_DISABLE
  _logger->log( logger::log_level::ALERT, "(@interface)","stop" );
  #endif
}

void interface::listen_stdin()
{
  std::string input;

  _io_ctx.post([this, input](){ _notify_func(input); });
}


};
