#ifndef CF21C64E_9786_4874_8330_AB7D2077BDA2
#define CF21C64E_9786_4874_8330_AB7D2077BDA2


#include <unordered_map>
#include <unordered_set>
#include <optional>

#include "./observer.hpp"


namespace ss
{


class observer_strage
{
protected:
  template < typename T >
  using observer_strage_entry = std::unordered_set< observer<T>, typename observer<T>::Hash >;

  template < typename ... Ts >
  using union_observer_strage = std::tuple<observer_strage_entry<Ts>...>;
  union_observer_strage<> _strage;

public:
  template < typename T >
  std::optional< observer<T> >& find_observer( observer_id id );

  template < typename T >
  void add_observer( observer<T> obs );
  
  observer_strage();
};


};


#endif


