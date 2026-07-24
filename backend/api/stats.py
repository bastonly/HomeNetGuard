from flask import Blueprint, jsonify, request
from datetime import datetime, date, timedelta
from flask_jwt_extended import jwt_required
from utils.adguard_client import AdGuardClient
from utils.pc_interface import PCInterface
from utils.db import query
from utils.redis_client import rc
import tldextract
import json

bp = Blueprint('stats', __name__)
agh = AdGuardClient()

# from backend.services.adguard import agh # 假设你的AGH工具类在这里

def _get_unified_summary(agh_data):
    """统一从 AGH 数据中提取并使用 Redis 补偿 summary 的逻辑"""
    summary = agh_data.get('summary', {"total": 0, "blocked": 0})
    
    # 合并 Redis 实时统计 (今日)
    today = date.today().isoformat()
    redis_key = f"pc:stats:date:{today}"
    redis_stats = rc.hgetall(redis_key)
    
    if redis_stats:
        total = int(redis_stats.get("total", 0))
        blocked = int(redis_stats.get("blocked", 0))
        # 只有当 Redis 数据更实时（大）时才更新
        if total > summary.get('total', 0):
             summary = {"total": total, "blocked": blocked}
    return summary, bool(redis_stats)

@bp.get('/')
@bp.get('/overview')
def get_overview():
    """获取 AdGuard 仪表盘统计数据"""
    try:
        agh = AdGuardClient()
        data = agh.get_processed_overview()
    except Exception as e:
        # Return empty data instead of 500 or error status
        return jsonify({
            "status": "success",
            "summary": {"total": 0, "blocked": 0},
            "charts": {"traffic_pie": [], "blocked_bar": []},
            "device_ranking": [],
            "blocked_ranking": []
        })
    
    if not data:
        return jsonify({
            "status": "success",
            "summary": {"total": 0, "blocked": 0},
            "charts": {"traffic_pie": [], "blocked_bar": []},
            "device_ranking": [],
            "blocked_ranking": []
        })
    
    summary, used_redis = _get_unified_summary(data)
    
    device_list = data.get('device_ranking') or []
    blocked_list = data.get('blocked_ranking') or []

    return jsonify({
        "summary": summary,
        "charts": {
            "traffic_pie": [
                { "name": d['name'], "value": d['query_count'] } 
                for d in device_list[:10]
            ],
            "blocked_bar": [
                { "name": b['domain'], "value": b['count'] } 
                for b in blocked_list[:10]
            ]
        },
        "device_ranking": device_list[:20],
        "blocked_ranking": blocked_list[:20],
        "source": "redis_sync" if used_redis else "agh_cache"
    })

@bp.get('/home')
def get_home_stats_api():
    """获取首页统计简报 (优先从 C++ 实现的异步缓存读取)"""
    try:
        # 1. 尝试从 Redis 缓存获取 (由 C++ AdGuardManager::refresh_home_stats_cache 维护)
        cache_data = rc.get("pc:stats:home_summary")
        if cache_data:
            try:
                summary = json.loads(cache_data)
                return jsonify({
                    "status": "success", 
                    "stats": {
                        "summary": {
                            "total": summary.get("total", 0),
                            "blocked": summary.get("blocked", 0)
                        },
                        "source": "cache"
                    }
                })
            except Exception as parse_err:
                pass # 回退到旧逻辑

        # 2. 如果无缓存，则执行旧的同步查询逻辑（保底容错）
        agh = AdGuardClient()
        data = agh.get_processed_overview()
        if not data:
            return jsonify({
                "status": "success", 
                "stats": {"summary": {"total": 0, "blocked": 0}}
            })
            
        summary, _ = _get_unified_summary(data)
        
        return jsonify({
            "status": "success", 
            "stats": {
                "summary": summary,
                "source": "live"
            }
        })
    except Exception as e:
        return jsonify({
            "status": "success", 
            "stats": {"summary": {"total": 0, "blocked": 0}}
        })

