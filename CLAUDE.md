# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

This is a C++20 CMake project for a P2P overlay network library.

### Build Commands

```bash
# Create build directory and configure
mkdir build && cd build
cmake ..

# Build the library
cmake --build .

# Build specific debug node (replace node_0 with desired node)
cmake -D_BUILD_DEBUG_NODE=True -D_TARGET=node_0 ..
cmake --build .

# Build all debug nodes
cmake -D_BUILD_DEBUG_ALL_NODE=True ..
cmake --build .
```

### Test Execution

Test files are located in `test/` directory. Build and run individual tests:

```bash
# From build directory
# Build specific test (example)
g++ -std=c++20 -I../include -L. -lss_p2p ../test/setup_node_controller.cpp -o test_node_controller
./test_node_controller
```

## Architecture Overview

### Core Components

1. **Node Controller** (`ss_p2p/node_controller.hpp`): Main orchestrator managing P2P operations
2. **Kademlia DHT** (`ss_p2p/kademlia/`): Distributed hash table implementation with XOR distance metric
3. **ICE Agent** (`ss_p2p/ice_agent/`): Handles NAT traversal using STUN/TURN protocols
4. **Message System** (`ss_p2p/message.hpp`, `ss_p2p/message_pool.hpp`): JSON-based message handling

### Key Directories

- `include/ss_p2p/`: Public API headers
- `src/ss_p2p/`: Implementation files
- `include/crypto_utils/`: Cryptographic utilities (AES, RSA, SHA)
- `run/`: Example node implementations
- `test/`: Test suite

### Dependencies

- **Boost.Asio**: Asynchronous I/O and networking
- **OpenSSL**: Cryptographic operations
- **nlohmann/json**: JSON parsing (included as `include/json.hpp`)

### Build Targets

- `ss_p2p`: Main static library
- `node_0` through `node_4`: Debug test nodes
- `stable_host`: Stable bootstrap node

### Logging

Logs are written to `log/` directory. Default location set via `DEFAULT_LOGGER_OUTFILE_DIR` compile definition.

### Usage Pattern

```cpp
// Basic node setup
ss::node_controller n_controller(self_endpoint, io_context);
n_controller.start(boot_endpoints);

// Single peer communication
auto peer = n_controller.get_peer(peer_endpoint);
auto message = peer.receive(timeout_s);

// Multi-peer message handling
auto &message_hub = n_controller.get_message_hub();
message_hub.start(receive_handler);
```



<!-- ==== エージェント起動設定 ==== -->
# Agents
## Trigger: 「エージェントを活用してください」と同等の文章がプロンプトに含まれている場合

- **test-automation-validator**: 「コード変更（ファイルの追加・修正・削除）を検知したら、既存テストを自動実行し、失敗テストには修正提案または自動アップデートを行い、さらにカバレッジ不足箇所には新規テストケースを生成して網羅性を常に最適化してください。」
- **code-static-analyzer**: 「プロジェクト全体に対してLintとコードフォーマッタを適用し、型不整合、依存関係エラー、命名規則違反などを静的解析で検出し、発見した問題に対して具体的な修正案をレポートしてください。」
- **dependency-security-scanner**: 「依存ライブラリのバージョンと既知のCVEをスキャンし、最新安定版へのアップデート候補と脆弱性レポート（緊急度・対応優先度付き）を作成してください。」
- **readme-sync-agent**: 「アーキテクチャやモジュール構成の変更を検知したときに `README.md` を自動更新し、プロジェクト概要、セットアップ手順、利用例を常に最新状態に保ってください。」
- **spec-doc-generator**: 「ソースコードとインラインコメントから機能一覧、クラス・モジュール構成、入出力仕様を抽出して簡易仕様書を生成し、変更があるたびに更新してください。」
- **api-doc-generator**: 「エンドポイント定義やデータモデルを解析し、Swagger/OpenAPI形式のAPIドキュメントをリクエスト／レスポンス例付きで自動生成・更新してください。」
- **cpp-implementation-agent**: 「要件に沿ってRAIIやスマートポインタなど適切なメモリ管理とシステムコールを用いた実務レベルのC++実装（ヘッダ／実装ファイル分割、ユニットテスト付き）を生成してください。」
- **pull-request-reviewer**: 「プルリクエストの差分を解析し、セキュリティリスク、エラーハンドリングの抜け、可読性やパフォーマンスの問題点を指摘し、各指摘に対して具体的な修正案をコメントしてください。」
- **debug-analyzer**: 「テスト失敗や例外発生時のスタックトレースおよびログを解析し、原因箇所を特定して修正手順と再現手順を詳細に提示してください。」
- **performance-profiler-analyzer**: 「実行時プロファイラ（CPU／メモリ）の結果を解析し、ボトルネックとなる関数やメモリ消費の多い箇所を特定し、アルゴリズム改善、キャッシュ利用、並列化など具体的な最適化手法を提案してください。」


