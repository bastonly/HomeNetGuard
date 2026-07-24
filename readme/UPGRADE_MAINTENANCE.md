# HomeNetGuard 客户端升级维护手册

本文档详细说明客户端 (`OpenWrt/Raspberry Pi`) 如何执行自动更新及日常维护。

## 1. 自动化升级脚本 (`upgrade.sh`)
脚本路径：`/opt/parent-control/service/upgrade.sh`

### 核心功能
1. **自动下载**：从 `https://verify.bastonly.top/download/update.tar.gz` 获取最新包。
2. **安全核检 (SHA256)**：强制执行哈希比对。若文件损坏或不匹配，立即终止升级，确保系统安全。
3. **环境快照**：覆盖前自动备份 `/opt/parent-control` 至备份目录。
4. **数据隔离保护**：**绝对不覆盖** `backend/data/` (数据库) 和 `backend/config/` (配置文件)。
5. **进程冲突清理**：自动清理所有活跃的 Python 和 C++ 进程，彻底解决 I/O 锁定问题。

## 2. 日常维护建议
- **手动触发升级**：可通过命令 `/opt/parent-control/service/upgrade.sh` 手动运行。
- **查看日志**：升级日志保存在 `/tmp/pc_upgrade_YYYYMMDD.log`。
- **手动回滚**：若升级后出现异常，可手动执行：
  `cp -a /opt/parent-control_backups/backup_timestamp/* /opt/parent-control/`
- **清理空间**：定期清理 `/opt/parent-control_backups/` 下的老旧备份，确保磁盘空间充足。

## 3. 常见问题 (Troubleshooting)
- **[Errno 5] I/O error**: 表明存在进程冲突。系统会自动清理，如无效请手动执行 `killall -9 pc_service python3`。
- **Hash Verification Failed**: 说明下载包已损坏，请检查网络或重新发布。
