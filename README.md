# 智能网关家长控制与上网行为管理系统 / HomeNetGuard: Gateway Parent Control & Behavior Management System

[English](#english) | [简体中文](#简体中文)

---

## 简体中文

### 📌 项目简介
**HomeNetGuard** 是一款运行于智能网关路由器（如基于 OpenWrt / Linux 的网关设备或树莓派）的**家长控制与上网行为管理系统**。通过结合底层的 C++ 高性能策略控制引擎与上层的 Python (Flask) Web 管理接口，实现了基于设备、时间段、日期类型的网络精细化阻断策略、临时上网授权、娱乐/游戏/视频防沉迷检测以及 AdGuard Home 的深度整合。

系统的核心逻辑是将前端 Vue 3 管理后台、Python API 服务以及 C++ 守护进程 (`pc_service`) 相结合，实现了高性能的底层网络流量拦截与高可用的配置管理。

### 🏗️ 系统架构
以下是本系统的核心组件与数据流向图：

```mermaid
graph TD
    User([管理员/用户]) -->|操作界面| Frontend[Vue 3 前端控制面板]
    Frontend -->|REST API| Backend[Python Flask 后端]
    Backend -->|读写配置与日志| DB[(SQLite 数据库)]
    Backend -->|多语言与缓存| Redis[(Redis 缓存/消息)]
    
    subgraph 路由器/网关底层 (Gateway Core)
        Daemon[C++ 守护进程 pc_service]
        Daemon -->|读取配置与策略| DB
        Daemon -->|定时同步状态| Redis
        Daemon -->|调用 / 执行 nftables| Firewall[nftables 防火墙阻断]
        Daemon -->|解析 & API 联动| AdGuard[AdGuard Home 过滤]
    end
```

### 🌟 核心功能
1. **多维度策略阻断**：
   - **基于时间段 (Period)**：允许家长配置每日特定时间段（如 `18:00 - 20:00`）内允许设备联网。
   - **基于限时 (Duration)**：在特定日期（如法定节假日）内，给设备指定上网的总时长限制。
   - **智能日期分类**：工作日、周末和法定节假日（支持法定节假日和调休的动态配置）。
2. **临时上网授权 (Temporary Pass)**：
   - 允许某台设备在指定时间段内（例如 30 分钟）绕过所有网络阻断策略。
3. **AdGuard Home 深度联动与防沉迷检测**：
   - 定期解析与分析 AdGuard DNS 查询日志，汇总各设备分类流量，实现 DNS 级别的违规服务/娱乐域名阻断与行为告警。
4. **轻量级 & 高性能**：
   - 底层由高效的 C++ 守护进程直接与系统防火墙（`nftables`）和 SQLite/Redis 交互，占用极低 CPU 和内存。
   - 上层由轻量级 Python Flask API 提供强交互，并基于 Redis 实现跨进程状态的低延迟分发。

### 📁 目录结构
* **[`frontend/`](./frontend)**：基于 Vue 3 + Vite 构建的管理后台，提供设备管理、策略配置、报表统计等响应式 UI。
* **[`backend/`](./backend)**：基于 Flask + SQLite3 + Redis 的 Python 后端，提供 RESTful APIs，处理配置持久化与接口鉴权。
* **[`service/`](./service)**：C++ 编写的核心守护进程项目（包含完整的 Makefile、CMake 与 Docker 构建脚本）。
* **[`readme/`](./readme)**：SQL 数据库建表脚本与架构补充文档。

---

## English

### 📌 Project Overview
**HomeNetGuard** is an enterprise-grade **Parental Control & Internet Behavior Management System** deployed on smart gateway routers (e.g., OpenWrt / Linux gateways or Raspberry Pi). By combining a high-performance C++ control engine with a Python Flask Web API, it implements device-based, schedule-based internet access blocking, temporary access authorization, entertainment/gaming anti-addiction detection, and seamless AdGuard Home integration.

### 🏗️ Architecture & Dataflow
```mermaid
graph TD
    User([Admin/User]) -->|Manage UI| Frontend[Vue 3 Frontend Dashboard]
    Frontend -->|REST API| Backend[Python Flask Backend]
    Backend -->|Read/Write Config| DB[(SQLite Database)]
    Backend -->|Multilingual Dict| Redis[(Redis Cache/IPC)]
    
    subgraph Gateway Core (Router OS)
        Daemon[C++ Daemon pc_service]
        Daemon -->|Fetch Policy/Device| DB
        Daemon -->|Sync Config| Redis
        Daemon -->|Manipulate Rules| Firewall[nftables Firewall Block]
        Daemon -->|REST Sync / QueryLog| AdGuard[AdGuard Home Engine]
    end
```

### 🌟 Key Features
1. **Multi-dimensional Firewall Policies**:
   - **Time Range Blocks**: Restrict internet access to specific timeframes (e.g., `18:00 - 20:00`).
   - **Time Duration limits**: Set total daily limit quotas (e.g., 2 hours limit on holidays).
   - **Smart Date Classification**: Handles Workdays, Weekends, and Holidays automatically.
2. **Temporary Overrides (Temp Access)**:
   - Grant immediate, time-bound internet access bypass (e.g., extend 30 mins) for specific MAC addresses.
3. **AdGuard Home Analytics & Anti-Addiction Detection**:
   - Periodically audits AdGuard DNS query logs and triggers behavioral warnings or blocks for entertainment domains.
4. **Low Resource Footprint**:
   - Microsecond-level firewall execution powered by native C++ interacting with `nftables`.

---

## 🛠️ 如何开始 / Getting Started

### 1. 准备配置文件 / Prepare Configuration
首先复制模板配置文件并进行基础设置 / Copy the template config file:
```bash
cp backend/config/base.conf.example backend/config/base.conf
```
根据实际网络参数修改 `base.conf` 里的配置（如 Redis 地址、AdGuard Home URL 与密码等）。

### 2. 编译 C++ 守护进程 / Compile C++ Daemon
在宿主机或 OpenWrt 交叉编译工具链中编译：
```bash
cd service
cmake .
make
```

### 3. 运行 Python 后端 / Start Python API Backend
```bash
cd backend
pip install -r requirements.txt
python app.py
```

### 4. 运行 Vue 3 前端 / Start Vue 3 Frontend
```bash
cd frontend
npm install
npm run dev
```

---

## 🚀 发布到 GitHub / Push to GitHub

要在 Github 上托管此项目，请运行以下 Git 命令 / Run the following steps to push to GitHub:

```bash
# 1. 初始化本地仓库 (如未初始化) / Initialize Git repository
git init

# 2. 添加所有变更 / Stage all files
git add .

# 3. 提交变更 / Commit changes
git commit -m "feat: initial commit of HomeNetGuard"

# 4. 重命名默认分支为 main / Rename branch to main
git branch -M main

# 5. 关联 GitHub 远程仓库 / Link GitHub remote URL
git remote add origin https://github.com/<YOUR_USERNAME>/HomeNetGuard.git

# 6. 推送到 GitHub / Push to GitHub
git push -u origin main
```

---

## 📄 License
This project is licensed under the [MIT License](./LICENSE).
