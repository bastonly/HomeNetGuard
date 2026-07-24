#!/bin/bash
set -e

# Docker 容器内编译脚本
# 使用 ARM aarch64 交叉编译工具链编译 pc_service

echo "=== ARM aarch64 交叉编译开始 ==="

cd /build

# 1. 清理之前的构建
make clean || true

# 2. 使用交叉编译工具链构建
echo "正在编译..."
make CXX=aarch64-linux-musl-g++ \
     CC=aarch64-linux-musl-gcc \
     AR=aarch64-linux-musl-ar \
     RANLIB=aarch64-linux-musl-ranlib \
     CXXFLAGS="-std=c++17 -Iinclude -I/opt/aarch64-linux-musl-cross/aarch64-linux-musl/include -Wall -O2 -static" \
     LDFLAGS="-static -L/opt/aarch64-linux-musl-cross/aarch64-linux-musl/lib -lcurl -lssl -lcrypto -lsqlite3 -lhiredis -lz -lpthread -ldl"

# 3. 验证生成的二进制文件
if [ -f pc_service ]; then
    echo "✅ 编译成功！"
    file pc_service
    ls -lh pc_service
    
    # Strip 调试符号以减小体积
    aarch64-linux-musl-strip pc_service
    echo "✅ 已 strip 调试符号"
    ls -lh pc_service
else
    echo "❌ 编译失败：未找到 pc_service"
    exit 1
fi

echo "=== 编译完成 ==="
