#include <ss_p2p/kademlia/message_format.hpp>


#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <iostream>

#include <json.hpp>


int dummy_message()
{
  ss::kademlia::message_format k_msg("response");
  k_msg.set_param("rpc", "ping");
  k_msg.print();

  return 0;
}

