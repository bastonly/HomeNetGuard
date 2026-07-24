# Implementation Plan - License Status & Grace Period Improvements

The goal is to provide better visibility into license status and why a system has entered a "Grace Period".

## Proposed Changes

### [Component] C++ Service (Backend)

We will introduce a formal way to track "why" a grace period was started and ensure this is logged for auditing.

#### [MODIFY] [license_manager.hpp](file:///Users/bastonly/Desktop/Source/client/service/include/license_manager.hpp)
- Define `enum class GraceReason` (NONE, INACTIVE_STATUS, CONNECTION_FAILED, TIMEOUT, UNKNOWN).
- Add helper to get string representation of reasons (in Chinese).

#### [MODIFY] [license_manager.cpp](file:///Users/bastonly/Desktop/Source/client/service/src/license_manager.cpp)
- **Status 4 Mapping**: In `checkin()`, enhance the log message when `license_status` is 4 to be "已暂停/待恢复" (Suspended).
- **Grace Period Triggers**:
    - Update logic to identify the specific reason (Server status non-active vs Network error).
    - Store `GRACE_REASON` string in `base.conf` (JSON).
    - Call `log_security_event` with tag `GRACE_PERIOD_TRIGGERED` whenever grace period starts.
    - Specifically handle "invalid server address" (connection code 0 or host not found) and log it as "无法连接服务器".
    - Logging: "授权状态异常 (%s)，启动宽限期" or "无法连接服务器，进入离线宽限期".


---

### [Component] Vue Frontend

#### [MODIFY] [SideMenu.vue](file:///Users/bastonly/Desktop/Source/client/frontend/src/layout/SideMenu.vue)
- Update `updateStatusUI` to extract `GRACE_REASON` from `serverData`.
- Map the reason codes/strings to user-friendly Chinese messages.
- Add an `<el-tooltip>` with a `?` icon near the "宽限期 X 天" text.

## Verification Plan

### Automated/Local Tests
1. **Log Check**: Trigger a network error (e.g. by setting an invalid server URL) and check if `log_security_event` writes to the `sys_messages` table.
   - Command: `sqlite3 /var/lib/pc_service/pc_service.db "SELECT * FROM sys_messages ORDER BY id DESC LIMIT 1;"` (Adjust path if needed).
2. **Config Check**: Verify `base.conf` (or wherever `GRACE_START_TIME` is) now has a `GRACE_REASON` field.

### Manual Verification
1. **Frontend UI**:
   - Manually set a grace period in the config file.
   - Observe `SideMenu.vue` for the new `?` icon and verify the reason tooltip appears on hover.
2. **Status 4 Logging**:
   - (If possible) simulate a server response with license status 4 and check the log output in `pc_service.log`.