@bp.get('/agh/status')
def get_agh_status():
    """获取 AdGuard Home 状态 (通过 C++ 服务)"""
    return jsonify(PCInterface.AGH.get_status())

@bp.post('/agh/control')
@jwt_required()
def control_agh():
    """控制 AdGuard Home (启动/恢复/暂停)"""
    data = request.json or {}
    action = data.get('action')
    
    if action == 'start':
        # Try both start and resume to be absolutely sure
        success = PCInterface.AGH.start()
        if success:
            import time
            time.sleep(1) # Wait a bit for the service to actually come up
            PCInterface.AGH.resume()
    elif action == 'resume':
        success = PCInterface.AGH.resume()
    elif action == 'pause':
        duration = data.get('duration', '')
        success = PCInterface.AGH.pause(duration)
    else:
        return jsonify({"ok": False, "msg": "未知操作"}), 400
        
    return jsonify({"ok": success})

@bp.get('/device/<int:device_id>')
def get_device_detail(device_id):
    target_date_str = request.args.get('date', date.today().isoformat())
    time_from = request.args.get('time_from') # HH:MM
    time_to = request.args.get('time_to')     # HH:MM
    
    # 强制补齐前导零以防出现 "10:30" < "9:30" 的致命字符串比对 Bug
    if time_from and len(time_from.split(':')[0]) == 1:
        time_from = "0" + time_from
    if time_to and len(time_to.split(':')[0]) == 1:
        time_to = "0" + time_to
        
    today_str = date.today().isoformat()
    
    # 1. 查询设备及分组信息
    sql = """
        SELECT d.id, d.name, d.ip, d.mac, d.type, g.name as group_key, g.remark as group_name 
        FROM devices d 
        LEFT JOIN groups g ON d.groups_id = g.id 
        WHERE d.id = ?
    """
    res = query(sql, (device_id,))
    device = res[0] if res else None
    
    if not device:
        return jsonify({"code": 404, "msg": "设备不存在"}), 404

    logs = []
    summary = {"total": 0, "blocked": 0}
    source = "local_db"

    # 2. 获取日志数据
    if target_date_str == today_str:
        # A. 聚合今日数据 (Redis 实时 + DB 已入库)
        source = "redis_live"
        detail_key = f"pc:stats:detail:{today_str}:device:{device_id}"
        
        # 1. 从 Redis 读取实时明细
        redis_data = rc.hgetall(detail_key)
        if redis_data:
            # 临时存放解析后的聚合块
            # redis_data 格式: { "domain:qtype:is_blocked:count": "5", ... }
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
                b_parts = base.split(':') # domain, qtype, is_blocked
                if len(b_parts) < 3: continue
                
                log_time = val['end'] # "2026-03-04 12:34:56"
                
                # 必须严格限制是当天的请求，因为 redis hgetall 可能存留了跨日聚合脏数据
                if not log_time.startswith(today_str):
                    continue
                    
                logs.append({
                    "time": log_time,
                    "domain": b_parts[0],
                    "status": "Blocked" if b_parts[2] == '1' else "Allowed",
                    "is_blocked": 1 if b_parts[2] == '1' else 0,
                    "query_type": b_parts[1],
                    "count": val['count']
                })

        # 2. 从 DB 读取今日已入库补充数据 (解决同步间隙)
        db_today = query(f"""
            SELECT end_at as time, domain, is_blocked, query_type, count
            FROM adguard_device_logs 
            WHERE device_id = ? AND access_date = ?
            ORDER BY end_at DESC
        """, [device_id, today_str])

        for log in db_today:
            is_blocked = 1 if log['is_blocked'] else 0
            logs.append({
                "time": log['time'],
                "domain": log['domain'],
                "status": "Blocked" if is_blocked else "Allowed",
                "is_blocked": is_blocked,
                "query_type": log['query_type'],
                "count": log['count']
            })

        # 3. 统计汇总 (计算出当日绝对总量 19443，不随查询改变)
        logs.sort(key=lambda x: x['time'], reverse=True)
        summary['total'] = sum(l['count'] for l in logs)
        summary['blocked'] = sum(l['count'] for l in logs if l['is_blocked'])

        if time_from or time_to:
            f_logs = []
            for l in logs:
                # 兼容 DB 的空格模式与 Redis 原生 AGH 的 'T' 模式
                curr_t = l['time'].replace('T', ' ').split(' ')[1][:5]
                if time_from and curr_t < time_from: continue
                if time_to and curr_t > time_to: continue
                f_logs.append(l)
            logs = f_logs
    else:
        # --- 历史数据 (DB) ---
        source = "local_db"
        # 构造 SQL 过滤条件
        params = [device_id, target_date_str]

        # 从 adguard_device_logs 表读取当日全量历史
        history_logs = query(f"""
            SELECT end_at as time, domain, is_blocked, query_type, count
            FROM adguard_device_logs 
            WHERE device_id = ? AND access_date = ?
            ORDER BY end_at DESC
        """, params)
        
        # 1. 首先计算该日历史数据的绝对固定总量 (不随查询改变)
        summary['total'] = sum(l['count'] for l in history_logs)
        summary['blocked'] = sum(l['count'] for l in history_logs if l['is_blocked'])

        # 2. 时间段过滤
        if time_from or time_to:
            f_logs = []
            for l in history_logs:
                # 兼容可能存在的格式差异
                curr_t = l['time'].replace('T', ' ').split(' ')[1][:5]
                if time_from and curr_t < time_from: continue
                if time_to and curr_t > time_to: continue
                f_logs.append(l)
            history_logs = f_logs

        for log in history_logs:
            is_blocked = 1 if log['is_blocked'] else 0
            logs.append({
                "time": log['time'],
                "domain": log['domain'],
                "status": "Blocked" if is_blocked else "Allowed",
                "is_blocked": is_blocked,
                "query_type": log['query_type'],
                "count": log['count']
            })

    date_sql = """
        SELECT DISTINCT access_date as day 
        FROM adguard_device_logs 
        WHERE device_id = ?
        ORDER BY day DESC
    """
    date_res = query(date_sql, (device_id,))
    
    # 提取日期列表
    available_dates = [d['day'] for d in date_res]
    
    # 确保“今天”始终在列表里（即使今天还没产生日志）
    if today_str not in available_dates:
        available_dates.insert(0, today_str)

    # 再次排序并确保只取前 7 个（以防万一数据库返回更多）
    available_dates.sort(reverse=True)
    available_dates = available_dates[:7]

    # 4. 判断是否为查询模式（如果是查询，则不设 500 条上限）
    is_query = bool(time_from or time_to)

    # 5. 组装返回
    return jsonify({
        "info": {
            "id": device['id'],
            "name": device['name'],
            "ip": device['ip'],
            "mac": device['mac'],
            "type": device['type'],
            "group_key": device['group_key'],
            "group_name": device['group_name'] or "未分配"
        },
        "summary": {
            "total": summary['total'],
            "blocked": summary['blocked']
        },
        "logs": logs if is_query else logs[:500],
        "source": source,
        "available_dates": available_dates,
        "is_query": is_query
    })

@bp.get('/sync/all')
def push_all():
    success, msg = agh.sync_all_groups()
    if success:
        return jsonify({"status": "success", "msg": msg})
    else:
        return jsonify({"status": "failed", "msg": msg})

@bp.post('/sync/group/<int:group_id>')
def push_group(group_id):
    return jsonify({"status": "success" if agh.sync_single_group(group_id) else "failed"})

@bp.get('/fetch-remote')
@jwt_required()
def pull_config():
    return jsonify({"status": "success" if agh.fetch_remote_to_local() else "failed"})