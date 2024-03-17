#ifndef F348B366_1EA4_449B_943F_E1BB62918819
#define F348B366_1EA4_449B_943F_E1BB62918819


#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <tuple>
#include <functional>

#include <ss_p2p/observer.hpp>
#include <ss_p2p/observer_strage.hpp>
#include "./ice_observer.hpp"

#include "boost/asio.hpp"


using namespace boost::asio;


namespace ss
{
namespace ice
{


class ice_observer_strage : public ss::observer_strage
{
private:
  union_observer_strage< signaling_request, signaling_response, signaling_relay, stun > _strage;

public:
  ice_observer_strage( io_context &io_ctx );
};


};
};


#endif 




