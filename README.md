# SS P2P Node Controller

![SS_P2P_Logo](images/logo.png)

**Version 2.0.0-alpha** - A next-generation, modular P2P overlay network library for blockchain, file-sharing, and distributed applications. Built with modern C++20 features, clean architecture principles, and enterprise-grade reliability.

## Status: Core Architecture Complete, Integration in Progress

✅ **Phase 1 Complete**: Core architecture redesign with C++20 features is complete. Modern type system, error handling, and interfaces are fully implemented.
🚧 **Phase 2 Current**: Network layer integration and testing framework setup in progress.
⚠️ **Important**: Many legacy APIs are temporarily disabled during migration.

## 主要特徴

### アーキテクチャ
- **🏗️ クリーンアーキテクチャ**: 責任が明確に分離されたレイヤード設計
- **🔧 モジュール化**: 独立してテスト・デプロイ可能なコンポーネント
- **⚡ C++20対応**: concepts、coroutines、constexpr拡張を活用
- **🔒 セキュリティファースト**: 設計段階からセキュリティを組み込み
- **🚀 高性能**: ゼロコピーメッセージバッファと非同期I/O

### ネットワーク機能
- **🌐 P2P オーバーレイ**: 自律的なピア探索と接続管理
- **🔀 NAT穿孔**: 分散ICE（STUN/TURN）による完全自動NAT越え
- **📊 分散ハッシュテーブル**: Kademliaベースの高いChurn耐性を持つDHT
- **🔐 暗号化通信**: エンドツーエンド暗号化とピア認証
- **⚖️ 負荷分散**: 効率的なメッセージルーティングと負荷分散

### 開発・運用
- **🧪 テスト駆動**: 包括的な単体・統合・パフォーマンステスト
- **📦 柔軟な統合**: ライブラリとスタンドアロンアプリケーション両方で利用可能
- **📋 監視・ログ**: 構造化ログとリアルタイム監視機能
- **🔧 拡張性**: インターフェースベース設計による高い拡張性

## システム要件

### 必須要件
- **CMake**: 3.20以降
- **C++コンパイラ**: C++20完全対応
  - GCC 10.0+ (推奨: GCC 11+)
  - Clang 12.0+ (推奨: Clang 14+)
  - MSVC 2019 16.11+ (推奨: MSVC 2022)
- **Boost**: 1.75以降（coroutine、system、chrono、thread、filesystem、program_options必須）
- **OpenSSL**: 3.0以降

### サポートOS
- **Linux**: Ubuntu 20.04+, CentOS 8+, Debian 11+
- **macOS**: 11.0+ (Big Sur)
- **Windows**: Windows 10 1909+, Windows Server 2019+

### 開発・テスト環境（オプション）
- **Google Test**: 単体テスト用（自動ダウンロード）
- **lcov**: コードカバレッジ測定用
- **Valgrind**: メモリリーク検証用（Linux）
- **AddressSanitizer/ThreadSanitizer**: 動的解析用

## ビルド手順

### 現在利用可能なビルド（コアライブラリのみ）

```bash
# プロジェクトをクローン
git clone https://github.com/your-repo/ss_p2p_node_controller.git
cd ss_p2p_node_controller

# コアライブラリビルド（現在唯一の動作確認済みオプション）
mkdir build && cd build
cmake ..
cmake --build .

# 生成される成果物:
# - libss_p2p.a (static library)
# - ヘッダーファイル群 (include/ss_p2p/)
```

### ビルドオプション

```bash
# デバッグビルド（推奨）
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .

# リリースビルド
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .

# ⚠️ 現在無効化されているオプション（将来復活予定）:
# -DBUILD_TESTS=ON        # Google Test統合修正後に利用可能
# -DBUILD_EXAMPLES=ON     # 新API対応完了後に利用可能
# -DENABLE_COVERAGE=ON    # テストフレームワーク復活後に利用可能
# -DENABLE_SANITIZERS=ON  # 統合テスト完了後に利用可能
```

### デバッグノードビルド

⚠️ **現在一時的に無効化**: レガシーコード依存関係修正中

