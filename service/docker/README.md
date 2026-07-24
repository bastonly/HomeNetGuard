# Docker 交叉编译环境

使用 Docker 在服务器上交叉编译 ARM aarch64 版本的 `pc_service`。

## 快速开始

```bash
cd service/docker
./compile-on-server.sh <你的服务器地址>
```

编译产物将保存到 `service/pc_service`。

## 文件说明

- `Dockerfile`: Ubuntu 22.04 + ARM aarch64 交叉编译工具链镜像
- `build-in-docker.sh`: 容器内编译脚本
- `compile-on-server.sh`: 远程服务器编译主脚本

## 使用步骤

### 1. 在服务器上编译

```bash
./compile-on-server.sh myserver.com
```

脚本会自动：
1. 同步源代码到服务器
2. 构建 Docker 镜像
3. 在容器中交叉编译
4. 下载编译产物到本地

### 2. 部署到路由器

```bash
cd ..
scp pc_service client:/opt/parent-control/service/
ssh client "chmod +x /opt/parent-control/service/pc_service"
```

### 3. 验证

```bash
ssh client "/opt/parent-control/service/pc_service --help"
```

## 高级用法

### 手动编译（在服务器上）

```bash
# 1. SSH 登录服务器
ssh myserver.com

# 2. 进入构建目录
cd /tmp/pc-service-build

# 3. 构建镜像
cd docker && docker build -t pc-service-cross-compiler .

# 4. 运行编译
cd ..
docker run --rm -v $(pwd):/build -w /build \
    pc-service-cross-compiler bash docker/build-in-docker.sh

# 5. 检查产物
file pc_service
```

### 自定义编译参数

编辑 `build-in-docker.sh`，修改 make 命令的参数。

## 故障排查

### Docker 镜像构建失败
- 检查服务器网络连接
- 尝试使用国内镜像源

### 编译失败
- 检查源代码是否完整
- 查看容器内的编译日志

### 运行时错误
```bash
# 在路由器上检查依赖
ssh client "ldd /opt/parent-control/service/pc_service"
```

## 清理

```bash
# 清理服务器上的构建文件
ssh myserver.com "rm -rf /tmp/pc-service-build"

# 清理 Docker 镜像
ssh myserver.com "docker rmi pc-service-cross-compiler"
```