## 言語ポリシー

- **コード分析 (Code Analysis)**、**推論 (Reasoning)**、**コード生成 (Code Writing)** はすべて **英語** で実施してください。  
  - 例: “// Analyze the function to ensure edge cases are handled.”

- **ユーザーへのプロンプト** や **フィードバック (User Prompts & Feedback)** はすべて **日本語** で行ってください。  
  - 例: “こちらのコードをご確認ください。ご質問があればお知らせください。”


<!-- ==== プログラム・プロジェクト概要 ==== -->
SS P2P Node Controllerは、自作ブロックチェーンやP2Pファイル共有システム向けに設計された、完全自律型のP2Pネットワーク構築・管理ライブラリです。通信にはUDPを用い、Boost.Asioによる非同期イベント駆動型I/Oで高速・効率的なメッセージ送受信を実現。開発者はソケットライクなシンプルAPIを呼び出すだけで、ノード探索やルーティングテーブル管理、NAT越え処理をすべてライブラリに任せられます。内部にはKademliaベースのDHTルーティングモジュールと、ICE（STUN/TURN）ベースのNAT穿孔モジュールが組み込まれ、Observerシステムが両者を統合管理する構成です。

1. P2Pオーバーレイネットワークの自律構築
ノード起動時には、開発者が指定した少数のブートノードアドレスをもとに、以下の手順で自動参加を行います。まず160ビット長の一意なノードIDを生成し、既知ブートノードへ自身の存在を通知しながらピア情報を収集。Kademliaのfind_nodeを用いてターゲット（例：ID=0）に近いノードを再帰的に探索し、得られたピア情報をルーティングテーブルに登録します。その後も内部の接続維持マネージャが定期的に全テーブルエントリへPingを送信し、応答のあったノードは同バケットの末尾へ移動、タイムアウトしたノードは削除。これにより高いChurnにも耐えられる安定したオーバーレイ網を維持します。

2. KademliaによるDHTルーティングとピア管理
ライブラリのコアはKademliaアルゴリズムで、160ビットID空間上のXOR距離を使って類似度を定義します（distance(x,y)=x⊕y）。各ノードは160個のバケット（k_bucket）を持ち、バケットごとに一定数のピア情報を格納。任意の通信イベント（Ping/Pongだけでなく、STUNバインド応答やシグナリング応答など）を通じて得られたレスポンスをすべてauto_update関数でルーティングテーブルに反映し、生存ノードとして末尾に移動させます。バケット内のノード数が閾値を下回るとfind_nodeを再度走らせてテーブルを拡大探索し、高いスケーラビリティと耐故障性を確保します。

3. 分散型ICEによるNAT越え（STUN/TURN）
NAT越えにはICEプロトコルを完全分散実装し、専用サーバなしでSTUN/TURN機能を全ピアが担います。新規ノードAがNAT配下にいる場合、まずAにB宛のダミーUDPパケット送信を指示し、A側のNATに穴を開ける。この情報をKademlia経由で選出した中継ノードCへsignaling_requestとして転送し、CがBへシグナリング要求を届けます。BはAの外部アドレスを受け取り逆方向にダミーパケットを送信して自身のNATを突破。最後にC経由でsignaling_responseを返し合うことで、双方の外部セッションが確立されます。もし直接通信に失敗した場合のみ、Cや他のノードをTURNリレーに使ってフォールバック接続を行います。

4. Observerシステムによる統合管理
すべての制御メッセージはObserver（オブザーバ）と呼ばれる一時オブジェクトで管理されます。たとえばping_observer／find_node_observer／signaling_request_observerなどが存在し、各ObserverにはUUIDが割り当てられ、送信メッセージに埋め込まれます。受信時はメッセージ種別に応じた処理モジュールが該当Observerを検索し、レスポンス処理を実行。Boost.Asioの非同期タイマで寿命管理し、タイムアウト時にはコールバックを呼んで自動破棄します。さらに、Observer経由で得られるすべての応答をルーティングテーブル更新に活用することで、DHTとICEがシームレスに連携し、すべての通信がピア管理に寄与する設計です。

以上のように、SS P2P Node ControllerはUDP＋Boost.Asioを基盤に、Kademlia DHTによるスケーラブルなノード探索＆維持、全ノード内蔵の分散型ICEによる柔軟なNAT越え、そしてObserverシステムによる制御メッセージの統合管理を組み合わせた、効率性と信頼性を両立した自律分散ネットワーク構築ライブラリです。開発者は従来の煩雑なネットワーク処理を意識せず、まるでソケット通信のようにメッセージ交換やデータ同期を行えます。今後はブロックチェーンのみならず、さまざまなP2Pアプリケーションでの活用が期待されます。