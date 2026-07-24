#!/bin/bash
set -e

# 在远程服务器上使用 Docker 交叉编译 pc_service
# 用法: ./compile-on-server.sh <ssh-host> [remote-build-dir]

if [ $# -lt 1 ]; then
    echo "用法: $0 <ssh-host> [remote-build-dir]"
    echo "示例: $0 myserver.com"
    echo "示例: $0 myserver.com /tmp/pc-service-build"
    exit 1
fi

SSH_HOST="$1"
REMOTE_DIR="${2:-/tmp/pc-service-build}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVICE_DIR="$(dirname "${SCRIPT_DIR}")"

echo "服务器: ${SSH_HOST}"
echo "远程目录: ${REMOTE_DIR}"
echo "本地 service 目录: ${SERVICE_DIR}"
echo ""

echo "=== 1/4 同步代码到服务器 ==="
ssh ${SSH_HOST} "mkdir -p ${REMOTE_DIR}"
rsync -avz --delete \
    --exclude 'pc_service' \
    --exclude '*.o' \
    --exclude 'stubs' \
    --exclude 'docker' \
    "${SERVICE_DIR}/" \
    "${SSH_HOST}:${REMOTE_DIR}/"

# 同步 docker 目录
rsync -avz \
    "${SCRIPT_DIR}/" \
    "${SSH_HOST}:${REMOTE_DIR}/docker/"

echo ""
echo "=== 2/4 在服务器上构建 Docker 镜像 ==="
ssh ${SSH_HOST} "cd ${REMOTE_DIR}/docker && docker build -t pc-service-cross-compiler ."

echo ""
echo "=== 3/4 在服务器 Docker 中编译 ==="
ssh ${SSH_HOST} "cd ${REMOTE_DIR} && docker run --rm -v \${PWD}:/build -w /build pc-service-cross-compiler bash docker/build-in-docker.sh"

echo ""
echo "=== 4/4 下载编译产物 ==="
scp "${SSH_HOST}:${REMOTE_DIR}/pc_service" "${SERVICE_DIR}/pc_service"

echo ""
echo "✅ 编译完成！"
file "${SERVICE_DIR}/pc_service"
ls -lh "${SERVICE_DIR}/pc_service"

echo ""
echo "现在可以部署到路由器："
echo "scp ${SERVICE_DIR}/pc_service client:/opt/parent-control/service/"
