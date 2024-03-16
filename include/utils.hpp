#ifndef EA2276B8_ADB8_45BA_A32B_10BB082E3CE9
#define EA2276B8_ADB8_45BA_A32B_10BB082E3CE9


#include <string>
#include <mutex>
#include <condition_variable>

#include "boost/asio.hpp"


using namespace boost::asio;


namespace ss
{


std::pair< std::shared_ptr<unsigned char>, std::size_t > endpoint_to_binary( ip::udp::endpoint &ep ) noexcept;
std::string endpoint_to_str( ip::udp::endpoint &ep ); // encode endpoint
ip::udp::endpoint str_to_endpoint( std::string &ep_str ); // decode endpoint
std::pair<std::string, std::uint16_t> extract_endpoint( ip::udp::endpoint &ep );
ip::udp::endpoint addr_pair_to_endpoint( std::string ip, std::uint16_t port );


}; // namespace ss


#endif 


