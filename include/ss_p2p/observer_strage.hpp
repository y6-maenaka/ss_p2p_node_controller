#ifndef CF21C64E_9786_4874_8330_AB7D2077BDA2
#define CF21C64E_9786_4874_8330_AB7D2077BDA2


#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <iostream>

#include "./observer.hpp"
#include <utils.hpp>

#include "boost/asio.hpp"
#include "boost/multi_index_container.hpp"
#include "boost/multi_index/tag.hpp"
#include "boost/multi_index/identity.hpp"
#include "boost/multi_index/indexed_by.hpp"
#include "boost/multi_index/hashed_index.hpp"
#include "boost/multi_index/ordered_index.hpp"
#include "boost/multi_index/sequenced_index.hpp"
#include "boost/multi_index/member.hpp"
#include "boost/multi_index/mem_fun.hpp"


using namespace boost::asio;


namespace ss
{


constexpr unsigned short DEFAULT_OBSERVER_STRAGE_TICK_TIME_s = 60/*[seconds]*/;
constexpr unsigned short DEFAULT_OBSERVER_STRAGE_SHOW_STATE_TIME_s = 2/*[seconds]*/;


struct by_observer_id{
  public:
	template<typename T> using key_type = observer<T>::id;
};

template <typename T> struct DEFAULT_OBSERVER_STRAGE_POLICY
{
  typedef boost::multi_index::indexed_by 
	<
	  boost::multi_index::hashed_unique< boost::multi_index::tag<by_observer_id>
	  , boost::multi_index::const_mem_fun< observer<T>, typename observer<T>::id/*戻り値の型*/, &observer<T>::get_id/*キー取得用のメソッド*/>
	  , typename observer<T>::Hash
	  , typename observer<T>::Equal 
	  > // observer_id
	> index;

  /*
   インデックスの種類
   (identity) : 要素自体をキーとして使用するインデックス(カスタムハッシュ等を与えることもできる)
   (member) : 構造体のメンバーを直接指定してそれをキーにすることができる
  */
};

template < template<typename> class IndexPolicy, typename... Ts >
class observer_strage
{
protected:
  // template < typename T > using entry = std::unordered_set< typename observer<T>::ref, typename observer<T>::Hash >;

  template<typename T>
	using entry = boost::multi_index_container<
		typename observer<T>::ref // 型がこの段階で特定できない場合にtypenameを使う(?)
		, typename IndexPolicy<T>::index >;

  // std::tuple< entry<Ts>... > _strage;
  std::tuple< entry<Ts>... > _strage;
  io_context &_io_ctx;

  template < typename T > void delete_expires_observer( entry<T> &e );
  template < typename T > void print_entry_state( const entry<T> &e );

  void call_tick();
  void refresh_tick( const boost::system::error_code &ec );
  deadline_timer _refresh_tick_timer;

public:
  observer_strage( io_context &io_ctx );

  // search系メソッド
  template < typename T > using found_observers = std::vector< typename observer<T>::ref >;
  template < typename T, typename Key = struct by_observer_id > found_observers<T> find_observer( const Key::template key_type<T> &id ); // 検索・取得メソッド
  template < typename T, typename Key = struct by_observer_id > auto find_observer_itr_range( const Key::template key_type<T> &id ) 
	-> decltype(std::declval<entry<T>>().template get<Key>().equal_range(id));
  template < typename T, typename Key = struct by_observer_id > found_observers<T> pop_observer( const Key::template key_type<T> &id );
 
  // 追加/削除系メソッド
  template < typename T > const bool add_observer( observer<T> obs ); // 追加メソッド
  template < typename T > const bool add_observer( typename observer<T>::ref obs_ref );
  template < typename T > entry<T>::iterator delete_observer( entry<T> &e, entry<T>::iterator obs_itr );