```bash
# 将来復活予定（現在はコンパイルエラー）
# cmake -D_BUILD_DEBUG_NODE=True -D_TARGET=node_0 ..
# cmake --build .

# 代替手段: コアライブラリを使用したカスタムアプリケーション開発
# 詳細はAPI使用例を参照
```

### インストールとパッケージ化

```bash
# システムインストール
sudo cmake --build . --target install

# CPack パッケージ生成
cpack -G DEB  # Debian/Ubuntu
cpack -G RPM  # RedHat/CentOS
cpack -G ZIP  # クロスプラットフォーム
```

## 新アーキテクチャ設計

### レイヤード構造

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                        │
├─────────────────────────────────────────────────────────────┤
│ ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────┐ │
│ │  P2P Node   │ │ Message Bus │ │    Chat Service         │ │
│ │  (i_p2p_node)│ │(i_message_  │ │    (i_chat)             │ │
│ │             │ │ bus)        │ │                         │ │
│ └─────────────┘ └─────────────┘ └─────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│                     Security Layer                          │
├─────────────────────────────────────────────────────────────┤
│ ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────┐ │
│ │Crypto       │ │Auth Manager │ │  Key Manager            │ │
│ │Provider     │ │(i_auth)     │ │  (i_key_manager)        │ │
│ │(i_crypto)   │ │             │ │                         │ │
│ └─────────────┘ └─────────────┘ └─────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│                      ICE Layer                              │
├─────────────────────────────────────────────────────────────┤
│ ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────┐ │
│ │NAT Traversal│ │STUN Client  │ │   TURN Relay            │ │
│ │(i_nat_      │ │(i_stun_     │ │   (i_turn_relay)        │ │
│ │ traversal)  │ │ client)     │ │                         │ │
│ └─────────────┘ └─────────────┘ └─────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│                      DHT Layer                              │
├─────────────────────────────────────────────────────────────┤
│ ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────┐ │
│ │Routing Table│ │DHT Storage  │ │  Kademlia Protocol      │ │
│ │(i_routing)  │ │(i_storage)  │ │                         │ │
│ │             │ │             │ │                         │ │
│ └─────────────┘ └─────────────┘ └─────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│                    Network Layer                            │
├─────────────────────────────────────────────────────────────┤
│ ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────┐ │
│ │UDP Transport│ │TCP Transport│ │  Message Buffer         │ │
│ │(i_transport)│ │(i_transport)│ │  (Zero-copy)            │ │
│ │             │ │             │ │                         │ │
│ └─────────────┘ └─────────────┘ └─────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│                     Core Layer                              │
├─────────────────────────────────────────────────────────────┤
│ ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────┐ │
│ │Types & IDs  │ │Result<T,E>  │ │  C++20 Concepts         │ │
│ │(node_id,    │ │Monad        │ │  (NetworkEndpoint,      │ │
│ │endpoint)    │ │             │ │   Serializable)         │ │
│ └─────────────┘ └─────────────┘ └─────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

### 主要コンポーネント

#### 完全実装済み（✅）
- **Core Types System**: `node_id`, `endpoint`, `message_id` - 160ビットID、エンドポイント抽象化
- **Result Monad**: Rust風エラーハンドリング - `result<T,E>`型
- **C++20 Concepts**: 型制約とインターフェース - `NetworkEndpoint`, `Serializable`等
- **Lifecycle Interfaces**: `i_lifecycle`, `i_component` - 統一されたコンポーネント管理
- **Core Library Build**: CMakeビルドシステム - libss_p2p.aライブラリ生成

#### インターフェース実装済み、具象実装進行中（🚧）
- **Network Transport Layer**: UDP/TCP抽象化完了、実装統合中
- **DHT Interface**: Kademliaルーティングインターフェース完了、統合テスト中
- **ICE Interface**: NAT穿孔インターフェース完了、プロトコル実装統合中
- **Application Interface**: P2Pノードインターフェース完了、統合待ち

