1. Object instantiation and initialization of the node controller <br>
(共通) 1. ノードコントローラーのオブジェクト化と開始
```cpp
ss::node_controlelr n_controller( boost::asio::ip::udp::endpoint self_endpoint, std::shared_ptr<boost::asio::io_context> io_context );
n_controller.start( std::vector<boost::asio::ip::udp::endpoint> boot_eps ); // 既知のノードをブートノードとして幾つか(>0)与える
```

<br><br>

2. Waiting for messages from a specified peer <br>
(単一ピア) 指定ピアからのメッセージ受信待機
```cpp
auto peer = n_controller.get_peer( boost::asio::ip::udp::endpoint peer_udp_endpoint );
auto recved_msg = peer.receive( std::time_t timeout_s ); // 指定無しでメッセージが到着するまでブロッキング
```

<br>

2. Waiting for incoming messages from multiple peers <br>
(多数ピア) ホストに流入してくるメッセージ受信待機
```cpp
auto &message_hub = n_controller.get_message_hub();
message_hub.start( std::function<void(ss::message_pool::_message_)> receive_handler );
```


<br><br><br>

本製品には、OpenSSL Toolkit で使用するために OpenSSL Project で開発されたソフトウェアが含まれています (https://www.openssl.org/)
Copyright (c) 1998-2011 The OpenSSL Project. All rights reserved.
