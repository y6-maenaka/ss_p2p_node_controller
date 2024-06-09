#ifndef F348B366_1EA4_449B_943F_E1BB62918819
#define F348B366_1EA4_449B_943F_E1BB62918819


#include <ss_p2p/observer_strage.hpp>
#include <ss_p2p/ice_agent/ice_observer.hpp>


namespace ss
{
namespace ice
{


constexpr unsigned short DEFAULT_OBSERVER_STRAGE_TICK_TIME_s = 10/*[seconds]*/;
constexpr unsigned short DEFAULT_OBSERVER_STRAGE_SHOW_STATE_TIME_s = 2/*[seconds]*/;
using ice_observer_strage = observer_strage< signaling_request, signaling_response, signaling_relay, binding_request >;


};
};


#endif
