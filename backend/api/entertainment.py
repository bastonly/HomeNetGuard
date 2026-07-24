from flask import Blueprint, jsonify, request
from utils.db import query, execute, query_one, executemany
from datetime import datetime, timedelta
from utils.pc_interface import PCInterface
from utils.redis_client import rc
import json
import tldextract
import logging
import pytz
import config

# 日志配置
logger = logging.getLogger("EntertainmentAPI")

bp = Blueprint('entertainment', __name__)

# 移除 EntertainmentDetector 类，检测逻辑已迁移至 C++

@bp.get('/detections')
def get_detections():
    """获取娱乐网站检测报告"""
    page = request.args.get('page', 1, type=int)
    page_size = request.args.get('page_size', 20, type=int)
    offset = (page - 1) * page_size

    # 获取查询参数
    device_id = request.args.get('device_id', type=int)
    category = request.args.get('category')  # 'video' 或 'gaming'
    date_from = request.args.get('date_from')
    date_to = request.args.get('date_to')
    
    # 构建查询
    sql = """
        SELECT 
            ed.id,
            ed.device_id,
            d.name as device_name,
            d.type as device_type,
            g.remark as group_name,
            g.name as group_internal_name,
            g.id as group_id,
            ed.domain,
            ed.category,
            ed.visit_count,
            ed.first_seen,
            ed.last_seen,
            ed.detection_date,
            ec.name as site_name
        FROM entertainment_detections ed
        LEFT JOIN devices d ON ed.device_id = d.id
        LEFT JOIN groups g ON d.groups_id = g.id
        LEFT JOIN entertainment_categories ec ON ed.domain = ec.domain
        WHERE 1=1
    """
    
    # 获取 AdGuard 黑名单确认状态
    blocked_rules = set(PCInterface.AGH.list_rules())
    
    # 先构建过滤条件的 SQL
    filter_sql = ""
    params = []
    
    if device_id:
        filter_sql += " AND ed.device_id = ?"
        params.append(device_id)
    
    if category:
        filter_sql += " AND ed.category = ?"
        params.append(category)
    
    # [优化] 如果没有指定日期，默认显示最近 7 天，确保点击同步后能看到数据
    if date_from:
        filter_sql += " AND ed.detection_date >= ?"
        params.append(date_from)
    elif not date_to:
        tz = config.get_timezone()
        default_from = (datetime.now(tz) - timedelta(days=7)).strftime('%Y-%m-%d')
        filter_sql += " AND ed.detection_date >= ?"
        params.append(default_from)
    
    if date_to:
        filter_sql += " AND ed.detection_date <= ?"
        params.append(date_to)

    # 1. 计算总数
    count_sql = """
        SELECT COUNT(*) as total
        FROM entertainment_detections ed
        WHERE 1=1
    """ + filter_sql
    total_res = query_one(count_sql, tuple(params))
    total = total_res['total'] if total_res else 0

    # 2. 查询分页数据
    sql += filter_sql + " ORDER BY ed.last_seen DESC"
    results = query(sql, tuple(params))
    
    # 注入阻止状态
    for row in results:
        domain = row.get('domain')
        rule_pattern = f"||{domain}^" if domain else ""
        row['is_blocked'] = rule_pattern in blocked_rules

    # 用户要求：data 只取 500 条记录，通过查询不限制条数
    # 为了防止分页器显示 ghost pages，total 也封顶在 500
    return jsonify({"ok": True, "data": results[:500], "total": min(total, 500)})

@bp.post('/detect')
def trigger_detection():
    """手动触发检测：将今日最新的 Redis 统计数据同步入库"""
    try:
        import subprocess
        date_str = datetime.now(config.get_timezone()).strftime('%Y-%m-%d')
        cmd = ["/opt/parent-control/service/pc_service", "--sync-entertainment-redis", date_str]
        
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        
        if result.returncode == 0:
            logger.info(f"Manual entertainment sync successful for {date_str}")
            return jsonify({"ok": True, "msg": "同步完成，数据已入库展示。"})
        else:
            logger.error(f"Manual sync failed: {result.stderr}")
            return jsonify({"ok": False, "msg": "同步失败，请检查服务日志。"}), 500
    except Exception as e:
        logger.error(f"Trigger sync error: {e}")
        return jsonify({"ok": False, "msg": str(e)}), 500

@bp.get('/stats')
def get_stats():
    """获取统计数据"""
    days = request.args.get('days', 7, type=int)
    tz = config.get_timezone()
    date_from = (datetime.now(tz) - timedelta(days=days)).strftime('%Y-%m-%d')
    
    # 按类别统计
    category_stats = query("""
        SELECT category, CAST(COUNT(*) AS INTEGER) as site_count, CAST(SUM(visit_count) AS INTEGER) as total_visits
        FROM entertainment_detections
        WHERE detection_date >= ?
        GROUP BY category
    """, (date_from,))
    
    # 统计活跃设备数
    active_devices_res = query_one("""
        SELECT COUNT(DISTINCT device_id) as device_count
        FROM entertainment_detections
        WHERE detection_date >= ?
    """, (date_from,))
    device_count = active_devices_res['device_count'] if active_devices_res else 0
    
    # 按设备统计
    device_stats = query("""
        SELECT 
            d.id,
            d.name,
            g.remark as group_name,
            g.name as group_internal_name,
            COUNT(DISTINCT ed.domain) as unique_sites,
            SUM(ed.visit_count) as total_visits
        FROM entertainment_detections ed
        LEFT JOIN devices d ON ed.device_id = d.id
        LEFT JOIN groups g ON d.groups_id = g.id
        WHERE ed.detection_date >= ?
        GROUP BY d.id
        ORDER BY total_visits DESC
        LIMIT 10
    """, (date_from,))
    
    # Top 网站
    top_sites = query("""
        SELECT 
            ed.domain,
            ec.name as site_name,
            ed.category,
            COUNT(DISTINCT ed.device_id) as device_count,
            SUM(ed.visit_count) as total_visits
        FROM entertainment_detections ed
        LEFT JOIN entertainment_categories ec ON ed.domain = ec.domain
        WHERE ed.detection_date >= ?
        GROUP BY ed.domain
        ORDER BY total_visits DESC
        LIMIT 10
    """, (date_from,))
    
    return jsonify({
        "ok": True,
        "data": {
            "category_stats": category_stats,
            "device_stats": device_stats,
            "top_sites": top_sites,
            "device_count": device_count,
            "cache_hit": True
        }
    })

@bp.post('/block')
def block_domain():
    """将域名加入 AdGuard 黑名单"""
    data = request.get_json(silent=True) or {}
    domain = data.get('domain')
    if not domain:
        return jsonify({"ok": False, "msg": "域名不能为空"}), 400
    try:
        if PCInterface.AGH.block_domain(domain):
            return jsonify({"ok": True, "msg": f"域名 {domain} 已成功加入黑名单"})
        return jsonify({"ok": False, "msg": "操作失败"}), 500
    except Exception as e:
        return jsonify({"ok": False, "msg": str(e)}), 500

@bp.post('/unblock')
def unblock_domain():
    """将域名从 AdGuard 黑名单移除"""
    data = request.get_json(silent=True) or {}
    domain = data.get('domain')
    if not domain:
        return jsonify({"ok": False, "msg": "域名不能为空"}), 400
    try:
        if PCInterface.AGH.unblock_domain(domain):
            return jsonify({"ok": True, "msg": f"域名 {domain} 已从黑名单解锁"})
        return jsonify({"ok": False, "msg": "操作失败"}), 500
    except Exception as e:
        return jsonify({"ok": False, "msg": str(e)}), 500
