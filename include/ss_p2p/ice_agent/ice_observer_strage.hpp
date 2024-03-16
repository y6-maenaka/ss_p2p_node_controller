#ifndef F348B366_1EA4_449B_943F_E1BB62918819
#define F348B366_1EA4_449B_943F_E1BB62918819


#include <optional>

#include <ss_p2p/observer.hpp>


namespace ss
{
namespace ice
{


class ice_observer_strage
{
private:
  /*
  using ping_observer_strage = std::unordered_set< observer<ping>, observer<ping>::Hash >;
  using find_node_observer_strage = std::unordered_set< observer<find_node>, observer<find_node>::Hash >;

  using observer_strage_idv = std::variant< ping_observer_strage, find_node_observer_strage >;
  using union_observer_strage = std::map< std::string, observer_strage_idv >;
  union_observer_strage _strage;
  */
public:
  template < typename T >
  std::optional< observer<T> >& find_observer( observer_id id );

  template < typename T >
  void add_observer( observer<T> obs );

};


};
};

#endif 




