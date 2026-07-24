from flask import Blueprint, request, jsonify
from utils.pc_interface import PCInterface
from utils.db import query, query_one, execute
import time

bp = Blueprint('temporary', __name__)

# 默认排除的组名
EXCLUDED_GROUPS = ('open', 'temp')

def is_group_allowed(groups_id):
    row = query_one("SELECT name FROM groups WHERE id = ?", (groups_id,))
    if not row:
        return False, "分组不存在"
    if row['name'] in EXCLUDED_GROUPS:
        return False, f"组 {row['name']} 无需或不支持临时授权"
    return True, row['name']

# 1. 获取列表
@bp.get("")
def list_temporary():
    groups_id = request.args.get("groups_id", type=int)
    if not groups_id:
        return jsonify({"ok": False, "msg": "缺少 groups_id"}), 400

    rows = query("""
        SELECT id, groups_id, from_at, end_at
        FROM temporary
        WHERE groups_id = ?
        ORDER BY id DESC
    """, (groups_id,))
    return jsonify(rows)

# 2. 新增临时授权
@bp.post("")
def create_temporary():
    data = request.get_json(silent=True) or {}
    groups_id = data.get("groups_id")
    from_at = data.get("from_at")
    end_at = data.get("end_at")

    if not groups_id or not from_at or not end_at:
        return jsonify({"ok": False, "msg": "缺少必要参数"}), 400

    allowed, result = is_group_allowed(groups_id)
    if not allowed:
        return jsonify({"ok": False, "msg": result}), 400

    try:
        # 将该组之前的活跃记录标记为过期（设置为当前时间），保留历史记录
        now_ts = int(time.time())
        execute("UPDATE temporary SET end_at = ? WHERE groups_id = ? AND end_at > ?", (now_ts, groups_id, now_ts))
        
        execute("""
            INSERT INTO temporary (groups_id, from_at, end_at)
            VALUES (?, ?, ?)
        """, (groups_id, from_at, end_at))

        # 立即同步防火墙
        PCInterface.NFT.sync()
        
        return jsonify({"ok": True, "msg": "临时授权已生效"})
    except Exception as e:
        return jsonify({"ok": False, "msg": str(e)}), 500

# 3. 修改授权（续费/调整）
@bp.put("/<int:temp_id>")
def update_temporary(temp_id):
    data = request.get_json(silent=True) or {}
    end_at = data.get("end_at")

    if not end_at:
        return jsonify({"ok": False, "msg": "缺少 end_at 参数"}), 400

    if not query_one("SELECT id FROM temporary WHERE id = ?", (temp_id,)):
        return jsonify({"ok": False, "msg": "记录不存在"}), 404

    try:
        execute("UPDATE temporary SET end_at = ? WHERE id = ?", (end_at, temp_id))
        
        # 立即同步防火墙
        PCInterface.NFT.sync()
        return jsonify({"ok": True, "msg": "截止时间已更新"})
    except Exception as e:
        return jsonify({"ok": False, "msg": str(e)}), 500

# 4. 立即断网（核心修改：将 end_at 设为当前时间）
@bp.post("/<int:temp_id>/stop")
@bp.delete("/<int:temp_id>") # 同时支持 DELETE 和专门的 stop 接口
def stop_temporary(temp_id):
    if not query_one("SELECT id FROM temporary WHERE id = ?", (temp_id,)):
        return jsonify({"ok": False, "msg": "记录不存在"}), 404

    try:
        # 获取当前时间戳
        now_ts = int(time.time())
        
        # 将结束时间改为现在，从而使策略失效
        execute("UPDATE temporary SET end_at = ? WHERE id = ?", (now_ts, temp_id))
        # execute("DELETE FROM temporary WHERE id = ?", (temp_id,)
        
        # 立即同步防火墙。sync_firewall_state 会发现 ? BETWEEN from_at AND end_at 不再成立
        PCInterface.NFT.sync()
        
        return jsonify({"ok": True, "msg": "已强制结束授权，设备已断网"})
    except Exception as e:
        return jsonify({"ok": False, "msg": str(e)}), 500