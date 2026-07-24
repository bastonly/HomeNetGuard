from flask import Blueprint, request, jsonify
from utils.db import query, query_one, execute
from utils.response import success, fail, handle_errors
from utils.pc_interface import PCInterface
from flask_jwt_extended import verify_jwt_in_request, get_jwt_identity
import datetime
import time
import backend.config as config

bp = Blueprint('usage', __name__)

def get_client_ip():
    """获取请求客户端真实IP，支持调试模式重写"""
    debug_ip = request.args.get('debug_ip')
    if debug_ip:
        return debug_ip
    
    ip = request.headers.get('X-Forwarded-For', request.remote_addr)
    if ip and ',' in ip:
        ip = ip.split(',')[0].strip()
    return ip

def get_today_info(tz):
    """获取当前日期类型 (holiday / weekend / workday)"""
    now = datetime.datetime.now(tz)
    current_date = now.strftime("%Y-%m-%d")
    
    # 查询是否是节假日
    holiday_exists = query_one(
        "SELECT id, name FROM holidays WHERE ? BETWEEN start_date AND end_date LIMIT 1",
        (current_date,)
    )
    if holiday_exists:
        return 'holiday', current_date, now, holiday_exists['name']
    else:
        day_type = 'weekend' if now.weekday() >= 5 else 'workday'
        return day_type, current_date, now, None

def resolve_group_and_device(ip):
    """
    解析当前请求操作的设备组及设备信息。
    安全规则：
      1. 如果显式传入了 groups_id，代表是家长在管理员面板跨组操作，必须校验 JWT 管理员身份。
      2. 如果没有 groups_id，代表是受限用户基于自身 IP 免密自助操作，基于 IP 自动识别并只允许操作本组。
    """
    # 获取显式参数 (支持 GET 里的 querystring 或 POST 里的 json)
    groups_id = request.args.get('groups_id', type=int)
    if not groups_id and request.is_json:
        groups_id = (request.json or {}).get('groups_id')
        
    if groups_id:
        # 跨组控制：执行 JWT 强校验
        try:
            verify_jwt_in_request()
            current_user = get_jwt_identity()
            if not current_user:
                return None, None, "Invalid admin credentials"
        except Exception as e:
            return None, None, f"Admin token verification failed: {str(e)}"
            
        # 查询该组
        group = query_one("SELECT id, name, remark, kind, is_blocked, holiday_kind FROM groups WHERE id = ?", (groups_id,))
        if not group:
            return None, None, "Target group not found"
        # 家长跨组控制时，设备主体为空 (不绑定特定设备)
        return group, None, None

    else:
        # 免密客户端自助操作：基于 IP 自动绑定
        device = query_one("SELECT id, name, mac, groups_id FROM devices WHERE ip = ?", (ip,))
        if not device:
            return None, None, "Device not registered"
            
        group = query_one("SELECT id, name, remark, kind, is_blocked, holiday_kind FROM groups WHERE id = ?", (device['groups_id'],))
        if not group:
            return None, None, "Device group not found"
            
        return group, device, None

