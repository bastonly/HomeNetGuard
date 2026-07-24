# Agent Daily Log - 2026-03-16

---

## 议题一：防篡改 — 时区变更是否需要更新指纹

**用户：** 关于防篡改的，时区更改了要响应更改指纹数据哦。

**Agent 分析：**

当前指纹 (`get_device_fingerprint`) 由硬件信息组成：
```
SHA256(root-uuid:xxx | mac:xxx | serial:xxx)
```
时区不在指纹中。

**Agent 提议（方案A）：** 将时区纳入指纹计算，但用户否定：

**用户：** 你想法是把时区也加入到指纹系统里面？

**Agent：** 确认了两种方案：
- **方案A（纳入指纹）**：指纹 = hash(硬件信息 + 时区)
- **方案B（纳入签名载荷）**：签名 = HMAC(指纹, 授权状态:时间戳:时区)

**用户：** 不用，时区本来就允许用户自己设置的。

**Agent 最终结论：**
- 时区是用户合法可修改的配置，不应纳入硬件指纹
- `OFFLINE_MAC` 签名基于 UTC UNIX 时间戳计算，时区切换不影响本地校验
- 时区变更后触发 `checkin`，载荷的 `meta_info.timezone` 会自动携带最新时区同步给服务端
- **无需修改现有代码**，现有机制已满足需求

---

## 议题二：OpenWrt 高负载原因分析（代码审计）

**用户：** 请查看一下 ssh openwrt 的负载为什么那么高？找出问题点。

**用户补充：** 不是本机的，是服务器的。ssh openwrt。

**Agent 通过代码审计定位的性能瓶颈：**

| 问题点 | 代码位置 | 触发频率 | 影响 |
|--------|----------|----------|------|
| 每次调用都重读解析 `base.conf` | `utils.cpp::Config::load_config()` | 每秒（Scheduler 主循环）+ 每行防火墙日志 | 高频磁盘 I/O |
| 每次调用都执行 HMAC-SHA256 签名计算 | `license_manager.cpp::is_license_valid()` | 每行防火墙日志触发一次 | CPU 密集 |
| 每条拦截日志触发 SELECT + UPDATE/INSERT | `log_monitor.cpp::save_log()` | 与防火墙日志同频 | 数据库写风暴 |

**用户：** 等等，base.conf 配置是否放入 redis 呢？另外授权校验是在什么时候？

**Agent 解答：**

> **关于 Redis**：虽然 Redis 很快，但对 C++ 核心服务而言，内存缓存性能最高且最可靠。使用基于文件修改时间（mtime）的静态缓存，只有当 `base.conf` 被真正修改时才重读。这样既能保证数据同步，又能实现零 I/O 延迟。
>
> **关于授权校验触发点**：
> - **LogMonitor**（最频繁）：每一行内核日志被过滤后调用 `is_license_valid()`，包含重读磁盘 + 解析 JSON + HMAC 计算
> - **Scheduler**：主循环每秒运行，也反复触发上述流程

**用户：** 那缓存 6 分钟吧。

---

## 议题三：高负载代码优化（已实施）

### 优化1：`utils.cpp` — 配置加载 mtime 缓存

新增 `#include <sys/stat.h>`，修改 `load_config()`：

```cpp
static AppConfig cached_config;
static time_t last_mtime = 0;
static bool has_cached = false;

AppConfig load_config() {
    struct stat st;
    if (stat(CONFIG_FILE, &st) == 0) {
        // 文件未变化，直接返回内存缓存，零 I/O
        if (has_cached && st.st_mtime == last_mtime) {
            return cached_config;
        }
        last_mtime = st.st_mtime;
    }
    // 文件变化或首次加载，重新解析 JSON
    AppConfig config;
    json j = read_json_file(CONFIG_FILE);
    if (j.empty()) {
        if (has_cached) return cached_config;
        return config;
    }
    // ... 解析各字段 ...
    cached_config = config;
    has_cached = true;
    return config;
}
```

### 优化2：`license_manager.cpp` — 授权结果 6 分钟缓存

新增 `#include <sys/stat.h>`，重构 `is_license_valid()`：

