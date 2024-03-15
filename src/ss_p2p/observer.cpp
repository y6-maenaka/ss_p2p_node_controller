#include <ss_p2p/observer.hpp>


namespace ss
{


base_observer::base_observer( io_context &io_ctx, deadline_timer &d_timer ) : 
  _id( random_generator()() ) ,
  _io_ctx( io_ctx ) ,
  _d_timer( d_timer )
{
  return;
}

base_observer::id base_observer::get_id() const
{
  return _id;
}

void base_observer::destruct_self()
{
  _expire_at = 0;
}

void base_observer::extend_expire_at( std::time_t t )
{
  _expire_at = std::time(nullptr) + t;
}

bool base_observer::is_expired() const
{
  return _expire_at <= std::time(nullptr);
}


template < typename T >
void observer<T>::init()
{
  return _body->init();
}

template < typename T >
bool observer<T>::is_expired() const
{
  return _body->is_expired();
}

template < typename T >
observer<T>::id observer<T>::get_id() const 
{
  return _body->get_id();
}

template < typename T >
void observer<T>::print() const
{
  return _body->print();
}

template < typename T >
bool observer<T>::operator==( const observer<T> &obs ) const
{
  return _body->get_id() == obs.get_id();
}

template < typename T >
bool observer<T>::operator!=( const observer<T> &obs ) const 
{
  return _body->get_id() != obs.get_id();
}


template < typename T >
std::size_t observer<T>::Hash::operator()( const observer<T> &obs ) const
{
  return std::hash<std::string>()( boost::uuids::to_string(obs.get_id()) );
}


base_observer::id str_to_observer_id( std::string id_str )
{
  boost::uuids::string_generator gen;
  return gen( id_str );
}


};
