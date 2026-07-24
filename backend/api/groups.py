from backend.utils.pc_interface import PCInterface
from flask import Blueprint, request, jsonify
from utils.db import query, query_one, execute
from utils.response import success, fail, handle_errors
import time

# 内存缓存
_status_cache = None
_status_cache_time = 0
CACHE_TTL = 5 # 缓存 5 秒

# 定义蓝图，建议前缀为 /api/groups
bp = Blueprint('groups', __name__)

@bp.route('', methods=['GET'])
@handle_errors
def list_groups():
    # 1. 尝试从 Redis 读取
    kind = request.args.get("kind", type=int)
    cached_groups = rc.get("pc:groups")
    if cached_groups:
        try:
            groups = json.loads(cached_groups)
            if kind is not None:
                groups = [g for g in groups if g.get('kind') is not None and int(g.get('kind')) == kind]
            return jsonify(groups)
        except:
            pass

    # 2. 降级：从数据库读取并同步到 Redis
    kind = request.args.get("kind", type=int)
    if kind is not None:
        rows = query("SELECT id, name, remark, adguard_client_key, kind, allow_vpn, enable_video_detection, enable_gaming_detection, is_blocked FROM groups WHERE kind = ? ORDER BY id ASC",(kind,))
    else:
        rows = query("SELECT id, name, remark, adguard_client_key, kind, allow_vpn, enable_video_detection, enable_gaming_detection, is_blocked FROM groups ORDER BY id ASC")
        # 仅全量列表才同步主缓存键
        try:
            rc.set("pc:groups", json.dumps(rows, ensure_ascii=False))
        except:
            pass
            
    return jsonify(rows)

from flask_jwt_extended import get_jwt_identity

# 2. 修改分组名称
@bp.route('/<int:id>', methods=['PUT'])
@handle_errors
def update_group_name(id):
    data = request.json or {}
    
    # 1. 检查分组是否存在并获取现有数据
    group = query_one("SELECT * FROM groups WHERE id = ?", (id,))
    if not group:
        return fail('api.group_not_found', 404)

    # 2. 提取字段
    remark = data.get('remark', group['remark'])
    if remark is not None:
        remark = str(remark).strip()
    
    kind = data.get('kind', group['kind'])
    adguard_client_key = data.get('adguard_client_key', group['adguard_client_key'])
    allow_vpn = data.get('allow_vpn', group['allow_vpn'])
    enable_video_detection = data.get('enable_video_detection', group['enable_video_detection'])
    enable_gaming_detection = data.get('enable_gaming_detection', group['enable_gaming_detection'])
    is_blocked = data.get('is_blocked', group['is_blocked'])

    # 审计日志逻辑：如果 is_blocked 状态发生变化，记录操作
    if is_blocked != group['is_blocked']:
        try:
            current_user = get_jwt_identity() or "Unknown"
            # 存入 Key，由前端负责翻译和变量注入
            event_key = "audit.manual_block" if is_blocked == 1 else "audit.manual_unblock"
            
            execute("""
                INSERT INTO sys_messages (category, level, event_tag, source_user, group_name, content, created_at, updated_at)
                VALUES ('security', 'warning', 'BLOCK_MANUAL', ?, ?, ?, datetime('now', 'localtime'), datetime('now', 'localtime'))
            """, (current_user, group['name'], event_key))
        except Exception as e:
            print(f"Failed to log audit event: {e}")

    # 3. 更新字段
    execute("""
        UPDATE groups 
        SET remark = ?, adguard_client_key = ?, kind = ?, allow_vpn = ?, 
            enable_video_detection = ?, enable_gaming_detection = ?, is_blocked = ?
        WHERE id = ?
    """, (remark, adguard_client_key, kind, allow_vpn, enable_video_detection, enable_gaming_detection, is_blocked, id))
    
    # 清除内存缓存和 Redis 缓存
    global _status_cache
    _status_cache = None
    rc.delete("pc:groups_status")
    rc.delete("pc:groups")
    
    # ！！！关键点：先清除缓存再触发同步，确保 C++ 同步时读取到最新 DB 数据 ！！！
    # 修改状态后立即同步防火墙
    PCInterface.NFT.sync()
    # 同时同步 AdGuard 配置
    PCInterface.AGH.sync()
    
    return success(msg='api.group_update_success')
    
from utils.redis_client import rc
import json

@bp.route('/status')
@handle_errors
def get_groups_with_status():
    global _status_cache, _status_cache_time
    start_time = time.time()
    now = start_time
    
    # 1. 尝试从 Redis 读取 (极速)
    try:
        redis_data = rc.get("pc:groups_status")
        if redis_data:
            print(f"Status API: Redis Hit in {time.time() - start_time:.4f}s")
            results = json.loads(redis_data)
            _status_cache = results
            _status_cache_time = now
            return jsonify(results)
    except Exception as e:
        print(f"Status API: Redis Error ({e}) in {time.time() - start_time:.4f}s")
    
    # 2. 如果 Redis 失败，检查 5s 内的内存缓存
    if _status_cache is not None and (now - _status_cache_time) < CACHE_TTL:
        print(f"Status API: Memory Cache Hit in {time.time() - start_time:.4f}s")
        return jsonify(_status_cache)

    # 3. 实在不行，回退到原有逻辑 (调用 C++ CLI)
    try:
        print(f"Status API: Falling back to CLI at {time.time() - start_time:.4f}s")
        results = PCInterface.NFT.get_status()
        if results:
            print(f"Status API: CLI Success in {time.time() - start_time:.4f}s")
            _status_cache = results
            _status_cache_time = now
            try:
                rc.set("pc:groups_status", json.dumps(results), ex=60)
            except:
                pass
            return jsonify(results)
    except Exception as e:
        print(f"Status API: CLI Fallback failed ({e}) at {time.time() - start_time:.4f}s")

    if _status_cache:
        print(f"Status API: Returning Stale Cache at {time.time() - start_time:.4f}s")
        return jsonify(_status_cache)
        
    print(f"Status API: All attempts failed in {time.time() - start_time:.4f}s")
    return jsonify([])
