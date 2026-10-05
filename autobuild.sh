#!/usr/bin/env bash

# 遇到错误、未定义变量或管道失败时立即退出
set -euo pipefail

# 项目根目录
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"

# 构建目录
BUILD_DIR="$ROOT_DIR/build"

# 如果仓库被移动过位置，旧的 CMakeCache.txt 会指向历史路径，需要先清理。
if [[ -f "$BUILD_DIR/CMakeCache.txt" ]] && ! grep -Fq "CMAKE_HOME_DIRECTORY:INTERNAL=$ROOT_DIR" "$BUILD_DIR/CMakeCache.txt"; then
  echo "检测到 build 目录缓存来自其他路径，正在清理旧缓存..."
  rm -rf "$BUILD_DIR"
fi

echo "[1/3] 正在生成构建文件..."
cmake -S "$ROOT_DIR" -B "$BUILD_DIR"

echo "[2/3] 正在编译 test_pool..."
cmake --build "$BUILD_DIR" --parallel

echo "[3/3] 正在运行自测..."
ctest --test-dir "$BUILD_DIR" --output-on-failure

echo "构建完成，可执行文件位置：$BUILD_DIR/bin/test_pool"

