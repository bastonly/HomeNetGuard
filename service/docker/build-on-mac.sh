#!/bin/bash
# 本地 Mac 编译脚本 - 使用 Docker musl 工具链编译 ARM64 版本的 pc_service
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVICE_DIR="$(dirname "${SCRIPT_DIR}")"

# 定义 Docker 命令 (优先使用标准 docker 命令)
if command -v docker > /dev/null 2>&1; then
    DOCKER_CMD="docker"
else
    DOCKER_BIN="/Applications/Docker.app/Contents/Resources/bin/docker"
    DOCKER_SOCK="${HOME}/.docker/run/docker.sock"
    DOCKER_CMD="$DOCKER_BIN -H unix://$DOCKER_SOCK"
fi

echo "=== 检查 Docker ==="
if ! $DOCKER_CMD info > /dev/null 2>&1; then
    echo "❌ 无法连接到 Docker"
    echo "尝试使用的 Docker 路径: $DOCKER_BIN"
    echo "尝试使用的 Socket: $DOCKER_SOCK"
    echo "请确保 Docker Desktop 已运行且 socket 存在。"
    exit 1
fi

echo "✅ Docker 运行中 ($($DOCKER_CMD --version))"
echo ""

echo "=== 构建 Docker 镜像（Alpine + musl 工具链）==="
$DOCKER_CMD build --platform linux/amd64 \
    --network=host \
    -f "${SCRIPT_DIR}/Dockerfile.alpine" \
    -t pc-service-musl-compiler \
    "${SCRIPT_DIR}"

echo ""
echo "=== 在 Docker 中编译 ARM aarch64 版本（musl）==="
$DOCKER_CMD run --rm --platform linux/amd64 \
    -v "${SERVICE_DIR}:/build" \
    -w /build \
    pc-service-musl-compiler \
    bash docker/build-in-docker.sh

echo ""
echo "=== 验证编译产物 ==="
if [ -f "${SERVICE_DIR}/pc_service" ]; then
    echo "✅ 编译成功！"
    file "${SERVICE_DIR}/pc_service"
    ls -lh "${SERVICE_DIR}/pc_service"
    echo ""
    echo "📦 二进制文件: ${SERVICE_DIR}/pc_service"
    echo ""
    echo "下一步："
    echo "  scp pc_service client:/opt/parent-control/service/"
else
    echo "❌ 编译失败"
    exit 1
fi
