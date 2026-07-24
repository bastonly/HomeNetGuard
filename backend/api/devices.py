from flask import Blueprint, request, jsonify
from utils.pc_interface import PCInterface
from utils.db import query, query_one, execute
from utils.response import success, fail, handle_errors
from utils.redis_client import rc
import time


# 内存缓存
_devices_cache = {} # key: func_name, value: (data, timestamp)
CACHE_TTL = 10 # 缓存 10 秒

bp = Blueprint('devices', __name__)

# --- 工具函数：获取组名 ---
def get_group_name(group_id):
    row = query_one("SELECT name FROM groups WHERE id = ?", (group_id,))
    return row['name'] if row else None

# 获取设备列表（基础）
@bp.route('', methods=['GET'])
@handle_errors
def list_devices():
    # 1. 优先从聚合缓存获取
    cached_devices = rc.get("pc:devices")
    if cached_devices:
        try:
            rows = json.loads(cached_devices)
            # 叠加在线状态
            online_macs = rc.smembers("pc:online_macs")
            for r in rows:
                r['status'] = 'online' if r['mac'] in online_macs else 'offline'
            return jsonify(rows)
        except:
            pass

    # 2. 降级：从数据库重建缓存
    rows = query("""
        SELECT id, name, mac, remark, status, last_seen, ip, groups_id, type, hotpot
        FROM devices
        ORDER BY last_seen DESC
    """)
    
    # 同步新聚合缓存
    try:
        rc.set("pc:devices", json.dumps(rows, ensure_ascii=False))
    except:
        pass

    # 叠加实时在线状态
    online_macs = rc.smembers("pc:online_macs")
    for r in rows:
        r['status'] = 'online' if r['mac'] in online_macs else 'offline'
        
    return jsonify(rows)


# 获取设备列表（带组名详情）
@bp.route('/list', methods=['GET'])
@handle_errors
def get_devices_list():
    # 1. 获取所有分组信息（用于关联名称）
    groups = {str(g['id']): g for g in query("SELECT id, name, remark FROM groups")}
    
    rows = []
    # 2. 优先从聚合缓存获取
    cached_devices = rc.get("pc:devices")
    if cached_devices:
        try:
            rows = json.loads(cached_devices)
        except:
            pass

    if not rows:
        # 降级：从数据库读取
        rows = query("""
            SELECT d.id, d.name, d.mac, d.remark, d.status, d.last_seen, d.type, d.hotpot,
                   d.ip, d.groups_id
            FROM devices d
            ORDER BY d.id DESC
        """)
        # 同步回聚合缓存
        try:
            rc.set("pc:devices", json.dumps(rows, ensure_ascii=False))
        except:
            pass

    # 3. 关联分组详情并叠加在线状态
    online_macs = rc.smembers("pc:online_macs")
    for d in rows:
        gid = str(d.get('groups_id', ''))
        group = groups.get(gid, {})
        d['group_name'] = group.get('name')
        d['group_remark'] = group.get('remark')
        d['status'] = 'online' if d['mac'] in online_macs else 'offline'
        
    # 按 ID 降序排序
    rows.sort(key=lambda x: int(x.get('id', 0)), reverse=True)
    return jsonify(rows)


# 新增设备
@bp.route('', methods=['POST'])
@handle_errors
def add_device():
    data = request.json or {}
    name = data.get('name', '').strip()
    mac = data.get('mac', '').strip().lower()
    remark = data.get('remark', '')
    groups_id = data.get('groups_id')

    if not mac:
        return fail('api.mac_required')

    # 检查设备是否已存在
    if query_one("SELECT id FROM devices WHERE mac = ?", (mac,)):
        return fail('api.mac_exists', 409)

    # 检查并获取组名
    group_name = get_group_name(groups_id)
    if not group_name:
        return fail('api.group_not_found')

    # 1. 写入数据库
    new_id = execute("""
        INSERT INTO devices (name, mac, remark, status, last_seen, ip, groups_id)
        VALUES (?, ?, ?, 'offline', strftime('%s','now'), '', ?)
    """, (name, mac, remark, groups_id))

    # 2. 清除聚合缓存，下次 list 时会自动重建
    rc.delete("pc:devices")


    # 2. 同步到防火墙集合
    # C++ NftManager.add_to_group
    PCInterface.NFT.add_to_group(group_name, mac)
    # 同步到 AdGuard
    PCInterface.AGH.sync()
    
    return success(msg='api.device_add_success')

