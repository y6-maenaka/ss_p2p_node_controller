#ifndef A3401654_2A6C_4D83_ACCD_0FAAE564EA70
#define A3401654_2A6C_4D83_ACCD_0FAAE564EA70


#include <ss_p2p/socket_manager.hpp>


namespace ss
{
namespace ice
{


class ice_agent
{
public:
  ice_agent( udp_socket_manager &sock_manager );
  void hello();

  void income_msg( std::shared_ptr<message> msg ); // メッセージ受信

};


}; // namespace ice
}; // namespace ss


#endif 


