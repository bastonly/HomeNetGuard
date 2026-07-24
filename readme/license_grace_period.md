# 授权宽限期与防篡改机制设计 (License Grace Period & Anti-Tamper Design)

## 宽限期规则

| 状态 | 宽限期起算点 | 宽限期时长 | 网络访问 |
|---|---|---|---|
| **Expired** (自然过期) | `EXPIRED_TS`（服务端返回的精确过期日期） | 30 天 | 倒计时内放行，过期则阻断 |
| **Suspended** (暂停) | 今天（设备收到暂停指令当日） | 30 天 | 倒计时内放行，过期则阻断 |
| **Revoked** (硬吊销 / 篡改检测) | — | **无宽限期** | **立即断网** |
| **断网离线** (Online → Offline) | 最后一次成功 checkin 时刻 | 30 天 | 倒计时内放行，过期则阻断 |

## 防篡改签名 (OFFLINE_MAC)

`OFFLINE_MAC` 是一个 HMAC-SHA256 签名，用于防止用户通过 SSH 直接修改 `base.conf` 配置文件来延长宽限期或伪造授权状态。

**签名公式：**
```
HMAC-SHA256(设备硬件指纹, "LAST_CHECKIN_TS:GRACE_START_TS:EXPIRED_TS")
```

**字段说明：**

| 字段 | 含义 | 何时更新 |
|---|---|---|
| `LAST_CHECKIN_TS` | 最后一次成功联网 checkin 的 Unix 时间戳 | 每次成功 checkin 时自动刷新 |
| `GRACE_START_TS` | 宽限期起算点 Unix 时间戳 | 进入宽限期时首次写入，之后不变 |
| `EXPIRED_TS` | 授权过期日期 Unix 时间戳 | 每次成功 checkin 时从服务端同步 |

**篡改检测流程：**
1. 每次 `is_license_valid()` 执行时，从本地读取以上三个 TS 字段，使用设备指纹重新计算一遍 HMAC。
2. 若计算结果与本地保存的 `OFFLINE_MAC` 不一致，则判定为配置被篡改。
3. 系统自动将授权状态写为 `Revoked`（硬吊销，无宽限期），向服务端发送篡改告警（`/api/v1/alert`），并立即执行 `nftables` 断网规则。

## checkin 签名同步

每次设备成功调用 checkin API，无论返回的授权状态是 Active 还是 Expired/Revoked，都会：
1. 更新本地 `LAST_CHECKIN_TS` 和 `EXPIRED_TS`。
2. 用最新的三个 TS 重新计算并覆盖写入 `OFFLINE_MAC`。

这确保了签名的时效性，防止攻击者利用老旧的签名进行重放攻击。

## 篡改告警请求 (Tamper Alert)

当 HMAC 校验失败时，`pc_service` 会异步向服务端发送一条告警请求。

**请求地址：** `POST /api/v1/alert`

**请求头：**
```
Content-Type: application/json
X-Customer-Token: <api_token>
```

**请求体 Payload：**
```json
{
  "license_id": "your-license-id",
  "device_fingerprint": "aabbccdd...",
  "alert_type": "tampering_detected",
  "details": "<篡改原因说明>",
  "meta_info": {
    "hostname": "OpenWrt",
    "client_date": "2026-03-09 14:51:30",
    "timezone": "America/New_York"
  }
}
```

**`details` 字段的可能取值：**

| 值 | 触发条件 |
|---|---|
| `HMAC verification failed. Local configuration tampering detected.` | `OFFLINE_MAC` 与本地重算的签名不一致，说明 `EXPIRED_TS`、`GRACE_START_TS` 或 `LAST_CHECKIN_TS` 被手动修改 |
| `System time is before grace start time. Time tampering detected.` | 系统时钟回拨，早于宽限期起始时间，疑似手动拨回系统时间 |