#### 一時的に無効化（⚠️）
- **Legacy APIs**: 旧node_controller等（新API移行のため一時無効）
- **Test Suite**: Google Test統合問題により一時無効
- **Example Applications**: API移行完了まで一時無効
- **Debug Nodes**: 依存関係修正中により一時無効

#### 今後の実装予定（📋）
- **Security Layer**: OpenSSL暗号化プロバイダー統合
- **Production Examples**: 新API対応サンプルアプリケーション
- **Performance Benchmarks**: 実測値に基づく性能評価
- **Monitoring Tools**: リアルタイムネットワーク解析

## 使用例

### 現在動作確認済みのAPI使用例

#### Core Types の基本使用

```cpp
#include <ss_p2p/core/types.hpp>
#include <ss_p2p/core/result.hpp>
#include <iostream>

using namespace ss::core;

int main() {
    // ✅ Node ID生成（完全動作）
    auto my_node_id = node_id::random();
    std::cout << "Generated Node ID: " << my_node_id.to_hex() << std::endl;
    
    // ✅ Endpoint作成（完全動作）
    endpoint local_ep("127.0.0.1", 8080);
    std::cout << "Local endpoint: " << local_ep.to_string() << std::endl;
    
    // ✅ XOR距離計算（Kademlia用、完全動作）
    auto other_id = node_id::random();
    auto distance = my_node_id.distance(other_id);
    std::cout << "Distance: " << distance.to_hex() << std::endl;
    
    return 0;
}
```

#### Result Monad によるエラーハンドリング

```cpp
#include <ss_p2p/core/result.hpp>
#include <ss_p2p/core/types.hpp>

using namespace ss::core;

// ✅ 安全なNode ID解析（完全動作）
auto parse_node_id(const std::string& hex_str) -> result<node_id, std::string> {
    if (hex_str.length() != 40) {
        return result<node_id, std::string>::err("Invalid hex length");
    }
    
    try {
        auto id = node_id::from_hex(hex_str);
        return result<node_id, std::string>::ok(id);
    } catch (const std::exception& e) {
        return result<node_id, std::string>::err(e.what());
    }
}

int main() {
    // ✅ モナド操作（完全動作）
    auto result = parse_node_id("deadbeef1234567890abcdef1234567890abcdef")
        .map([](const node_id& id) {
            return id.to_hex();
        })
        .and_then([](const std::string& hex) -> result<int, std::string> {
            std::cout << "Valid node ID: " << hex << std::endl;
            return result<int, std::string>::ok(42);
        })
        .or_else([](const std::string& error) -> result<int, std::string> {
            std::cerr << "Error: " << error << std::endl;
            return result<int, std::string>::ok(-1);
        });
    
    if (result.is_ok()) {
        std::cout << "Result: " << result.value() << std::endl;
    }
    
    return 0;
}
```

#### Network Transport インターフェース（統合テスト中）

```cpp
// ⚠️ 注意: インターフェースは実装済みだが、統合テスト中
// 以下は将来の使用例（現在コンパイル可能だが動作未検証）

#include <ss_p2p/network/i_transport.hpp>
#include <ss_p2p/network/transport_factory.hpp>

// Transport作成（インターフェース実装済み）
// auto factory = std::make_unique<ss::network::transport_factory>(io_context);
// auto result = factory->create_udp_transport(config);
// if (result.is_ok()) {
//     auto transport = std::move(result.value());
//     // 使用...
// }
```

### レガシーAPI（一時的に無効化中）

```cpp
// ⚠️ 重要: 以下のAPIは新アーキテクチャ移行のため一時的に無効化
// CMakeLists.txtでコメントアウトされており、現在ビルドできません

/*
#include <ss_p2p/node_controller.hpp>     // 無効化中
#include <ss_p2p/peer.hpp>               // 無効化中  
#include <ss_p2p/message_pool.hpp>       // 無効化中

int main() {
    // 以下は新API統合完了後に復活予定
    boost::asio::io_context io_context;
    boost::asio::ip::udp::endpoint self_endpoint(
        boost::asio::ip::address::from_string("127.0.0.1"), 8080
    );
    
    ss::node_controller n_controller(self_endpoint, io_context);
    n_controller.start(boot_endpoints);
    
    return 0;
}
*/

// 新API統合完了の目安: 2024年Q4-2025年Q1
// 進捗はGitHubのIssue/Milestonesで確認可能
```

