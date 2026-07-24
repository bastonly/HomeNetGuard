# Task: License Status Mapping and Grace Period Audit Logs

## Backend (C++) Changes
- [x] Define `GraceReason` enum and string mappings in `license_manager.hpp`
- [x] Update `license_manager.cpp`:
    - [x] Map License status `4` to a descriptive string in logs
    - [x] Log audit events when grace period is triggered
    - [x] Capture "Invalid server address" and "Network failure" as grace triggers
    - [x] Store specific grace reason in `base.conf`
    - [x] Refactor `checkin()` to handle reasons more cleanly

## Frontend (Vue) Changes
- [x] Update `SideMenu.vue`:
    - [x] Add `GRACE_REASON` to the reactive status logic
    - [x] Implement a `?` icon (tooltip) next to the grace period text
    - [x] Display the human-readable reason on hover/touch

## Verification
- [x] Verify audit log entries in `sys_messages` table
- [x] Verify `base.conf` contains the correct reason
- [x] Verify frontend tooltip display