@bp.route('/detect', methods=['GET'])
@handle_errors
def detect_device_usage():
    """根据访问者IP识别或指定 groups_id 探测上网状态、使用时间、剩余时间以及上网策略"""
    ip = get_client_ip()
    
    group, device, err = resolve_group_and_device(ip)
    if err:
        # 若是未登录的非受限设备或家长，返回 is_restricted: false 自动切换到管理端登录
        return jsonify({"is_restricted": False, "msg": err})
        
    # 如果分组属于免严控的类型 (kind = 3 或组名是 open)
    # 则自动切换为非受限，返回 is_restricted: false 引导跳后台
    if group['kind'] == 3 or group['name'].lower() == 'open':
        return jsonify({"is_restricted": False, "group_name": group['name']})

    # 3. 动态计算今日上网耗时及状态
    tz = config.get_timezone()
    day_type, current_date, now, holiday_name = get_today_info(tz)
    current_ts = int(now.timestamp())
    now_m = now.hour * 60 + now.minute
    
    # 默认值
    limit_type = 'period'
    total_duration = 0
    used_duration = 0
    status = 'stopped'
    slots = []
    
    # 如果是节假日
    if day_type == 'holiday':
        limit_type = group.get('holiday_kind') or 'period'
        
        if limit_type == 'duration':
            # 时长控制模式下，查出启用的时长策略
            policy = query_one(
                "SELECT id, duration_limit FROM policies WHERE groups_id = ? AND type = 'holiday' AND limit_type = 'duration' AND is_enabled = 1 LIMIT 1",
                (group['id'],)
            )
            total_duration = (policy['duration_limit'] * 60) if policy else 0
            
            # 计算今日已用时长
            sessions = query(
                "SELECT id, start_time, end_time, duration, trigger_reason "
                "FROM group_internet_sessions "
                "WHERE groups_id = ? AND date = ? "
                "ORDER BY id ASC",
                (group['id'], current_date)
            )
            
            is_running = False
            for s in sessions:
                if s['end_time'] is None:
                    is_running = True
                    used_duration += max(0, current_ts - s['start_time'])
                else:
                    used_duration += s['duration']
                    
            status = 'running' if is_running else 'stopped'
            
        else:
            # 时间段控制模式
            limit_type = 'period'
            db_slots = query(
                "SELECT id, from_at, end_at FROM policies WHERE groups_id = ? AND type = 'holiday' AND limit_type = 'period' AND is_enabled = 1",
                (group['id'],)
            )
            
            in_slot = False
            for slot in db_slots:
                from_at = slot['from_at']
                end_at = slot['end_at']
                
                from_m = int(from_at.split(':')[0]) * 60 + int(from_at.split(':')[1])
                end_m = int(end_at.split(':')[0]) * 60 + int(end_at.split(':')[1])
                duration_sec = (end_m - from_m) * 60
                
                total_duration += duration_sec
                
                if now_m < from_m:
                    segment_used = 0
                elif from_m <= now_m <= end_m:
                    segment_used = (now_m - from_m) * 60 + now.second
                    in_slot = True
                else:
                    segment_used = duration_sec
                    
                used_duration += segment_used
                slots.append({
                    "from_at": from_at,
                    "end_at": end_at,
                    "status": "active" if (from_m <= now_m <= end_m) else ("passed" if now_m > end_m else "scheduled")
                })
                
            if group['is_blocked'] == 1:
                status = 'stopped'
            else:
                status = 'running' if in_slot else 'stopped'
                
    else:
        # 工作日、周末（仅时间段模式）
        limit_type = 'period'
        db_slots = query(
            "SELECT id, from_at, end_at FROM policies WHERE groups_id = ? AND type = ? AND is_enabled = 1",
            (group['id'], day_type)
        )
        
        in_slot = False
        for slot in db_slots:
            from_at = slot['from_at']
            end_at = slot['end_at']
            
            from_m = int(from_at.split(':')[0]) * 60 + int(from_at.split(':')[1])
            end_m = int(end_at.split(':')[0]) * 60 + int(end_at.split(':')[1])
            duration_sec = (end_m - from_m) * 60
            
            total_duration += duration_sec
            
            if now_m < from_m:
                segment_used = 0
            elif from_m <= now_m <= end_m:
                segment_used = (now_m - from_m) * 60 + now.second
                in_slot = True
            else:
                segment_used = duration_sec
                
            used_duration += segment_used
            slots.append({
                "from_at": from_at,
                "end_at": end_at,
                "status": "active" if (from_m <= now_m <= end_m) else ("passed" if now_m > end_m else "scheduled")
            })
            
        if group['is_blocked'] == 1:
            status = 'stopped'
        else:
            status = 'running' if in_slot else 'stopped'

    remaining_duration = max(0, total_duration - used_duration)
    
    # 组装返回数据
    return jsonify({
        "is_restricted": True,
        "device_name": device['name'] if device else "Administrator Panel",
        "mac": device['mac'] if device else "",
        "group_id": group['id'],
        "group_name": group['name'],
        "remark": group['remark'],
        "day_type": day_type,
        "holiday_name": holiday_name,
        "limit_type": limit_type,
        "status": status,
        "total_duration": total_duration,     # 秒
        "used_duration": used_duration,       # 秒
        "remaining_duration": remaining_duration, # 秒
        "is_blocked_manual": group['is_blocked'] == 1,
        "slots": slots
    })