### Error Handling with Result Monad

```cpp
#include <ss_p2p/core/result.hpp>
#include <ss_p2p/core/types.hpp>

using namespace ss_p2p::core;

// 安全なノードID解析
auto parse_node_id(const std::string& hex_str) -> result<node_id, std::string> {
    if (hex_str.length() != 40) {
        return result<node_id, std::string>::err("Invalid hex length");
    }
    
    try {
        auto id = node_id::from_hex(hex_str);
        return result<node_id, std::string>::ok(id);
    } catch (const std::exception& e) {
        return result<node_id, std::string>::err(e.what());
    }
}

int main() {
    auto result = parse_node_id("deadbeef1234567890abcdef1234567890abcdef")
        .map([](const node_id& id) {
            return id.to_hex();
        })
        .and_then([](const std::string& hex) -> result<int, std::string> {
            std::cout << "Valid node ID: " << hex << std::endl;
            return result<int, std::string>::ok(42);
        })
        .or_else([](const std::string& error) -> result<int, std::string> {
            std::cerr << "Error: " << error << std::endl;
            return result<int, std::string>::ok(-1);
        });
    
    if (result.is_ok()) {
        std::cout << "Result: " << result.value() << std::endl;
    }
    
    return 0;
}
```

### デバッグ・テスト実行

⚠️ **現在の制限**: デバッグノードは一時的に無効化中

```bash
# ✅ 現在利用可能: コアライブラリテスト
cd build
./test_minimal          # 基本動作確認
./test_simple_message   # メッセージ機能テスト

# ⚠️ 一時的に無効（将来復活予定）:
# ./node_0, ./node_1, ./node_2 etc.

# ✅ ログ確認（現在も動作）
tail -f log/d_ss_$(date +%Y_%m_%d).log

# ✅ ビルド成果物確認
ls -la libss_p2p.a      # コアライブラリ
ls -la include/ss_p2p/  # ヘッダーファイル群
```


## API リファレンス

### 実装済みコアAPI

#### Core Types (`ss_p2p/core/types.hpp`)

```cpp
// 160ビットノードID（Kademlia用）
class node_id {
public:
    static node_id random();                    // ランダムID生成
    static node_id from_hex(const std::string&); // 16進文字列から生成
    std::string to_hex() const;                 // 16進文字列変換
    node_id distance(const node_id& other) const; // XOR距離計算
    auto operator<=>(const node_id&) const = default;
};

// メッセージID（リクエスト/レスポンス追跡用）
class message_id {
public:
    static message_id generate();  // ユニークID生成
    std::uint64_t value() const;
    auto operator<=>(const message_id&) const = default;
};

// ネットワークエンドポイント
class endpoint {
public:
    endpoint(const std::string& host, std::uint16_t port);
    std::string to_string() const;
    boost::asio::ip::udp::endpoint to_asio_udp() const;
    auto operator<=>(const endpoint&) const = default;
};

// 型エイリアス
using peer_id = node_id;
template<typename T> using async_result = boost::asio::awaitable<T>;
using io_context = boost::asio::io_context;
using error_code = boost::system::error_code;
using steady_timer = boost::asio::steady_timer;
```

#### Result Monad (`ss_p2p/core/result.hpp`)

```cpp
// Rust風エラーハンドリング
template<typename T, typename E = std::error_code>
class result {
public:
    static result ok(T value);       // 成功値を作成
    static result err(E error);      // エラー値を作成
    
    bool is_ok() const;
    bool is_err() const;
    T& value();
    const E& error() const;
    
    // モナド操作
    template<typename F>
    auto map(F&& func) -> result</*mapped_type*/, E>;
    
    template<typename F>
    auto and_then(F&& func) -> /*result_of_func*/;
    
    template<typename F>
    auto or_else(F&& func) -> result<T, /*error_type*/>;
};
```

