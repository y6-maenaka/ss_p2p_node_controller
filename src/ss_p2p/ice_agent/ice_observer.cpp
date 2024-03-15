#include <ss_p2p/ice_agent/ice_observer.hpp>


namespace ss
{
namespace ice
{


ice_observer::ice_observer( io_context &io_ctx, deadline_timer &d_timer, direct_routing_table_controller &d_routing_table_controller ) :
  base_observer( io_ctx, d_timer ) ,
  _d_routing_table_controller( d_routing_table_controller )
{
  return;
}


signaling_open::signaling_open( io_context &io_ctx, deadline_timer &d_timer, direct_routing_table_controller &d_routing_table_controller ) :
  ice_observer( io_ctx, d_timer, d_routing_table_controller ) 
{
  return;
}

void signaling_open::init()
{
  return;
}

void signaling_open::handle_response( std::shared_ptr<message> msg )
{
  // メッセージの検証
  std::string type = msg->get_param("type");

  // 1. ターゲットが自身の場合 -> 応答を送信する
   

  // 2. ターゲットが自身でない場合 -> routing_tableから数個コードを取り出してそこに転送する
  // 2.1. 有効期限が切れていれば削除する 

  // _send_func( ep, msg );
}


}; // namespace ice
}; // namespace ss
