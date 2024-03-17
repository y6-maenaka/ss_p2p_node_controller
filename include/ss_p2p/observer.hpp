#ifndef A3106EAE_F653_43A8_AFA9_3E54D88AD98F
#define A3106EAE_F653_43A8_AFA9_3E54D88AD98F


#include <ss_p2p/message.hpp>

#include "boost/asio.hpp"
#include "boost/uuid/uuid.hpp"
#include "boost/uuid/uuid_io.hpp"
#include "boost/uuid/uuid_generators.hpp"
#include "boost/lexical_cast.hpp"


using namespace boost::asio;
using namespace boost::uuids;


namespace ss
{


namespace ice
{
  class signaling_open;
};


constexpr unsigned short DEFAULT_EXPIRE_TIME_s = 10/*[seconds]*/;


class base_observer
{
public:
  using id = uuid;
  uuid get_id() const;
  bool is_expired() const;

  virtual void init() = 0;

protected:
  base_observer( io_context &io_ctx );

  void destruct_self(); // 本オブザーバーの破棄を許可する
  void extend_expire_at( std::time_t t = DEFAULT_EXPIRE_TIME_s );
  std::time_t _expire_at; // このオブザーバーを破棄する時間
  
  io_context &_io_ctx;
  const id _id;
};


template < typename T >
class observer // 実質wrapperクラス
{
private:
  std::shared_ptr<T> _body;

public:
  using id = base_observer::id;

  observer( std::shared_ptr<T> from );
  observer(  T from );
  template < typename ... Args >
  observer( io_context &io_ctx, Args ... args );

  void init();
  void income_message( message &msg );
  void on_send_success( const boost::system::error_code &ec );
  bool is_expired() const;
  id get_id() const;
  void print() const;

  std::shared_ptr<T> get_raw();

  bool operator==(const observer<T> &obs ) const;
  bool operator!=(const observer<T> &obs ) const;
  struct Hash;
};
template < typename T >
struct observer<T>::Hash
{
  std::size_t operator()( const observer<T> &obs ) const;
};

using observer_id = base_observer::id;
base_observer::id str_to_observer_id( std::string from );

};


#endif 


