#ifndef CD82DC15_71A2_4534_A194_33DF2E3A0011
#define CD82DC15_71A2_4534_A194_33DF2E3A0011

#include <vector>
#include <functional>

#include "./message.hpp"

namespace ss
{


class message_pool
{
private:
  using pool = std::vector< std::shared_ptr<const message> >;
  pool _pool;

public:
  void retrive(); // 取得
  void store(); // 追加

  using index = pool::iterator;
  using message_retrive_func = std::function< message(index idx) >;
  message_retrive_func message_retriver();

};




};


#endif 


