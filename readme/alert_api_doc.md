# Alert API 文档 — 篡改检测上报

## 接口概述

当 `pc_service` 检测到本地配置被篡改（HMAC 校验失败或时钟回拨），会自动将授权状态设为 `suspended`，并异步向远程服务器上报告警。

---

## 请求信息

### 地址

```
POST {server_url}/api/v{ver}/alert
```

> `ver` 取自本地 config 中 `current_api_version` 的主版本号（例如 `"1.01"` → `"1"`）

### Headers

| Header | 值 |
|---|---|
| `Content-Type` | `application/json` |
| `X-Customer-Token` | config 中的 `api_token`（若存在则添加） |

---

## 请求 Body

```json
{
  "license_id": "YOUR_LICENSE_ID",
  "device_fingerprint": "Hardware fingerprint of the device",
  "alert_type": "tampering_detected",
  "details": "HMAC verification failed. Local configuration tampering detected.",
  "meta_info": {
    "hostname": "ImmortalWrt",
    "client_date": "2026-03-09 15:05:49",
    "timezone": "Asia/Shanghai"
  }
}
```

### 字段说明

| 字段 | 类型 | 说明 |
|---|---|---|
| `license_id` | string | 设备 config 中的 License ID |
| `device_fingerprint` | string | 设备硬件指纹（`Utils::get_device_fingerprint()` 生成） |
| `alert_type` | string | 固定值：`"tampering_detected"` |
| `details` | string | 详细原因，见下文「触发场景」 |
| `meta_info.hostname` | string | 路由器 hostname |
| `meta_info.client_date` | string | 本地当前时间，格式 `YYYY-MM-DD HH:MM:SS` |
| `meta_info.timezone` | string | 本地时区，如 `"Asia/Shanghai"` |

---

## 触发场景

| 场景 | `details` 值 |
|---|---|
| HMAC 校验失败（config 被手动篡改） | `"HMAC verification failed. Local configuration tampering detected."` |
| 时钟回拨（系统时间早于宽限期起始时间超过 1 天） | `"System time is before grace start time. Time tampering detected."` |

---

## 行为说明

1. 触发函数：`LicenseManager::suspend_tampered_license(reason)`
2. **本地处理（同步）**：
   - 将 config 中 `LICENSE_STATUS` 强制写为 `"suspended"`
   - 清除 `GRACE_START_TIME` 和 `OFFLINE_MAC` 字段
   - 写入 `sys_messages` 安全审计日志（level: `WARNING`，tag: `AUTHORIZATION_FAILURE`，event: `cpp.TAMPER_DETECTED`）
3. **远程上报（异步）**：
   - 在独立子线程（`std::thread().detach()`）中发送 HTTP POST
   - 请求超时：**10 秒**
   - 发送失败不重试，不影响主流程

---

## 相关源码

- 触发逻辑：[`license_manager.cpp`](../service/src/license_manager.cpp) — `suspend_tampered_license()` 函数（L95–L151）
- 触发条件：`is_license_valid()` 函数（L189–L198）