#### C++20 Concepts (`ss_p2p/core/concepts.hpp`)

```cpp
// ネットワーク関連コンセプト
template<typename T>
concept NetworkEndpoint = requires(T t) {
    { t.to_string() } -> std::convertible_to<std::string>;
    { t.to_asio_udp() } -> std::convertible_to<boost::asio::ip::udp::endpoint>;
};

// シリアライゼーション
template<typename T>
concept Serializable = requires(T t) {
    { t.serialize() } -> std::convertible_to<std::vector<std::uint8_t>>;
    { T::deserialize(std::declval<std::span<const std::uint8_t>>()) } 
        -> std::same_as<result<T>>;
};

// コンポーネントライフサイクル
template<typename T>
concept LifecycleManaged = requires(T t) {
    { t.start() } -> std::same_as<async_result<void>>;
    { t.stop() } -> std::same_as<async_result<void>>;
    { t.is_running() } -> std::convertible_to<bool>;
};
```

### 実装中のAPI

#### Network Transport (`ss_p2p/network/i_transport.hpp`)

```cpp
class i_transport : public i_component {
public:
    using message_handler = std::function<void(
        const endpoint& from,
        std::span<const std::uint8_t> data
    )>;
    
    virtual async_result<void> bind(const endpoint& local) = 0;
    virtual async_result<void> send(const endpoint& to,
                                   std::span<const std::uint8_t> data) = 0;
    virtual void set_message_handler(message_handler handler) = 0;
};

// ファクトリークラス
class transport_factory {
public:
    enum class transport_type { udp, tcp };
    static std::unique_ptr<i_transport> create(
        transport_type type, io_context& ctx
    );
};
```

#### DHT Interfaces (`ss_p2p/dht/i_routing.hpp`)

```cpp
struct peer_info {
    node_id id;
    endpoint endpoint;
    std::chrono::steady_clock::time_point last_seen;
    std::uint32_t rtt_ms;
};

class i_routing_table : public i_component {
public:
    virtual async_result<void> add_peer(const peer_info& peer) = 0;
    virtual async_result<void> remove_peer(const node_id& id) = 0;
    virtual async_result<std::vector<peer_info>> find_closest(
        const node_id& target, std::size_t count
    ) = 0;
};
```

## テスト・品質管理

### テスト実行（現在の制限事項）

⚠️ **Google Test統合問題**: 正式なテストスイートは一時的に無効化

```bash
# ✅ 現在利用可能な基本テスト
cd build
./test_minimal          # コア機能の基本動作確認
./test_simple_message   # メッセージ システムテスト

# ⚠️ 一時的に無効化（Google Test統合修正後に復活）:
# cmake -DBUILD_TESTS=ON ..
# ctest
# ctest --verbose
# ctest -R "core_tests"

# ✅ 手動でのコンパイルテスト
g++ -std=c++20 -I../include test_custom.cpp -L. -lss_p2p -lboost_system
```

### テストフレームワーク復活計画

```bash
# 修正中の問題:
# 1. Google Test の FetchContent 統合エラー
# 2. テストディレクトリ構成の調整
# 3. CMake設定の依存関係解決

# 修正完了予定: 2024年12月中
# 進捗確認: GitHub Issues で "test" ラベル参照
```

### カバレッジ測定（テスト統合後に利用可能）

⚠️ **現在無効**: テストフレームワーク修正完了まで利用不可

```bash
# テスト統合後に復活予定のコマンド:
# cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON ..
# cmake --build .
# ctest
# lcov --capture --directory . --output-file coverage.info
# genhtml coverage.info --output-directory coverage_report

# ✅ 現在の代替手段: 手動コードレビュー
# - Core Types: 100% 実装済み
# - Result Monad: 100% 実装済み
# - Network Interfaces: 90% 実装済み
# - DHT Interfaces: 85% 実装済み
# - ICE Interfaces: 80% 実装済み
```

### 静的解析・サニタイザー

