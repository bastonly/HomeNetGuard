# Walkthrough - License Status & Grace Period Improvements

We have implemented better status mapping and auditing for license check-ins and grace periods.

## Changes Made

### 1. Backend (C++) - Status Mapping & Auditing
- Added `GraceReason` enum and helpers in [license_manager.hpp](file:///Users/bastonly/Desktop/Source/client/service/include/license_manager.hpp) and [license_manager.cpp](file:///Users/bastonly/Desktop/Source/client/service/src/license_manager.cpp).
- **Status 4** is now correctly mapped to **"已暂停/待恢复"** in logs.
- **Grace Period Triggers**:
    - When the system enters a grace period (due to network failure or inactive license status), a security audit log is now recorded with the tag `GRACE_PERIOD_TRIGGERED`.
    - Specific reasons such as **"无法连接服务器"** (Connection Failed) or **"授权状态异常"** are captured.
- The reason is stored in `base.conf` as `GRACE_REASON`.

### 2. Frontend (Vue) - Service Status UI
- Updated [SideMenu.vue](file:///Users/bastonly/Desktop/Source/client/frontend/src/layout/SideMenu.vue) to display the grace period reason.
- Added a `?` icon next to the "宽限期 X 天" text.
- Hovering over the icon (or touching on mobile) reveals the specific trigger reason (e.g., "原因: 无法连接服务器").

## How to Verify

### Backend Audit Logs
You can check the audit logs in the Security Audit page or directly via SQL:
```sql
SELECT event_tag, content, payload FROM sys_messages WHERE event_tag = 'GRACE_PERIOD_TRIGGERED' ORDER BY id DESC LIMIT 1;
```

### Configuration Verification
Check if `base.conf` contains the reason:
```bash
grep "GRACE_REASON" /etc/pc_service/base.conf
```
(Note: Actual path may vary depending on deployment).

### Frontend UI
1. Navigate to the sidebar.
2. If the system is in a grace period, you will see the `?` icon.
3. Hover/Touch to see the reason.
