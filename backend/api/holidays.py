from flask import Blueprint, request, jsonify
from utils.pc_interface import PCInterface
from utils.db import query, query_one, execute

bp = Blueprint('holidays', __name__)

# 1. 获取节假日列表
@bp.get("")
def list_holidays():
    # 返回数组结构，按开始日期倒序
    rows = query("SELECT id, start_date, end_date, name FROM holidays ORDER BY start_date DESC")
    return jsonify(rows)

# 2. 新增节假日
@bp.post("")
def create_holiday():
    data = request.get_json(silent=True) or {}
    start_date = data.get("start_date")
    end_date = data.get("end_date")
    name = data.get("name", "")

    if not start_date or not end_date:
        return jsonify({"ok": False, "msg": "请选择日期范围"}), 400
    
    if start_date > end_date:
        return jsonify({"ok": False, "msg": "开始日期不能晚于结束日期"}), 400

    # 校验：检查是否有日期重叠
    # 逻辑：(NewStart <= OldEnd) AND (NewEnd >= OldStart)
    overlap = query_one("""
        SELECT id FROM holidays 
        WHERE (? <= end_date) AND (? >= start_date) 
        LIMIT 1
    """, (start_date, end_date))
    
    if overlap:
        return jsonify({"ok": False, "msg": "所选日期区间与已有节假日重叠"}), 409

    try:
        execute("INSERT INTO holidays (start_date, end_date, name) VALUES (?, ?, ?)", 
                (start_date, end_date, name))
        
        # 立即同步防火墙
        PCInterface.NFT.sync()
        
        return jsonify({"ok": True, "msg": "节假日添加成功"})
    except Exception as e:
        return jsonify({"ok": False, "msg": str(e)}), 500

# 3. 修改节假日
@bp.put("/<int:holiday_id>")
def update_holiday(holiday_id):
    data = request.get_json(silent=True) or {}
    start_date = data.get("start_date")
    end_date = data.get("end_date")
    name = data.get("name", "")

    if not start_date or not end_date:
        return jsonify({"ok": False, "msg": "日期不能为空"}), 400
    
    if start_date > end_date:
        return jsonify({"ok": False, "msg": "开始日期不能晚于结束日期"}), 400

    # 校验：重叠检查（排除自身）
    overlap = query_one("""
        SELECT id FROM holidays 
        WHERE (? <= end_date) AND (? >= start_date) AND id != ?
        LIMIT 1
    """, (start_date, end_date, holiday_id))
    
    if overlap:
        return jsonify({"ok": False, "msg": "修改后的日期区间与其它节假日重叠"}), 409

    try:
        execute("UPDATE holidays SET start_date = ?, end_date = ?, name = ? WHERE id = ?", 
                (start_date, end_date, name, holiday_id))
        
        # 立即同步防火墙
        PCInterface.NFT.sync()
        
        return jsonify({"ok": True, "msg": "修改成功"})
    except Exception as e:
        return jsonify({"ok": False, "msg": str(e)}), 500

# 4. 删除节假日
@bp.delete("/<int:holiday_id>")
def delete_holiday(holiday_id):
    if not query_one("SELECT id FROM holidays WHERE id = ?", (holiday_id,)):
        return jsonify({"ok": False, "msg": "记录不存在"}), 404

    try:
        execute("DELETE FROM holidays WHERE id = ?", (holiday_id,))
        
        # 立即同步防火墙
        PCInterface.NFT.sync()
        
        return jsonify({"ok": True, "msg": "删除成功"})
    except Exception as e:
        return jsonify({"ok": False, "msg": str(e)}), 500
