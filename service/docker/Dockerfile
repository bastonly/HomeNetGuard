FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# 添加 ARM64 架构支持并配置源
RUN dpkg --add-architecture arm64 \
    && sed -i 's/^deb/deb [arch=amd64]/' /etc/apt/sources.list \
    && echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ jammy main restricted universe multiverse" >> /etc/apt/sources.list \
    && echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ jammy-updates main restricted universe multiverse" >> /etc/apt/sources.list \
    && echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ jammy-backports main restricted universe multiverse" >> /etc/apt/sources.list \
    && echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ jammy-security main restricted universe multiverse" >> /etc/apt/sources.list

# 安装基础工具和 ARM aarch64 交叉编译工具链
RUN apt-get update && apt-get install -y \
    build-essential \
    g++-aarch64-linux-gnu \
    git \
    wget \
    curl \
    python3 \
    python3-pip \
    pkg-config \
    file \
    libcurl4-openssl-dev:arm64 \
    libsqlite3-dev:arm64 \
    && rm -rf /var/lib/apt/lists/*

# 设置交叉编译环境变量
ENV CC=aarch64-linux-gnu-gcc
ENV CXX=aarch64-linux-gnu-g++
ENV AR=aarch64-linux-gnu-ar
ENV RANLIB=aarch64-linux-gnu-ranlib
ENV STRIP=aarch64-linux-gnu-strip

WORKDIR /build

CMD ["/bin/bash"]
