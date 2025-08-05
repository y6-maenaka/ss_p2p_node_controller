# SS P2P Node Controller - Docker Setup

本プロジェクトをDockerで実行するためのセットアップガイドです。

## 前提条件

- Docker Engine 20.10以降
- Docker Compose v2.0以降
- 使用可能メモリ 4GB以上（複数ノード実行時）

## クイックスタート

### 1. 開発環境の起動

```bash
# Docker開発スクリプトを使用
./scripts/docker-dev.sh build
./scripts/docker-dev.sh dev

# または直接docker-composeを使用
docker-compose up -d ss-p2p-dev
```

### 2. 開発コンテナに接続

```bash
# スクリプト経由
./scripts/docker-dev.sh shell

# または直接
docker exec -it ss_p2p_dev /bin/bash
```

### 3. プロジェクトのビルドとテスト

コンテナ内で:

```bash
# ビルド済みのプロジェクトを確認
cd /workspace/ss_p2p_node_controller/build
ls -la

# 再ビルド（変更後）
cmake --build . -j$(nproc)

# テスト実行
ctest --verbose

# 個別ノードの実行
./node_0
```

## P2Pネットワーク全体の起動

```bash
# 完全なP2Pネットワーク（stable-host + 4ノード）を起動
./scripts/docker-dev.sh network

# ログ確認
./scripts/docker-dev.sh logs stable-host
./scripts/docker-dev.sh logs node-0

# 特定ノードのシェルに接続
./scripts/docker-dev.sh shell node-0
```

## 使用可能なコマンド

```bash
# ヘルプ表示
./scripts/docker-dev.sh help

# 主要コマンド
./scripts/docker-dev.sh build     # Dockerイメージビルド
./scripts/docker-dev.sh dev       # 開発環境起動
./scripts/docker-dev.sh network   # 完全ネットワーク起動
./scripts/docker-dev.sh test      # テスト実行
./scripts/docker-dev.sh logs      # ログ表示
./scripts/docker-dev.sh shell     # シェル接続
./scripts/docker-dev.sh stop      # 全コンテナ停止
./scripts/docker-dev.sh clean     # クリーンアップ
```

## ポート割り当て

| サービス | ホストポート | コンテナポート | 説明 |
|---------|-------------|--------------|------|
| ss-p2p-dev | 8080 | 8080 | 開発環境 |
| stable-host | 8080 | 8080 | ブートストラップノード |
| node-0 | 8081 | 8080 | P2Pノード0 |
| node-1 | 8082 | 8080 | P2Pノード1 |
| node-2 | 8083 | 8080 | P2Pノード2 |
| node-3 | 8084 | 8080 | P2Pノード3 |

## ディレクトリ構成

```
/workspace/ss_p2p_node_controller/  # コンテナ内作業ディレクトリ
├── build/                          # ビルド成果物
├── log/                           # ログファイル（永続化）
├── src/                           # ソースコード
├── include/                       # ヘッダファイル
└── run/                           # ノード実行ファイル
```

## 開発ワークフロー

### 1. コード変更とテスト

```bash
# 開発コンテナ起動
./scripts/docker-dev.sh dev
./scripts/docker-dev.sh shell

# コンテナ内でコード変更後
cd /workspace/ss_p2p_node_controller/build
cmake --build . -j$(nproc)
ctest
```

### 2. デバッグ

```bash
# 開発コンテナ内でデバッガ使用
gdb ./node_0
(gdb) run
(gdb) bt

# Valgrindでメモリリーク検査
valgrind --leak-check=full ./node_0
```

### 3. ネットワークテスト

```bash
# 複数ノードでの動作確認
./scripts/docker-dev.sh network

# 各ノードのログを並行監視
./scripts/docker-dev.sh logs stable-host &
./scripts/docker-dev.sh logs node-0 &
./scripts/docker-dev.sh logs node-1 &
```

## トラブルシューティング

### ビルドエラー

```bash
# 完全クリーンビルド
./scripts/docker-dev.sh rebuild

# 依存関係の確認
./scripts/docker-dev.sh shell
apt list --installed | grep -E "(boost|ssl|cmake)"
```

### ネットワーク接続問題

```bash
# コンテナ間通信確認
./scripts/docker-dev.sh shell node-0
ping ss_p2p_stable_host
netstat -tulpn
```

### ログ確認

```bash
# 全サービスのログ
./scripts/docker-dev.sh logs

# 特定サービスのリアルタイムログ
./scripts/docker-dev.sh logs node-0
```

## クリーンアップ

```bash
# 全コンテナ・ボリューム削除
./scripts/docker-dev.sh clean

# Dockerシステム全体クリーンアップ
docker system prune -a --volumes
```