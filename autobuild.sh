#!/bin/bash
set -x

BUILD_DIR="$PWD/build"

# 如果 build 目录不存在，就创建它
mkdir -p "$BUILD_DIR"

# 清理旧文件（如果有）
rm -rf "$BUILD_DIR"/*

# 进入 build 目录并编译
cd "$BUILD_DIR" && cmake .. && make