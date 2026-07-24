#!/bin/sh

# ==============================================================================
# HomeNetGuard 系统自动升级与静默部署脚本 v1.2
# 功能: 全自动下载、SHA256 校验、快照备份、进程清理及原子化部署
# ==============================================================================

# --- 配置区 ---
PC_ROOT="/opt/parent-control"
BACKUP_ROOT="/opt/parent-control_backups"
TMP_DIR="/tmp/pc_upgrade_workspace"
LOG_FILE="/tmp/pc_upgrade_$(date +%Y%m%d).log"
INIT_SCRIPT="/etc/init.d/pc_service"

# 默认下载源 (可由外部环境变量或参数覆盖)
UPDATE_URL=${1:-"https://verify.bastonly.top/download/update.tar.gz"}
SHA_URL="${UPDATE_URL}.sha256"

# 日志颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log() {
    echo -e "[$(date +'%H:%M:%S')] $1" | tee -a "$LOG_FILE"
}

success() {
    log "${GREEN}SUCCESS: $1${NC}"
}

warn() {
    log "${YELLOW}WARN: $1${NC}"
}

error() {
    log "${RED}ERROR: $1${NC}"
}

# --- 数据库审计日志函数 ---
log_to_db() {
    local level=$1    # info, warning, error
    local tag=$2      # UPGRADE_START/SUCCESS/FAILED
    local content=$3  # i18n key
    local detail=$4   # JSON payload
    local db_path="${PC_ROOT}/backend/data/pc.db"
    if [ ! -f "$db_path" ]; then return; fi
    local now=$(date +'%Y-%m-%d %H:%M:%S')
    local safe_content=$(echo "$content" | sed "s/'/''/g")
    local safe_detail=$(echo "$detail" | sed "s/'/''/g")
    sqlite3 "$db_path" <<EOF
INSERT INTO sys_messages (category, level, event_tag, source_ip, source_mac, source_user, content, payload, is_read, created_at, updated_at)
VALUES ('security', '$level', '$tag', '127.0.0.1', 'SYSTEM', 'SYSTEM', '$safe_content', '$safe_detail', 0, '$now', '$now');
EOF
}

log "==== 启动 HomeNetGuard 自动化全流程升级 [v1.2] ===="
log_to_db "info" "UPGRADE_START" "cpp.UPGRADE_START" "{\"url\":\"$UPDATE_URL\"}"

# --- 1. 准备工作环境 ---
rm -rf "$TMP_DIR" && mkdir -p "$TMP_DIR"
cd "$TMP_DIR"

# --- 2. 自动化下载 ---
log "正在下载下载升级包: ${UPDATE_URL}"
# 使用 wget (OpenWrt 常用) 或 curl
DOWNLOAD_OK=0
if command -v wget >/dev/null; then
    wget -q --no-check-certificate "$UPDATE_URL" -O update.tar.gz && \
    wget -q --no-check-certificate "$SHA_URL" -O update.tar.gz.sha256 2>/dev/null
    DOWNLOAD_OK=$?
else
    curl -skL "$UPDATE_URL" -o update.tar.gz && \
    curl -skL "$SHA_URL" -o update.tar.gz.sha256 2>/dev/null
    DOWNLOAD_OK=$?
fi

if [ $DOWNLOAD_OK -ne 0 ] || [ ! -s "update.tar.gz" ]; then
    error "下载失败或文件为空 (Exit Code: $DOWNLOAD_OK)。升级终止。"
    log_to_db "error" "UPGRADE_FAILED" "cpp.UPGRADE_DOWNLOAD_FAILED" "{\"code\":$DOWNLOAD_OK}"
    exit 1
fi
success "文件下载指令执行完毕，进入校验环节。"

# --- 3. 哈希 integrity 校验 ---
if [ -f "update.tar.gz.sha256" ]; then
    log "正在校验文件完整性..."
    EXPECTED_SHA=$(awk '{print $1}' update.tar.gz.sha256)
    ACTUAL_SHA=$(sha256sum update.tar.gz | awk '{print $1}')
    
    if [ "$EXPECTED_SHA" != "$ACTUAL_SHA" ]; then
        error "哈希校验失败！包可能已损坏或源端不匹配。"
        log "  预期: $EXPECTED_SHA"
        log "  实际: $ACTUAL_SHA"
        log_to_db "error" "UPGRADE_FAILED" "cpp.UPGRADE_HASH_FAILED" "{\"expected\":\"$EXPECTED_SHA\", \"actual\":\"$ACTUAL_SHA\"}"
        exit 3
    fi
    success "包完整性校验通过。"
else
    warn "服务器未提供校验文件，跳过哈希检查。"
fi

