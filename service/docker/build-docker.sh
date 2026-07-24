#!/bin/bash
set -e

# 本地 Docker 编译主脚本（如果本地有 Docker）
# 大部分情况下应该使用 compile-on-server.sh 在服务器上编译

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVICE_DIR="$(dirname "${SCRIPT_DIR}")"

echo "=== 构建 Docker 镜像 ==="
docker build -f "${SCRIPT_DIR}/Dockerfile.alpine" -t pc-service-cross-compiler "${SCRIPT_DIR}"

echo ""
echo "=== 在 Docker 中交叉编译 pc_service (ARM aarch64) ==="
docker run --rm \
    -v "${SERVICE_DIR}:/build" \
    -w /build \
    pc-service-cross-compiler \
    bash docker/build-in-docker.sh

echo ""
echo "=== 验证编译产物 ==="
if [ -f "${SERVICE_DIR}/pc_service" ]; then
    echo "✅ 交叉编译成功！"
    file "${SERVICE_DIR}/pc_service"
    ls -lh "${SERVICE_DIR}/pc_service"
    echo ""
    echo "编译产物已保存到: ${SERVICE_DIR}/pc_service"
    echo "可以直接部署到路由器使用"
else
    echo "❌ 未找到编译产物"
    exit 1
fi
