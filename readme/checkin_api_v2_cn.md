# Checkin 接口文档 (v2)

## 1. 设备签到 (Checkin)

设备定期向服务器发送心跳，汇报状态并获取最新的授权信息。

**接口地址**: `/api/checkin`
**请求方式**: `POST`

### 请求头 (Headers)

| 参数名 | 必选 | 类型 | 说明 |
| :--- | :--- | :--- | :--- |
| `X-Customer-Token` | 是 | `string` | 客户的 API Token (Customer API Key)，用于身份验证 |

### 请求参数 (Body)

| 参数名 | 必选 | 类型 | 说明 |
| :--- | :--- | :--- | :--- |
| `license_id` | 是 | `string` | 授权码 ID |
| `device_fingerprint` | 是 | `string` | 设备唯一指纹 |
| `meta_info` | 否 | `object` | 设备的元数据信息 (JSON对象) |

**请求示例**:

```json
{
  "license_id": "LIC-12345678",
  "device_fingerprint": "a1b2c3d4e5",
  "meta_info": {
    "ip": "192.168.1.10",
    "host": "server-01"
  }
}
```

### 响应参数 (Response)

| 参数名 | 类型 | 说明 |
| :--- | :--- | :--- |
| `status` | `string` | 请求状态: `ok` (成功) 或 `fail` (失败) |
| `message` | `string` | 说明性文字，包括错误原因或状态说明 |
| `license_status` | `int` | 授权状态码: <br>0: PENDING (待激活)<br>1: ISSUED (已颁发/正常)<br>2: EXPIRED (已过期)<br>3: REVOKED (已吊销)<br>4: SUSPENDED (已暂停) |
| `device_status` | `int` | 设备状态码: <br>0: FAIL (失败/无效)<br>1: ACTIVE (激活/正常)<br>2: UNBOUND (未绑定/解绑)<br>3: REVOKED (已拉黑) |
| `token_issued` | `int` | Token 发行状态 (1 表示成功) |
| `expired_date` | `datetime` | 授权过期时间 (ISO 8601 格式) |
| `lastest_api_version`| `string` | 服务器当前的最新 API 版本号 |
| `server_time` | `datetime` | 服务器当前时间 (美东时间 ISO 8601 格式) |

### 响应示例

#### 1. 成功 (Success)

```json
{
    "status": "ok",
    "message": null,
    "license_status": 1,
    "device_status": 1,
    "token_issued": 1,
    "expired_date": "2027-01-01T00:00:00.000000+00:00",
    "lastest_api_version": "1.0.2",
    "server_time": "2026-02-10T22:35:00.000000-05:00"
}
```

#### 2. 失败 - Token 缺失或错误 (Identity Mismatch)

```json
{
    "status": "fail",
    "message": "Missing Customer Token",
    "license_status": null,
    "device_status": null,
    "token_issued": 0,
    "expired_date": null,
    "lastest_api_version": "1.0.2",
    "server_time": "2026-02-10T22:35:00.000000-05:00"
}
```

```json
{
    "status": "fail",
    "message": "Identity mismatch",
    "license_status": null, 
    "device_status": null,
    "token_issued": 0,
    "expired_date": null,
    "lastest_api_version": "1.0.2",
    "server_time": "2026-02-10T22:35:00.000000-05:00"
}
```

#### 3. 失败 - 授权过期 (License Expired)

```json
{
    "status": "fail",
    "message": "License expired",
    "license_status": 2,
    "device_status": 0,
    "token_issued": 0,
    "expired_date": "2025-01-01T00:00:00.000000+00:00",
    "lastest_api_version": "1.0.2",
    "server_time": "2026-02-10T22:35:00.000000-05:00"
}
```

## 2. 设备激活 (Activate)

新设备首次连接或重新绑定授权码时使用。

**接口地址**: `/api/activate`
**请求方式**: `POST`

### 请求头 (Headers)

| 参数名 | 必选 | 类型 | 说明 |
| :--- | :--- | :--- | :--- |
| `X-Customer-Token` | 是 | `string` | 客户的 API Token (Customer API Key)，用于身份验证 |

### 请求参数 (Body)

| 参数名 | 必选 | 类型 | 说明 |
| :--- | :--- | :--- | :--- |
| `activation_code` | 是 | `string` | 激活码 (用于查找授权) |
| `device_fingerprint` | 是 | `string` | 设备唯一指纹 |
| `model` | 否 | `string` | 设备型号 (如 "iPhone 13") |
| `os_version` | 否 | `string` | 操作系统版本 (如 "iOS 15.0") |
| `app_version` | 否 | `string` | 应用版本号 (如 "1.0.0") |
| `meta_info` | 否 | `object` | 其他元数据 (JSON对象) |

**请求示例**:

```json
{
  "activation_code": "ACTIVATION-CODE-1234",
  "device_fingerprint": "a1b2c3d4e5",
  "model": "MyDevice",
  "os_version": "1.0",
  "app_version": "1.0",
  "meta_info": {}
}
```

### 响应参数 (Response)

| 参数名 | 类型 | 说明 |
| :--- | :--- | :--- |
| `status` | `string` | 请求状态: `ok` 或 `fail` |
| `message` | `string` | 状态说明 |
| `license_id` | `string` | 激活成功后返回关联的 License ID |
| `license_status` | `int` | 授权状态码 |
| `server_time` | `datetime` | 服务器当前时间 |
| `lastest_api_version`| `string` | 最新 API 版本 |
| `expired_date` | `datetime` | 授权过期时间 |

### 响应示例

#### 1. 成功 (Success)

```json
{
    "status": "ok",
    "message": "Device activated",
    "license_id": "LIC-12345678",
    "license_status": 1,
    "server_time": "2026-02-10T22:35:00.000000-05:00",
    "lastest_api_version": "1.0.2",
    "expired_date": "2027-01-01T00:00:00.000000+00:00"
}
```

#### 2. 失败 (Failures)

```json
{
    "status": "fail",
    "message": "Missing Customer Token",
    "server_time": "2026-02-10T22:35:00.000000-05:00",
    "lastest_api_version": "1.0.2"
}
```

```json
{
    "status": "fail",
    "message": "Invalid activation code",
    "server_time": "2026-02-10T22:35:00.000000-05:00",
    "lastest_api_version": "1.0.2"
}
```
