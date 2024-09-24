#ifndef DCA789F2_049E_4A86_91A5_F44944098F52
#define DCA789F2_049E_4A86_91A5_F44944098F52


#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <ss_p2p/ss_logger.hpp>

#include "boost/asio.hpp"


using namespace boost::asio;

namespace ss
{


class interface
{ 
  // テスト用に簡易的な標準入力からの入力を受け付ける機能を実装
  using input_command_callback = std::function<void(std::vector<std::string>)>;
public:
  interface( io_context &io_ctx, const input_command_callback notify_func, ss_logger *logger );
  ~interface();

  void listen_stdin();

private:
  std::thread _th;
  io_context &_io_ctx;
  const input_command_callback _notify_func;
  ss_logger *_logger;
};


};


#endif 


