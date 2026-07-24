from flask import Blueprint, request, jsonify
from utils.db import query, query_one, execute
from utils.pc_interface import PCInterface

bp = Blueprint("policies", __name__)

VALID_TYPES = ("workday", "weekend", "holiday")

@bp.get("")
def list_policies():
    groups_id = request.args.get("groups_id", type=int)
    if not groups_id:
        return jsonify({"ok": False, "msg": "缺少 groups_id"}), 400

    rows = query("""
        SELECT p.id, p.groups_id, g.remark, p.type, p.from_at, p.end_at, p.limit_type, p.duration_limit, p.is_enabled, g.holiday_kind
        FROM policies as p
        LEFT JOIN groups g ON p.groups_id = g.id
        WHERE p.groups_id = ?
        ORDER BY p.type, from_at
    """, (groups_id,))
    return jsonify(rows)

@bp.post("")
def create_policy():
    data = request.get_json(silent=True) or {}
    groups_id = data.get("groups_id")
    typ = data.get("type")
    limit_type = data.get("limit_type", "period")
    duration_limit = data.get("duration_limit", 0)
    from_at = data.get("from_at", "")
    end_at = data.get("end_at", "")
    is_enabled = data.get("is_enabled", 1)

    if not groups_id:
        return jsonify({"ok": False, "msg": "缺少 groups_id"}), 400
    if typ not in VALID_TYPES:
        return jsonify({"ok": False, "msg": "type 不合法"}), 400

    if not query_one("SELECT id FROM groups WHERE id = ?", (groups_id,)):
        return jsonify({"ok": False, "msg": "所选分组不存在"}), 400

    # 策略基本校验
    if typ == 'holiday' and limit_type == 'duration':
        if not duration_limit or int(duration_limit) <= 0:
            return jsonify({"ok": False, "msg": "时长限制必须大于0分钟"}), 400
        
        # 确保每个组在节假日仅存有一条时长策略配置行，已存在则更新它
        existing_duration = query_one(
            "SELECT id FROM policies WHERE groups_id = ? AND type = 'holiday' AND limit_type = 'duration' LIMIT 1",
            (groups_id,)
        )
        if existing_duration:
            execute(
                "UPDATE policies SET duration_limit = ?, is_enabled = ? WHERE id = ?",
                (int(duration_limit), int(is_enabled), existing_duration['id'])
            )
            PCInterface.NFT.sync()
            return jsonify({"ok": True})

        from_at = ""
        end_at = ""
    else:
        # 时间段控制模式（或工作日、周末）
        limit_type = "period"
        duration_limit = 0
        if not from_at or not end_at:
            return jsonify({"ok": False, "msg": "缺少时间段"}), 400

    execute("""
        INSERT INTO policies (groups_id, type, from_at, end_at, limit_type, duration_limit, is_enabled)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    """, (groups_id, typ, from_at, end_at, limit_type, int(duration_limit), int(is_enabled)))
    
    PCInterface.NFT.sync()
    
    return jsonify({"ok": True})

@bp.put("/<int:policy_id>")
def update_policy(policy_id):
    data = request.get_json(silent=True) or {}
    typ = data.get("type")
    limit_type = data.get("limit_type", "period")
    duration_limit = data.get("duration_limit", 0)
    from_at = data.get("from_at", "")
    end_at = data.get("end_at", "")
    is_enabled = data.get("is_enabled", 1)

    if typ not in VALID_TYPES:
        return jsonify({"ok": False, "msg": "type 不合法"}), 400

    current_policy = query_one("SELECT groups_id FROM policies WHERE id = ?", (policy_id,))
    if not current_policy:
        return jsonify({"ok": False, "msg": "策略不存在"}), 404
        
    groups_id = current_policy['groups_id']

    # 策略基本校验
    if typ == 'holiday' and limit_type == 'duration':
        if not duration_limit or int(duration_limit) <= 0:
            return jsonify({"ok": False, "msg": "时长限制必须大于0分钟"}), 400
        from_at = ""
        end_at = ""
    else:
        limit_type = "period"
        duration_limit = 0
        if not from_at or not end_at:
            return jsonify({"ok": False, "msg": "缺少时间段"}), 400

    execute("""
        UPDATE policies
        SET type = ?, from_at = ?, end_at = ?, limit_type = ?, duration_limit = ?, is_enabled = ?
        WHERE id = ?
    """, (typ, from_at, end_at, limit_type, int(duration_limit), int(is_enabled), policy_id))
    
    PCInterface.NFT.sync()

    return jsonify({"ok": True})

@bp.get("/<int:policy_id>")
def get_policy(policy_id):
    row = query_one("""
        SELECT id, groups_id, type, from_at, end_at, limit_type, duration_limit, is_enabled 
        FROM policies WHERE id = ?
    """, (policy_id,))
    if not row:
        return jsonify({"ok": False, "msg": "策略不存在"}), 404
    return jsonify(row)

@bp.delete("/<int:policy_id>")
def delete_policy(policy_id):
    if not query_one("SELECT id FROM policies WHERE id = ?", (policy_id,)):
        return jsonify({"ok": False, "msg": "策略不存在"}), 404

    execute("DELETE FROM policies WHERE id = ?", (policy_id,))
    
    PCInterface.NFT.sync()

    return jsonify({"ok": True})

# --- 新增：获取与更新设备组的节假日控制模式 ---
@bp.get("/groups/<int:groups_id>/holiday-mode")
def get_group_holiday_mode(groups_id):
    row = query_one("SELECT holiday_kind FROM groups WHERE id = ?", (groups_id,))
    if not row:
        return jsonify({"ok": False, "msg": "设备组不存在"}), 404
    return jsonify({"holiday_mode": row["holiday_kind"] or "period"})

@bp.put("/groups/<int:groups_id>/holiday-mode")
def update_group_holiday_mode(groups_id):
    data = request.get_json(silent=True) or {}
    holiday_mode = data.get("holiday_mode")
    if holiday_mode not in ('period', 'duration'):
        return jsonify({"ok": False, "msg": "模式不合法"}), 400
        
    execute("UPDATE groups SET holiday_kind = ? WHERE id = ?", (holiday_mode, groups_id))
    PCInterface.NFT.sync()
    return jsonify({"ok": True})
