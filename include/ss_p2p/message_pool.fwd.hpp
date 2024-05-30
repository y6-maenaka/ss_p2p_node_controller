#include <memory>


// この方法が良いのか不明

namespace ss
{


class peer_message_buffer;
using peer_message_buffer_ref = std::shared_ptr<peer_message_buffer>;

class message_pool;
using message_pool_ref = std::shared_ptr<message_pool>;

class ss_message;
using ss_message_ref = std::shared_ptr<ss_message>;


};