# 更新设备
@bp.route('/<int:id>', methods=['PUT'])
@handle_errors
def update_device(id):
    data = request.json or {}
    name = data.get('name', '').strip()
    groups_id = data.get('groups_id')
    type = data.get('type', '')
    hotpot = data.get('hotpot', '')

    if not name:
        return fail('api.device_name_required')
    
    # 获取旧设备信息
    old_device = query_one("SELECT mac, groups_id, hotpot FROM devices WHERE id = ?", (id,))
    if not old_device:
        return fail('api.device_not_found', 404)

    # 分组变更逻辑
    if int(groups_id) != old_device['groups_id']:
        new_group_name = get_group_name(groups_id)
        if not new_group_name:
            return fail('api.group_not_found')

        # A. 从所有旧的 nft 集合中彻底清理该 MAC (Legacy: remove_from_all_groups)
        # 现在的 C++ 逻辑 add_to_group 会自动处理组变更 (add to new, auto remove from others?)
        # 查阅 C++ convertion: `add_to_group` just adds. 
        # But wait, `add_to_group` CLI calls `NftManager::add_to_group`, which calls `db.add_device_to_group`? 
        # No, CLI calls `NftManager::add_to_group`.
        # In Python NftManager.add_to_group: it just runs `nft add element ...`
        # In Python NftManager.remove_from_all_groups: it scans all sets and deletes.
        # Our PCInterface only has `add_to_group`. 
        # We might need `sync()` to fully reset based on DB.
        
        # B. 更新数据库
        execute("""
            UPDATE devices 
            SET name = ?, groups_id = ?, type = ?, hotpot = ?
            WHERE id = ?
        """, (name, groups_id, type, hotpot, id))
        
        # C. ！！！关键点：先清除缓存再触发同步，确保 C++ 同步时读取到最新 DB 数据 ！！！
        rc.delete("pc:devices")
        
        # D. 触发一次全量同步 (最稳健)
        PCInterface.NFT.sync()

        # E. 同步Adguard服务器配置
        # C++ AdguardManager syncs based on DB state or fresh Redis.
        PCInterface.AGH.sync()

    else:
        # 仅修改名称或备注
        execute("UPDATE devices SET name = ?, type = ?, hotpot = ? WHERE id = ?", (name, type, hotpot, id))

    # 更新 Redis 缓存 (针对非分组变更)
    rc.delete("pc:devices")


    # 检查 hotpot 是否发生了变化（或者简单起见，只要有更新就同步一次）
    hotpot_changed = (int(hotpot) != old_device['hotpot'])
    if hotpot_changed:
        # Hotspot logic is also covered by full sync
        PCInterface.NFT.sync()

    return success(msg='api.device_update_success')

# 删除设备
@bp.route('/<int:id>', methods=['DELETE'])
@handle_errors
def delete_device(id):
    # 先获取设备 MAC
    device = query_one("SELECT mac FROM devices WHERE id = ?", (id,))
    if not device:
        return fail('api.device_not_found', 404)
        
    # 1. 之前逻辑：先删数据库和缓存，再同步
    execute("DELETE FROM devices WHERE id = ?", (id,))
    
    # 2. 从 Redis 删除缓存
    mac = device['mac']
    rc.delete("pc:devices")
    rc.srem("pc:online_macs", mac)

    
    # 2. 全量同步防火墙 (自动移除不存在设备的规则)
    PCInterface.NFT.sync()
    # 同步到 AdGuard
    PCInterface.AGH.sync()
    
    return success(msg='api.device_delete_success')