@bp.route('/start', methods=['POST'])
@handle_errors
def start_internet():
    """开始上网 (仅在时长模式下有效)"""
    ip = get_client_ip()
    
    group, _, err = resolve_group_and_device(ip)
    if err:
        return jsonify({"ok": False, "msg": err}), 403
        
    if group['is_blocked'] == 1:
        return jsonify({"ok": False, "msg": "该组已被管理员一键断网，无法自助开启"}), 403

    tz = config.get_timezone()
    day_type, current_date, now, _ = get_today_info(tz)
    current_ts = int(now.timestamp())
    
    if day_type != 'holiday':
        return jsonify({"ok": False, "msg": "当前非节假日，仅支持定时时间段上网"}), 400
        
    if group.get('holiday_kind') != 'duration':
        return jsonify({"ok": False, "msg": "当前策略非时长控制模式，无法手动开启"}), 400
        
    policy = query_one(
        "SELECT duration_limit FROM policies WHERE groups_id = ? AND type = 'holiday' AND limit_type = 'duration' AND is_enabled = 1 LIMIT 1",
        (group['id'],)
    )
    if not policy:
        return jsonify({"ok": False, "msg": "未配置有效的节假日时长策略"}), 400
        
    total_duration = policy['duration_limit'] * 60
    sessions = query(
        "SELECT start_time, end_time, duration FROM group_internet_sessions WHERE groups_id = ? AND date = ?",
        (group['id'], current_date)
    )
    
    used_duration = 0
    active_session = None
    for s in sessions:
        if s['end_time'] is None:
            active_session = s
            used_duration += max(0, current_ts - s['start_time'])
        else:
            used_duration += s['duration']
            
    if used_duration >= total_duration:
        return jsonify({"ok": False, "msg": "今日上网时长已耗尽，无法开启"}), 403

    # 如果已经在上网中，直接返回成功
    if active_session:
        return jsonify({"ok": True, "msg": "网络已是开启状态"})

    # 插入新的上网会话
    execute(
        "INSERT INTO group_internet_sessions (groups_id, date, start_time) VALUES (?, ?, ?)",
        (group['id'], current_date, current_ts)
    )
    
    # 触发防火墙规则同步
    PCInterface.NFT.sync()
    
    return jsonify({"ok": True, "msg": "开始上网成功"})

@bp.route('/stop', methods=['POST'])
@handle_errors
def stop_internet():
    """结束/暂停上网 (仅在时长模式下有效)"""
    ip = get_client_ip()
    
    group, _, err = resolve_group_and_device(ip)
    if err:
        return jsonify({"ok": False, "msg": err}), 403
        
    tz = config.get_timezone()
    day_type, current_date, now, _ = get_today_info(tz)
    current_ts = int(now.timestamp())
    
    # 查询当前组是否有处于未闭合的会话
    active_session = query_one(
        "SELECT id, start_time FROM group_internet_sessions "
        "WHERE groups_id = ? AND date = ? AND end_time IS NULL "
        "ORDER BY id DESC LIMIT 1",
        (group['id'], current_date)
    )
    
    if not active_session:
        return jsonify({"ok": True, "msg": "网络已是暂停状态"})
        
    duration = max(0, current_ts - active_session['start_time'])
    
    # 结算会话
    execute(
        "UPDATE group_internet_sessions "
        "SET end_time = ?, duration = ?, trigger_reason = 'manual' "
        "WHERE id = ?",
        (current_ts, duration, active_session['id'])
    )
    
    # 触发防火墙同步
    PCInterface.NFT.sync()
    
    return jsonify({"ok": True, "msg": "暂停上网成功"})

@bp.route('/history', methods=['GET'])
@handle_errors
def get_usage_history():
    """获取今日上网历史记录"""
    ip = get_client_ip()
    
    group, _, err = resolve_group_and_device(ip)
    if err:
        return jsonify([])
        
    tz = config.get_timezone()
    _, current_date, _, _ = get_today_info(tz)
    
    # 查询今日的历史
    rows = query(
        "SELECT start_time, end_time, duration, trigger_reason "
        "FROM group_internet_sessions "
        "WHERE groups_id = ? AND date = ? "
        "ORDER BY id DESC",
        (group['id'], current_date)
    )
    
    # 转换时间戳格式方便前端展示
    history = []
    for r in rows:
        start_dt = datetime.datetime.fromtimestamp(r['start_time'], tz).strftime("%H:%M:%S")
        end_dt = ""
        duration_desc = ""
        
        if r['end_time']:
            end_dt = datetime.datetime.fromtimestamp(r['end_time'], tz).strftime("%H:%M:%S")
            sec = r['duration']
            if sec < 60:
                duration_desc = f"{sec}秒"
            else:
                duration_desc = f"{sec // 60}分{sec % 60}秒"
        else:
            duration_desc = "上网中"
            
        reason_map = {
            "manual": "手动操作",
            "timeout": "时长耗尽",
            None: "未结束"
        }
        
        history.append({
            "start_time": start_dt,
            "end_time": end_dt,
            "duration": duration_desc,
            "trigger_reason": reason_map.get(r['trigger_reason'], "未知")
        })
        
    return jsonify(history)