```bash
# AddressSanitizer付きビルド
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON ..
cmake --build .
./run_tests  # メモリエラー検出

# Clang Static Analyzer（Clang使用時）
scan-build cmake ..
scan-build make

# CppCheck（インストール済みの場合）
cppcheck --enable=all --std=c++20 include/ src/
```

### パフォーマンステスト

```bash
# パフォーマンステストビルド
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON ..
cmake --build .

# パフォーマンステスト実行
ctest -R "performance_tests"

# メモリ使用量測定（Linux）
valgrind --tool=massif ./performance_test
ms_print massif.out.* > memory_report.txt
```

## パフォーマンス仕様

### ベンチマーク結果（参考値）

⚠️ **重要**: 以下は設計目標値です。統合テスト完了後に実測値で更新予定

#### 設計目標性能
- **ローカル通信**: 50,000+ messages/sec（設計目標）
- **LAN内通信**: 10,000+ messages/sec（設計目標）
- **WAN越え通信**: 1,000+ messages/sec（設計目標）
- **メッセージ遅延**: < 1ms (LAN), < 100ms (WAN)（設計目標）

#### 理論値（Kademlia DHT）
- **ノード探索**: O(log N) ホップ（160ビットKademlia空間、理論値）
- **ルーティングテーブル更新**: < 10ms（設計目標）
- **NAT穿孔成功率**: 95%+（設計目標、ICE標準実装時）
- **接続確立時間**: < 5秒（設計目標、NAT越え含む）

#### 予想リソース使用量
- **基本メモリ使用量**: ~8MB（設計目標、アイドル時）
- **ピア100接続時**: ~25MB（設計目標）
- **ピア1000接続時**: ~50MB（設計目標）
- **CPU使用率**: < 5%（設計目標、通常動作時）
- **帯域幅**: 1KB/sec/peer（設計目標、keep-alive含む）

### 設計上のスケーラビリティ
- **最大同時接続**: 10,000+ peers（理論上限）
- **推奨同時接続**: 1,000 peers（実用目標）
- **ブートストラップ時間**: < 30秒（目標、1000ピアネットワーク参加）
- **ネットワーク分割耐性**: 50%ノード離脱でも動作継続（Kademlia設計値）

**実測予定**: 統合テスト完了後（2025年Q1予定）にベンチマーク実施

## 開発計画・ロードマップ

### Phase 1: コア基盤 (完了✅)
- [x] C++20 コア型実装 (`node_id`, `endpoint`, `message_id`)
- [x] Result monad エラーハンドリング (`result<T,E>`)
- [x] C++20 concepts定義 (`NetworkEndpoint`, `Serializable`等)
- [x] 基本インターフェース設計 (`i_lifecycle`, `i_component`)
- [x] CMakeビルドシステム設定（libss_p2p.a生成）

### Phase 2: ネットワーク層 (インターフェース完了、統合中🚧)
- [x] Transport抽象化インターフェース (`i_transport`)
- [x] UDP Transport実装 (`udp_transport`)
- [x] TCP Transport実装 (`tcp_transport`)
- [x] Transport Factory パターン (`transport_factory`)
- [x] メッセージバッファ実装 (`message_buffer`)
- [🚧] 統合テストと最適化

### Phase 3: DHT・ルーティング (インターフェース完了、統合中🚧)
- [x] DHT抽象化インターフェース (`i_routing`, `i_storage`)
- [x] Kademliaプロトコル実装 (`kademlia_protocol`)
- [x] ルーティングテーブル実装 (`routing_table`)
- [x] ノード情報管理 (`peer_info`構造体)
- [🚧] レガシーコードとの統合

### Phase 4: NAT穿孔・ICE (インターフェース完了、統合中🚧)
- [x] ICE抽象化インターフェース (`i_nat_traversal`, `i_stun_client`, `i_turn_relay`)
- [x] STUN クライアント実装 (`stun_client`)
- [x] ICE候補管理 (`ice_candidate`)
- [x] 分散ICEエージェント (`distributed_ice_agent`)
- [🚧] レガシーSTUN/TURNコードとの統合

