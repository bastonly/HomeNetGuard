#!/usr/bin/env python3
"""
数据库迁移脚本：为 entertainment_detections 添加唯一索引
防止重复数据并支持 INSERT OR REPLACE
执行方式：python3 backend/migrations/update_entertainment_schema.py
"""
import sqlite3
import os
import sys

# 添加项目根目录到路径
project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, project_root)

DB_PATH = os.path.join(project_root, 'backend/data/pc.db')

def migrate():
    if not os.path.exists(DB_PATH):
        print(f"错误: 数据库文件不存在 {DB_PATH}")
        return False

    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    
    try:
        print("开始增强数据库完整性...")
        
        # 1. 删除旧的普通索引（如果存在）
        cursor.execute("DROP INDEX IF EXISTS idx_ent_det_device_date")
        
        # 2. 创建唯一索引
        # 这样 INSERT OR REPLACE 会基于 (device_id, domain, detection_date) 进行冲突检测
        cursor.execute("""
            CREATE UNIQUE INDEX IF NOT EXISTS idx_ent_det_unique 
            ON entertainment_detections (device_id, domain, detection_date)
        """)
        print("✓ 已创建唯一索引: idx_ent_det_unique (device_id, domain, detection_date)")
        
        conn.commit()
        print("\n✅ 数据库优化完成！")
        return True
        
    except Exception as e:
        conn.rollback()
        print(f"\n❌ 优化失败: {e}")
        return False
    finally:
        conn.close()

if __name__ == "__main__":
    success = migrate()
    sys.exit(0 if success else 1)
