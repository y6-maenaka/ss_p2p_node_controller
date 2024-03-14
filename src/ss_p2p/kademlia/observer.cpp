#include <ss_p2p/kademlia/observer.hpp>


namespace ss
{
namespace kademlia
{


base_observer::base_observer( k_routing_table &routing_table, io_context &io_ctx, deadline_timer &d_timer ) : 
  _obs_id( random_generator()() ) ,
  _routing_table(routing_table) ,
  _io_ctx( io_ctx ) ,
  _d_timer( d_timer )

{
  return;
}

base_observer::observer_id base_observer::get_id() const
{
  return _obs_id;
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


ping_observer::ping_observer( k_routing_table &routing_table, io_context &io_ctx, deadline_timer &d_timer, k_node host_node, k_node swap_node ) : 
  base_observer( routing_table, io_ctx, d_timer ) ,
  _host_node( host_node ) ,
  _swap_node( swap_node )
{
  _is_pong_arrived = false;
}

void ping_observer::init()
{
  #if SS_VERBOSE
  std::cout << "(init observer) ping" << "\n";
  #endif

  _d_timer.expires_from_now( boost::posix_time::seconds(DEFAULT_PING_RESPONSE_TIMEOUT_s) );
  _d_timer.async_wait( std::bind( &ping_observer::timeout, this, std::placeholders::_1 ) );
  // 指定時間経過後にpongが到着していなければswapする
}

void ping_observer::timeout( const boost::system::error_code &ec )
{
 if( _is_pong_arrived ) return; // pongが到着していれば特に何もしない
  
  this->destruct_self() ; // 実質破棄を許可する
  _routing_table.swap_node( _host_node, _swap_node ); // タイムアウトしていればノードを交換する
}

void ping_observer::handle_response( std::shared_ptr<message> msg )
{
  _is_pong_arrived = true;
}


find_node_observer::find_node_observer( k_routing_table &routing_table, io_context &io_ctx, deadline_timer &d_timer, int _ ) :
  base_observer( routing_table, io_ctx, d_timer )
{
  return; 
}

void find_node_observer::init()
{
  #if SS_VERBOSE
  std::cout << "(init observer) find_node" << "\n";
  #endif
}

void find_node_observer::handle_response( std::shared_ptr<message> msg )
{
  return;
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
base_observer::observer_id observer<T>::get_id() const 
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


}; // namespace kademlia
}; // namespace ss
