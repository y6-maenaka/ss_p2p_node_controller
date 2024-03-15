#ifndef BD51C051_D379_4330_8C9D_0B9E15DFEFC9
#define BD51C051_D379_4330_8C9D_0B9E15DFEFC9


#include <functional>
#include <memory>


namespace ss
{


class signaling_server
{
public:
  using s_send_func = std::function<void(ip::udp::endpoint &ep, std::string, const json payload )> _send_func;
  s_send_func get_signaling_send_func();
};


};


#endif 