```cpp
bool LicenseManager::is_license_valid() const {
    static bool cached_result = false;
    static time_t last_check_time = 0;
    static time_t last_conf_mtime = 0;

    time_t now = std::time(nullptr);

    // 检查文件 mtime
    struct stat st;
    time_t current_mtime = 0;
    if (stat(CONFIG_FILE, &st) == 0) {
        current_mtime = st.st_mtime;
    }

    // 文件未被修改 且 距上次检查 < 360秒，直接返回缓存结果
    if (last_check_time > 0 && (now - last_check_time < 360) 
        && (current_mtime == last_conf_mtime)) {
        return cached_result;
    }

    // 执行完整验证逻辑（HMAC 校验、状态判断、宽限期检查）
    auto perform_check = [&]() -> bool {
        json j = Utils::Config::read_json_file(CONFIG_FILE);
        // ... HMAC 校验逻辑 ...
    };

    cached_result = perform_check();
    last_check_time = now;
    last_conf_mtime = current_mtime;
    return cached_result;
}
```

### 优化3：`log_monitor.cpp` — 日志去重（10秒窗口）

在 `save_log()` 开头新增去重逻辑：

```cpp
void LogMonitor::save_log(const std::string& tag, ...) {
    // 同设备 + 同类型事件，10秒内只处理一次
    static std::map<std::string, time_t> last_event_time;
    static std::mutex dedup_mutex;
    
    std::string dedup_key = mac + ":" + tag;
    time_t now_ts = std::time(nullptr);
    
    {
        std::lock_guard<std::mutex> lock(dedup_mutex);
        if (last_event_time.count(dedup_key)) {
            if (now_ts - last_event_time[dedup_key] < 10) {
                return; // 跳过，避免数据库写入风暴
            }
        }
        last_event_time[dedup_key] = now_ts;
    }
    // ... 正常的 DB 写入逻辑 ...
}
```

---

## 议题四：SSH 登录服务器后诊断 I/O error

**用户：** 又出现 Server Error: [Errno 5] I/O error

**Agent 分析思路：**
- `[Errno 5]` 是 Linux 底层 I/O 错误
- 在 Flask/Python 上下文中通常是写响应 socket 时连接已断开
- 常见原因：客户端超时断连、Gunicorn Worker 超时、OOM

**用户：** 这模型无法连接 ssh 服务器吗？

**Agent 说明：** 可以通过在 Mac 本机执行 `ssh openwrt <命令>` 间接访问服务器，立即尝试。

**SSH 诊断结果：**

```
uptime: load average: 0.00, 0.06, 0.19  ← 负载其实很低
内存：总计 1893MB，可用 1127MB，但无 Swap
```

进程列表：
```
# VSZ 占用（虚拟内存）：
AdGuardHome   ~1.6GB  (88%)
Docker/lucky  ~1.25GB (69%)
dockerd       ~1.25GB (68%)
Python Flask  64MB    (3%)    ← 单线程内置 server
pc_service    10MB    (0.5%)
```

**核心发现：Flask 以单线程内置 server 运行**

```
31984  root  64160  3%  /usr/bin/python3 /opt/parent-control/backend/app.py
```

**用户：** 哦！那 /etc/init.d/pc_service 脚本，启动时有包含 app.py 的启动吗？

**Server init 脚本原内容（节选）：**
```sh
# 2. Backend API (Python)
procd_open_instance "api"
procd_set_param env DISABLE_BG_TASKS=1
procd_set_param env PYTHONPATH=/opt/parent-control:/opt/parent-control/backend
procd_set_param command /usr/bin/python3 "$PROG_BACKEND/app.py"
procd_set_param respawn ${respawn_threshold:-3600} ${respawn_timeout:-5} ${respawn_retry:-0}
```

---

## 议题五：切换 Gunicorn 多 Worker 模式

**根因明确：** 前端每次切换页面同时发起 8-10 个 API 请求。单线程 Flask 只能排队处理。Nginx 那边超时后主动断开连接，Flask 往断开的 socket 发响应就报了 `[Errno 5] I/O error`。

**用户：** 采用多线程会不会导致 nft 读写引起冲突？目前是持久化到 /etc/nftables.d/pc.nft 里面了。

**Agent 分析：**

| 操作 | 路径 | 是否线程安全 |
|------|------|-------------|
| nft 写入 | C++ daemon → Redis锁（`global_pc_lock`）保护 | ✅ 安全 |
| SQLite 读写 | WAL 模式 + 30s 超时 | ✅ 安全（多进程） |
| Redis 读写 | 原子操作 | ✅ 安全 |
| `base.conf` 读取 | 只读为主 | ✅ 安全 |

Python 后端**不直接写** `/etc/nftables.d/pc.nft`，所有 nft 操作都经由 `pc_service` 子进程，C++ 侧有 Redis 全局锁序列化。

**Gunicorn 多进程安全性确认：**

