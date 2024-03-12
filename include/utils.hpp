#ifndef EA2276B8_ADB8_45BA_A32B_10BB082E3CE9
#define EA2276B8_ADB8_45BA_A32B_10BB082E3CE9


#include "boost/asio.hpp"


namespace ss
{


static std::pair< std::shared_ptr<unsigned char>, std::size_t > endpoint_to_binary( ip::udp::endpoint &ep ) noexcept
{
  const auto addr_str = ep.address().to_string();
  const auto port = ep.port();

  std::shared_ptr<unsigned char> addr_binary = std::shared_ptr<unsigned char>( new unsigned char[addr_str.size() + sizeof(port)] );
  std::size_t cpyOffset = 0;
  std::memcpy( addr_binary.get() + cpyOffset, addr_str.data(), addr_str.size() ); cpyOffset += addr_str.size();
  std::memcpy( addr_binary.get() + cpyOffset, &port, sizeof(port) ); cpyOffset += sizeof(port);

  return std::make_pair( addr_binary, cpyOffset );
}


};


#endif 