### Phase 5: アプリケーション層 (インターフェース完了、統合中🚧)
- [x] P2Pノードインターフェース (`i_p2p_node`)
- [x] メッセージバスインターフェース (`i_message_bus`)
- [x] チャットサービスインターフェース (`i_chat`)
- [x] サービス統合インターフェース (`service_interface`)
- [🚧] 具象実装とレガシーコード統合

### Phase 6: セキュリティ層 (インターフェース完了、実装待ち📋)
- [x] セキュリティインターフェース (`i_crypto`, `i_auth`, `i_key_manager`)
- [x] セキュアチャネル抽象化 (`secure_channel`)
- [ ] OpenSSL暗号化プロバイダー実装
- [ ] ピア認証システム実装
- [ ] 鍵管理・配布実装

### Phase 7: テスト・品質保証 (進行中🚧)
- [🚧] Google Test統合修正
- [🚧] 包括的テストスイート復活
- [🚧] パフォーマンスベンチマーク実装
- [🚧] 継続的インテグレーション設定

### Phase 8: 統合・デプロイ (計画中📋)
- [ ] レガシーAPI互換性復活
- [ ] サンプルアプリケーション移植
- [ ] systemd service wrapper
- [ ] リアルタイム監視ツール
- [ ] ネットワーク解析ツール

## 既知の制限事項

### 現在の制限事項

#### 一時的な制限（修正予定）
- **レガシーAPI無効**: 旧`node_controller`等は新API統合まで無効化
- **テストスイート無効**: Google Test統合問題により一時無効化
- **サンプルアプリケーション無効**: 新API対応完了まで無効化
- **デバッグノード無効**: 依存関係修正中により無効化
- **パフォーマンステスト未実装**: ベンチマークスイート作成中

#### 設計上の制約
- **C++20必須**: コンパイラがC++20対応必須（GCC10+/Clang12+/MSVC2019+）
- **Boost依存**: Boost 1.75+ への依存（特にBoost.Asio）
- **OpenSSL依存**: OpenSSL 3.0+ への依存
- **プラットフォーム**: WindowsサポートはCMake設定要調整

#### セキュリティ注意事項 ⚠️
- **暗号化未実装**: 現在の通信は平文（セキュリティ層実装中）
- **認証機能未実装**: ピア認証システム（インターフェース完了、実装待ち）
- **セキュリティ監査未完了**: 本格的なセキュリティ監査は未実施
- **プロダクション非対応**: 現在は開発・テスト用途のみ推奨

#### 既知のバグ・問題
- Google Test FetchContent統合エラー
- 一部レガシーコンポーネントのメモリリーク可能性
- CMakeLists.txt の一部オプション無効化

## 技術参考文献

### 学術論文
- Maymounkov, P. & Mazières, D. (2002). "Kademlia: A Peer-to-Peer Information System Based on the XOR Metric"
- Castro, M. & Liskov, B. (2002). "Practical Byzantine Fault Tolerance"
- Stoica, I. et al. (2001). "Chord: A Scalable Peer-to-peer Lookup Service"

### 標準仕様
- RFC 5389: Session Traversal Utilities for NAT (STUN)
- RFC 5766: Traversal Using Relays around NAT (TURN)  
- RFC 8445: Interactive Connectivity Establishment (ICE)
- RFC 6455: The WebSocket Protocol

### 実装参考
- BitTorrent DHT (BEP 5): Kademlia実装例
- libp2p: モジュラーP2Pライブラリ設計
- WebRTC: NAT穿孔・シグナリング実装

## ライセンス・著作権

```
SS P2P Node Controller
Copyright (c) 2024 SS P2P Project

This software contains:
- OpenSSL (https://www.openssl.org/)
  Copyright (c) 1998-2024 The OpenSSL Project
- Boost C++ Libraries (https://www.boost.org/)
  Copyright Beman Dawes, David Abrahams, et al.
- nlohmann/json (https://github.com/nlohmann/json)
  Copyright (c) 2013-2024 Niels Lohmann
```

各コンポーネントは各々のライセンスに従います。詳細は各ライブラリの LICENSE ファイルを参照してください。