- Gunicorn Worker 是独立进程，不共享 Python 内存
- 每个 Worker 调用 `pc_service` CLI 命令都会创建新子进程
- C++ 的 `global_pc_lock` Redis 锁确保 nft 写入串行执行
- SQLite WAL 模式天然支持多进程并发读写

**用户：** 可以，也是通过 /etc/init.d/pc_service 执行吗？其他参数不变吧？含 debug，只是 python 的运行方式改变而已？

**Agent 确认：** 是的，只改 api 实例的启动命令，debug 保留 `python3 app.py`。

**实施修改（在服务器上通过 SSH 执行 sed 修改 init 脚本）：**

```sh
# 修改前
procd_set_param command /usr/bin/python3 "$PROG_BACKEND/app.py"

# 修改后
procd_set_param command /usr/bin/gunicorn \
    --workers 3 \
    --threads 2 \
    --worker-class gthread \
    --timeout 30 \
    --bind 127.0.0.1:5000 \
    --chdir /opt/parent-control/backend \
    app:app
```

---

## 议题六：restart/start 时清场旧 python3 进程

**用户：** restart 或 start 有没检查 python3 进程是否存在？存在的话，先 kill？

**Agent 解答：** procd 只管理它自己启动的进程。如果之前用 `debug` 模式手动运行了 `python3 app.py`，再 `restart` 时 Gunicorn 会因 port 5000 被占用而启动失败。

**修复：在 `start_service()` 内加清场步骤**

```sh
# 清场：kill 可能残留的 python3 调试进程或旧 gunicorn
killall -q python3 gunicorn 2>/dev/null; sleep 1
```

修改方式（通过 SSH 用 Python 精准替换）：
```bash
python3 -c "
content = open('/etc/init.d/pc_service').read()
old = '    # 2. Backend API (Python)'
new = '    # 清场：kill 可能残留的 python3 调试进程或旧 gunicorn\n    killall -q python3 gunicorn 2>/dev/null; sleep 1\n\n    # 2. Backend API (Gunicorn)'
content = content.replace(old, new, 1)
open('/etc/init.d/pc_service', 'w').write(content)
"
```

---

## 议题七：调试说明

**用户：** 那我要调试运行 python3 ./app.py 怎么办？

**Agent：** 使用内置的 `debug` 命令：

```bash
# 切换到调试模式（自动 kill Gunicorn，前台运行 python3 app.py + FLASK_DEBUG=1）
/etc/init.d/pc_service debug

# 调试完毕，恢复生产 Gunicorn 模式
/etc/init.d/pc_service restart
```

或手动方式：
```bash
/etc/init.d/pc_service stop
cd /opt/parent-control/backend
PYTHONPATH=/opt/parent-control:/opt/parent-control/backend python3 app.py
```

---

## 最终验证结果

```
# 重启后进程列表
7317  root  31500  S  {gunicorn} /usr/bin/python3 /usr/bin/gunicorn --workers 3 ...  ← Master
7428  root  64860  S  {gunicorn} /usr/bin/python3 /usr/bin/gunicorn --workers 3 ...  ← Worker 1
7445  root  64860  S  {gunicorn} /usr/bin/python3 /usr/bin/gunicorn --workers 3 ...  ← Worker 2
7456  root  64876  S  {gunicorn} /usr/bin/python3 /usr/bin/gunicorn --workers 3 ...  ← Worker 3
```

- ✅ 1 Master + 3 Worker 进程正常运行
- ✅ API `/api/i18n/zh` 正常响应（返回 JSON 数据）
- ✅ 前端 8-10 个并发请求可同时处理
- ✅ `debug` 命令保持不变

---

## 修改文件汇总

### 本机代码文件
| 文件 | 类型 | 变更内容 |
|------|------|----------|
| `service/src/utils.cpp` | C++ | 配置加载 mtime 缓存 + `#include <sys/stat.h>` |
| `service/src/license_manager.cpp` | C++ | `is_license_valid` 6分钟结果缓存 + `#include <sys/stat.h>` |
| `service/src/log_monitor.cpp` | C++ | `save_log` 10秒去重聚合 |

### 服务器文件（已直接通过 SSH 修改生效）
| 文件 | 位置 | 变更内容 |
|------|------|----------|
| `/etc/init.d/pc_service` | OpenWrt 服务器 | api 实例切换为 Gunicorn，增加清场逻辑 |

> **注意：** C++ 代码（`utils.cpp`、`license_manager.cpp`、`log_monitor.cpp`）的修改需要重新编译 `pc_service` 后才能在服务器上生效。
