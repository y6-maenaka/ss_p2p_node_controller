#ifndef CE0F66A1_C3F7_4BC9_9A20_EFCD065B3EDD
#define CE0F66A1_C3F7_4BC9_9A20_EFCD065B3EDD


#include <iostream>
#include <ss_p2p/peer.hpp>
#include <ss_p2p/message.hpp>


class message_server
{
public:
  void on_receive_message( ss::peer::ref peer_ref , ss::ss_message::ref msg_ref )
  {
	std::size_t console_width = ss::get_console_width();

  
	std::cout << "<@ " << peer_ref->get_endpoint() << "> ";
	if( msg_ref->body.contains("messenger") ) std::cout << msg_ref->body["messenger"] << "\n";
	else std::cout << "[message(serialized)]" << "\n";

  }
};


#endif 
