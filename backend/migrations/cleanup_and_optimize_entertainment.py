#!/usr/bin/env python3
"""
数据库清理与优化脚本 (修复版)
1. 合并 entertainment_detections 表中的重复项
2. 添加 UNIQUE 索引以防止未来产生重复项
"""
import sqlite3
import os
import sys

# 设置路径
project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DB_PATH = os.path.join(project_root, 'backend/data/pc.db')

def cleanup_and_optimize():
    if not os.path.exists(DB_PATH):
        print(f"Error: Database not found at {DB_PATH}")
        return False

    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    cursor = conn.cursor()

    try:
        print("1. 识别并合并重复记录...")
        
        # 查找重复组
        cursor.execute("""
            SELECT device_id, domain, detection_date, COUNT(*) as cnt
            FROM entertainment_detections
            GROUP BY device_id, domain, detection_date
            HAVING cnt > 1
        """)
        duplicates = cursor.fetchall()
        
        if not duplicates:
            print("   没有发现重复记录。")
        else:
            print(f"   发现 {len(duplicates)} 组重复数据，开始合并...")
            for dup in duplicates:
                d_id = dup['device_id']
                dom = dup['domain']
                d_date = dup['detection_date']
                
                # 获取该组所有数据，确保包含 category
                cursor.execute("""
                    SELECT id, category, visit_count, first_seen, last_seen 
                    FROM entertainment_detections
                    WHERE device_id = ? AND domain = ? AND detection_date = ?
                """, (d_id, dom, d_date))
                rows = cursor.fetchall()
                
                # 合并逻辑
                total_visits = sum(row['visit_count'] for row in rows)
                # 处理 None 值
                f_seen_list = [row['first_seen'] for row in rows if row['first_seen']]
                l_seen_list = [row['last_seen'] for row in rows if row['last_seen']]
                
                first_seen = min(f_seen_list) if f_seen_list else None
                last_seen = max(l_seen_list) if l_seen_list else None
                category = rows[0]['category']
                
                # 删除所有旧项
                ids_to_delete = [row['id'] for row in rows]
                cursor.execute(f"DELETE FROM entertainment_detections WHERE id IN ({','.join(['?']*len(ids_to_delete))})", ids_to_delete)
                
                # 插入合并后的新项
                cursor.execute("""
                    INSERT INTO entertainment_detections (device_id, domain, category, visit_count, first_seen, last_seen, detection_date)
                    VALUES (?, ?, ?, ?, ?, ?, ?)
                """, (d_id, dom, category, total_visits, first_seen, last_seen, d_date))
            
            print(f"   ✓ 合并完成。")

        print("2. 应用 UNIQUE 索引...")
        cursor.execute("DROP INDEX IF EXISTS idx_ent_det_device_date")
        cursor.execute("DROP INDEX IF EXISTS idx_ent_det_unique") # 确保彻底删除旧的尝试
        cursor.execute("""
            CREATE UNIQUE INDEX IF NOT EXISTS idx_ent_det_unique 
            ON entertainment_detections (device_id, domain, detection_date)
        """)
        print("   ✓ 唯一索引创建成功。")

        conn.commit()
        print("\n✅ 数据库清理与优化圆满完成！")
        return True

    except Exception as e:
        conn.rollback()
        print(f"\n❌ 操作失败: {e}")
        return False
    finally:
        conn.close()

if __name__ == "__main__":
    success = cleanup_and_optimize()
    sys.exit(0 if success else 1)
