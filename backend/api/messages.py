from flask import Blueprint, request, jsonify
from utils.db import query, query_one, execute
from utils.response import success, fail, handle_errors
from utils.redis_client import rc
from datetime import datetime, timedelta
import logging

logger = logging.getLogger(__name__)

bp = Blueprint('messages', __name__)

# --- 审计日志相关接口 ---

# 获取审计日志
@bp.route('/security', methods=['GET'])
@handle_errors
def get_security_logs():
    """
    获取安全审计日志
    """
    level = request.args.get('level')
    date_from = request.args.get('date_from')
    date_to = request.args.get('date_to')
    keyword = request.args.get('keyword') # 新增关键字搜索
    
    sql = "SELECT id, category, level, event_tag, source_ip, source_mac, source_user, group_name, content, payload, occurrence_count, is_read, created_at, updated_at FROM sys_messages WHERE category = 'security'"
    params = []
    
    if level:
        sql += " AND level = ?"
        params.append(level)
    if date_from:
        sql += " AND updated_at >= ?"
        params.append(date_from + " 00:00:00")
    if date_to:
        sql += " AND updated_at <= ?"
        params.append(date_to + " 23:59:59")
    if keyword:
        sql += " AND (content LIKE ? OR source_ip LIKE ? OR source_user LIKE ? OR source_mac LIKE ?)"
        pattern = f"%{keyword}%"
        params.extend([pattern, pattern, pattern, pattern])
        
    sql += " ORDER BY updated_at DESC"
    
    rows = query(sql, tuple(params))
    results = [dict(row) for row in rows[:500]] # 限制返回 500 条
    return jsonify(results)

@bp.route('/security/<int:msg_id>/read', methods=['POST'])
@handle_errors
def mark_security_log_read(msg_id):
    """
    标记单条日志为已读
    """
    execute("UPDATE sys_messages SET is_read = 1 WHERE id = ?", (msg_id,))
    return success(msg='Marked as read')

@bp.route('/security/mark-all-read', methods=['POST'], strict_slashes=False)
@handle_errors
def mark_security_logs_read_all():
    """
    标记所有安全日志为已读
    """
    execute("UPDATE sys_messages SET is_read = 1 WHERE category = 'security' AND is_read = 0")
    return success(msg='All marked as read')

@bp.route('/security/unread_count', methods=['GET'])
@handle_errors
def get_unread_count():
    """
    获取未读安全日志数量 (is_read=0)
    """
    row = query_one("SELECT COUNT(*) as count FROM sys_messages WHERE category = 'security' AND is_read = 0")
    count = row['count'] if row else 0
    return success({'count': count})
    
@bp.route('/security/clear', methods=['GET'])
@handle_errors
def clear_security_logs():
    """
    清空所有审计日志
    """
    execute("DELETE FROM sys_messages WHERE category = 'security'")
    return success(msg='Security logs cleared')

@bp.route('/security/<int:msg_id>/details', methods=['GET'])
@handle_errors
def get_message_details(msg_id):
    """
    获取安全日志的详细信息（如关联域名），支持从 DB 和 Redis 合并
    """
    # 1. 获取该消息的原始信息
    msg = query_one("SELECT source_mac, updated_at, event_tag FROM sys_messages WHERE id = ?", (msg_id,))
    if not msg:
        return fail(msg='Message not found')
    
    mac = msg['source_mac']
    event_time_str = msg['updated_at']
    try:
        event_dt = datetime.strptime(event_time_str, '%Y-%m-%d %H:%M:%S')
    except:
        event_dt = datetime.now()
        
    date_str = event_dt.strftime('%Y-%m-%d')
    
    # 2. 查询设备 ID
    device = query_one("SELECT id FROM devices WHERE LOWER(mac) = LOWER(?)", (mac,))
    device_id = device['id'] if device else None

    # 时间窗口 (适当放宽到前后 15 分钟，应对设备时钟偏移)
    start_win = (event_dt - timedelta(minutes=15)).strftime('%Y-%m-%d %H:%M:%S')
    end_win = (event_dt + timedelta(minutes=5)).strftime('%Y-%m-%d %H:%M:%S')

    logger.info(f"Auditing device {device_id} ({mac}) at {event_time_str}. Range: {start_win} ~ {end_win}")

    results = []
    
    # 3. 从 Redis 读取实时数据
    if date_str == datetime.now().strftime('%Y-%m-%d') and device_id:
        detail_key = f"pc:stats:detail:{date_str}:device:{device_id}"
        redis_data = rc.hgetall(detail_key)
        if redis_data:
            agg_map = {}
            for k, v in redis_data.items():
                parts = k.rsplit(':', 1)
                if len(parts) < 2: continue
                base, suffix = parts
                if base not in agg_map: agg_map[base] = {"count": 0, "begin": "", "end": ""}
                if suffix == "count": agg_map[base]["count"] = int(v)
                elif suffix == "begin": agg_map[base]["begin"] = v
                elif suffix == "end": agg_map[base]["end"] = v
            
            for base, val in agg_map.items():
                # 逻辑调整：由于 AdGuard 返回的 Redis 数据包含 'T' 分隔符，而 start_win/end_win 使用空格，
                # 必须标准化后再进行 String 比较，否则 'T' (ASCII 84) > ' ' (ASCII 32) 会导致 begin <= end_win 判定失效。
                b_norm = val['begin'].replace('T', ' ')[:19]
                e_norm = val['end'].replace('T', ' ')[:19]
                
                if e_norm >= start_win and b_norm <= end_win:
                    b_parts = base.split(':') # domain, qtype, is_blocked
                    if len(b_parts) >= 3:
                        results.append({
                            "domain": b_parts[0],
                            "is_blocked": int(b_parts[2]),
                            "access_time": e_norm
                        })

    # 4. 从数据库补充
    sql = """
        SELECT DISTINCT l.domain, l.is_blocked, 
               l.end_at as access_time
        FROM adguard_device_logs l
        JOIN devices d ON l.device_id = d.id
        WHERE LOWER(d.mac) = LOWER(?)
        AND l.end_at >= ?
        AND l.begin_at <= ?
        ORDER BY l.end_at DESC
        LIMIT 100
    """
    db_rows = query(sql, (mac, start_win, end_win))
    
    # 合并（去重）
    seen_domains = {r['domain']: r['access_time'] for r in results}
    for row in db_rows:
        dom = row['domain']
        time = row['access_time'].replace('T', ' ')[:19]
        if dom not in seen_domains:
            results.append({
                "domain": dom,
                "is_blocked": row['is_blocked'],
                "access_time": time
            })
        elif time > seen_domains[dom]:
             # 更新为更晚的时间
             for r in results:
                 if r['domain'] == dom:
                     r['access_time'] = time
                     break

    # 5. 排序输出
    results.sort(key=lambda x: x['access_time'], reverse=True)
    logger.info(f"Audit details found: {len(results)} items within window {start_win} ~ {end_win}")
    return jsonify(results[:100])