  #if SS_DEBUG
  deadline_timer _debug_tick_timer;
  void show_state( const boost::system::error_code &ec );
  #endif
};


template < template<typename> class IndexPolicy, typename... Ts >
observer_strage< IndexPolicy, Ts...>::observer_strage( io_context &io_ctx ) :
  _io_ctx(io_ctx)
  , _refresh_tick_timer( io_ctx )
  #if SS_DEBUG
  , _debug_tick_timer( io_ctx )
  #endif
{
  this->call_tick();
}

template< template<typename> class IndexPolicy, typename... Ts >
template< typename T > void observer_strage< IndexPolicy, Ts...>::delete_expires_observer( typename observer_strage::entry<T> &e )
{
  for( auto itr = e.begin(); itr != e.end(); )
  {
	if( (*itr)->is_expired() )
	{
	  #if SS_VERBOSE
	  // std::cout << "\x1b[33m" << " | [" << (*itr)->get_type_name() << "](delete observer) :: " <<"\x1b[39m" << (*itr)->get_id() << "\n";
	  std::cout << (*itr)->get_type_name() << "\n";
	  std::cout << (*itr)->get_id() << "\n";
	  #endif

	  itr = this->delete_observer<T>( e, itr );
	  // itr = e.erase(itr);
	}
	else itr++;
  }
  return;
}

template < template<typename> class IndexPolicy, typename... Ts >
template < typename T > void observer_strage< IndexPolicy, Ts...>::print_entry_state( const typename observer_strage::entry<T> &e ) // 修正が必要
{
  for( int i=0; i<get_console_width()/2; i++ ){ printf("="); } std::cout << "\n";
  // if constexpr (std::is_same_v<T, signaling_request>)
  // std::cout << "| signaling_request" << "\n";
  // else if constexpr (std::is_same_v<T, signaling_relay>) std::cout << "| signaling_relay" << "\n";
  // else if constexpr (std::is_same_v<T, signaling_response>) std::cout << "| signaling_response" << "\n";
  // else if constexpr (std::is_same_v<T, binding_request>) std::cout << "| binding_request" << "\n";
  // else if constexpr (std::is_same_v<T, ping>) std::cout << "| ping" << "\n";
  // else if constexpr (std::is_same_v<T, find_node>) std::cout << "| find_node" << "\n";
  // else std::cout << "| undefine" << "\n";
  std::cout << "| ENTRY" << "\n";

  unsigned int count = 0;
  for( int i=0; i<get_console_width()/2; i++ ){ printf("-"); } std::cout << "\n";
  std::cout << "\x1b[32m";
  for( auto &itr : e )
  {
	std::cout << "| ("<< count << ") ";
	itr->print(); std::cout << "\n";
	count++;
  }
  std::cout << "\x1b[39m" << "\n";
} 

template < template<typename> class IndexPolicy, typename... Ts >
void observer_strage< IndexPolicy, Ts...>::call_tick()
{
  _refresh_tick_timer.expires_from_now(boost::posix_time::seconds( DEFAULT_OBSERVER_STRAGE_TICK_TIME_s ));
  _refresh_tick_timer.async_wait( std::bind( &observer_strage::refresh_tick, this , std::placeholders::_1 ) );
}

template < template<typename> class IndexPolicy, typename... Ts >
void observer_strage< IndexPolicy, Ts...>::refresh_tick( const boost::system::error_code &ec )
{
  std::apply([this](auto &... args)
	  {
		  // ((this->delete_expires_observer<typename std::decay<decltype(*args.begin())>::type::element_type>(args)), ...);
		  ((this->delete_expires_observer<Ts>(args)), ...);
	  }, _strage ); 

  call_tick(); // 循環的に呼び出し
}

template < template<typename> class IndexPolicy, typename... Ts >
template < typename T, typename Key > auto observer_strage< IndexPolicy, Ts...>::find_observer_itr_range( const Key::template key_type<T> &id ) 
  -> decltype(std::declval<entry<T>>().template get<Key>().equal_range(id))
{
  /*
  auto &s_entry = std::get< entry<T> >(_strage);
  for( auto itr = s_entry.begin(); itr != s_entry.end(); itr++ )
	if( (*itr)->get_id() == id && !((*itr)->is_expired()) ) return itr;
  return s_entry.end();
  */
  
  auto &s_entry = std::get< entry<T> >(_strage);
  auto ret_itr_range = s_entry.template get<Key>().equal_range(id);
  return ret_itr_range;
}

template < template <typename> class IndexPolicy, typename... Ts >
template < typename T, typename Key > std::vector< typename observer<T>::ref > observer_strage< IndexPolicy, Ts...>::find_observer( const Key::template key_type<T> &id ) // 検索・取得メソッド
{
  // auto &s_entry = std::get< entry<T> >(_strage);
  // s_entry.template get<by_observer_id>().find( id ); // templateつけて明示的に依存関係を通知する

  found_observers<T> ret;
  auto itr_range = this->find_observer_itr_range<T, Key>(id);
  for( auto itr = itr_range.first; itr != itr_range.second; ++itr ) ret.push_back(*itr);

  return ret;

  /*
  if( auto ret = this->find_observer_itr<T>(id); ret != std::get< entry<T> >(_strage).end() ) return *ret;
  return nullptr;
  */
}

template < template<typename> class IndexPolicy, typename... Ts >
template < typename T, typename Key > std::vector< typename observer<T>::ref > observer_strage< IndexPolicy, Ts...>::pop_observer( const Key::template key_type<T> &id )
{
  found_observers<T> ret;

  auto itr_range = this->find_observer_itr_range<T, Key>(id);
  for( auto itr = itr_range.first; itr != itr_range.second; ++itr ) 
  {
	typename observer<T>::ref itr_ref = *itr;

	// 冗長かも
	auto &s_entry = std::get< entry<T> >(_strage);
	this->delete_observer<T>( s_entry, itr );

	ret.push_back( itr_ref );
  }
  
  /*
  auto &s_entry = std::get< entry<T> >(_strage);
  if( auto ret_itr = find_observer_itr<T>(id); ret_itr != s_entry.end() )
  {
	typename observer<T>::ref ret_ref = *ret_itr; // 削除前に参照を保持
	this->delete_observer<T>( s_entry, ret_itr );
	return ret_ref;
  }
  return nullptr;
  */

  return ret;
}

template < template<typename> class IndexPolicy, typename... Ts >
template < typename T > const bool observer_strage< IndexPolicy, Ts...>::add_observer( observer<T> obs ) // 修正が必要
{
  /* #if SS_VERBOSE
  if constexpr (std::is_same_v<T, signaling_request>) std::cout << "| [ice observer strage](signaling_request observer) store" << "\n";
  else if constexpr (std::is_same_v<T, signaling_relay>) std::cout << "| [ice observer strage](signaling_relay observer) store" << "\n";
  else if constexpr (std::is_same_v<T, signaling_response>) std::cout << "| [ice observer strage](signaling_response observer) store" << "\n";
  else if constexpr (std::is_same_v<T, binding_request>) std::cout << "| [ice observer strage](binding_request observer) store" << "\n";
  else std::cout << "| [ice observer strage](undefined observer) store" << "\n";
  #endif */

  // auto &s_entry = std::get< entry<T> >(_strage);
  
  auto &s_entry = std::get< typename observer_strage<IndexPolicy, Ts...>::template entry<T> >(_strage);
  return (s_entry.insert( obs.get_ref() )).second;
}

template < template<typename> class IndexPolicy, typename... Ts >
template < typename T > const bool observer_strage< IndexPolicy, Ts...>::add_observer( typename observer<T>::ref obs_ref )
{
  auto &s_entry = std::get< typename observer_strage< IndexPolicy, Ts...>::template entry<T> >(_strage);
  return (s_entry.insert( obs_ref )).second;
}

template < template<typename> class IndexPolicy, typename... Ts >
template < typename T > observer_strage< IndexPolicy, Ts...>::entry<T>::iterator observer_strage< IndexPolicy, Ts...>::delete_observer( entry<T> &e, entry<T>::iterator obs_itr )
{
  return e.erase(obs_itr);
}


#if SS_DEBUG
template < template<typename> class IndexPolicy, typename... Ts >
void observer_strage< IndexPolicy, Ts...>::show_state( const boost::system::error_code &ec )
{
  std::apply([this](auto &... args)
	{
		// ((this->print_entry_state<typename std::decay<decltype(*args.begin())>::type::element_type>(args)), ...);
		/*
		 decltype(*args.begin()); 
		  - パラメータパックのbegin()メソッドを呼び出す
		  - decltypeはその参照型を取得する

		  std::decay<...>::type;
		  - std::decayは型修飾を取り除き, 基本的な型を取得する. (例) ポインタや修飾, const属性を取り除く

		  element_type;
		  - スマートポインタやコンテナ型などで使用されるメンバ型
		*/

		((this->print_entry_state<Ts>(args)), ...);
		/* イメージ
		 print_entry_state(args1);
		 print_entry_state(args2);
		 print_entry_state(args3);
		*/
	}, _strage );

  _debug_tick_timer.expires_from_now(boost::posix_time::seconds( DEFAULT_OBSERVER_STRAGE_SHOW_STATE_TIME_s ));
  _debug_tick_timer.async_wait( std::bind( &observer_strage::show_state, this , std::placeholders::_1 ) );
}
#endif 


};


#endif


