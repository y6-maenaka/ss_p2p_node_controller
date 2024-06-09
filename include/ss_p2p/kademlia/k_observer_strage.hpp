#ifndef CD87A151_55B7_427F_969A_75CA759E9C4A
#define CD87A151_55B7_427F_969A_75CA759E9C4A


#include <ss_p2p/observer.hpp>
#include <ss_p2p/observer_strage.hpp>
#include <ss_p2p/kademlia/k_observer.hpp>


namespace ss
{
namespace kademlia
{


constexpr unsigned short DEFAULT_OBSERVER_STRAGE_TICK_TIME_s = 8/*[seconds]*/;
constexpr unsigned short DEFAULT_OBSERVER_STRAGE_SHOW_STATE_TIME_s = 2/*[seconds]*/;
using k_observer_strage = observer_strage<ping, find_node>;


};
};


#endif
