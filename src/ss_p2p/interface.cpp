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
  std::vector< std::string > inputs;
  std::string input;

  while( true )
  {
	std::getline( std::cin, input );
	std::istringstream iss(input);
	std::string _;
	while( iss >> _ ){
	  inputs.push_back(_);
	}

	_io_ctx.post([this, inputs](){ _notify_func(inputs); });
	inputs.clear(); // inputsのクリア
  }
  return;
}


};