# --- 4. 检查磁盘空间 ---
log "检查目标空间..."
FREE_SPACE=$(df -m /opt | awk 'NR==2 {print $4}')
if [ "$FREE_SPACE" -lt 100 ]; then
    warn "剩余空间较低 ($FREE_SPACE MB)，继续执行但请关注后续日志。"
fi

# --- 5. 执行全量快照备份 ---
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
BACKUP_DIR="${BACKUP_ROOT}/auto_backup_${TIMESTAMP}"
mkdir -p "$BACKUP_DIR"

log "正在备份当前版本至: $BACKUP_DIR"
cp -a "$PC_ROOT" "$BACKUP_DIR/"
[ -f "$INIT_SCRIPT" ] && cp "$INIT_SCRIPT" "${BACKUP_DIR}/pc_service.init.bak"
success "快照备份完成。"

# --- 6. 停止并强制清理进程 (解决 I/O 冲突的核心) ---
log "正在强制终止旧实例并释放锁定..."
[ -f "$INIT_SCRIPT" ] && "$INIT_SCRIPT" stop 2>/dev/null
killall -9 python3 pc_service 2>/dev/null
rm -f /var/lock/pc_service.lock /var/lock/procd_pc_service.lock
sleep 2

# --- 7. 执行针对性部署覆盖 ---
log "解压新版本至工作区..."
tar -xzf update.tar.gz -C "$TMP_DIR"

# A. 处理预定义删除 (针对冗余或已移除的文件)
if [ -f "delete_list.txt" ]; then
    log "执行增量删除逻辑 (delete_list.txt)..."
    while IFS= read -r file_to_delete || [ -n "$file_to_delete" ]; do
        # 跳过空行和注释
        [ -z "$file_to_delete" ] || echo "$file_to_delete" | grep -q "^#" && continue
        
        target_path="${PC_ROOT}/${file_to_delete}"
        if [ -e "$target_path" ]; then
            log "  正在删除: $file_to_delete"
            rm -rf "$target_path"
        fi
    done < "delete_list.txt"
fi

# B. 部署前端资源 (如果是全量则覆盖，如果是增量则合并)
if [ -d "./frontend" ]; then
    log "正在部署/更新前端资源..."
    # 策略：如果不包含 delete_list.txt 则假定是全量，先清空；
    # 否则假定是增量，直接覆盖/新增。
    if [ ! -f "delete_list.txt" ]; then
        rm -rf "${PC_ROOT}/frontend"
    fi
    cp -a "./frontend" "${PC_ROOT}/"
fi

# C. 部署后端代码
if [ -d "./backend" ]; then
    log "正在部署/更新后端代码 (排除数据库与本地配置)..."
    for folder in $(ls "./backend"); do
        if [ "$folder" = "data" ] || [ "$folder" = "config" ]; then
            log "  跳过保存目录: $folder"
        else
            # 增量逻辑：除非明确在 delete_list.txt 否则不主动删除
            cp -a "./backend/$folder" "${PC_ROOT}/backend/"
        fi
    done
    cp "./backend"/*.py "${PC_ROOT}/backend/" 2>/dev/null
fi

# D. 更新二执行文件与启动脚本
if [ -f "./service/pc_service" ]; then
    log "更新核心二进制服务..."
    cp -a "./service/pc_service" "${PC_ROOT}/service/"
    chmod +x "${PC_ROOT}/service/pc_service"
fi

if [ -f "./init/pc_service" ]; then
    log "更新系统启动脚本..."
    cp "./init/pc_service" "$INIT_SCRIPT"
    chmod +x "$INIT_SCRIPT"
fi

# E. 执行自定义后置脚本 (如数据库迁移、特殊权限设置)
if [ -f "post_upgrade.sh" ]; then
    log "执行自定义后置脚本 (post_upgrade.sh)..."
    chmod +x post_upgrade.sh
    ./post_upgrade.sh "$PC_ROOT" | tee -a "$LOG_FILE"
    success "后置脚本执行完毕。"
fi

# --- 8. 启动与验证 ---
log "重启服务中..."
[ -f "$INIT_SCRIPT" ] && "$INIT_SCRIPT" start

log "执行自检 Check-in..."
sleep 5
CHECK_OUT=$("${PC_ROOT}/service/pc_service" --checkin 2>&1)
if echo "$CHECK_OUT" | grep -qi "success\|ok"; then
    success "系统升级自检通过。"
    log "==== 升级圆满完成！(版本追踪由系统信息自动更新) ===="
    log_to_db "info" "UPGRADE_SUCCESS" "cpp.UPGRADE_SUCCESS" "{}"
    rm -rf "$TMP_DIR"
    exit 0
else
    error "自检异常: $CHECK_OUT"
    log_to_db "warning" "UPGRADE_PARTIAL" "cpp.UPGRADE_CHECKIN_FAILED" "{}"
    exit 5
fi
