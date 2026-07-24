#!/usr/bin/env python3
"""
数据库迁移脚本：重构访问统计明细表结构
1. 重建 adguard_device_logs 表以支持聚合存储 (count, begin_at, end_at, is_blocked)
2. 更新唯一索引 idx_adguard_agg
3. 为 entertainment_detections 补全唯一索引 idx_ent_agg
执行方式：python3 backend/migrations/refactor_adguard_stats_schema.py
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
        print("开始实施统计架构重构迁移...")
        
        # 1. 重建 adguard_device_logs
        print("正在重建 adguard_device_logs 表...")
        cursor.execute("DROP TABLE IF EXISTS adguard_device_logs")
        cursor.execute("""
            CREATE TABLE adguard_device_logs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_id INTEGER NOT NULL,
                domain TEXT NOT NULL,
                query_type TEXT,
                is_blocked INTEGER DEFAULT 0,
                count INTEGER DEFAULT 1,
                access_date TEXT NOT NULL,
                begin_at TEXT,
                end_at TEXT,
                created_at TEXT DEFAULT (datetime('now', 'localtime')),
                FOREIGN KEY (device_id) REFERENCES devices(id)
            )
        """)
        
        # 2. 创建统计表唯一索引
        cursor.execute("""
            CREATE UNIQUE INDEX IF NOT EXISTS idx_adguard_agg 
            ON adguard_device_logs (device_id, domain, query_type, is_blocked, access_date)
        """)
        print("✓ 已重建 adguard_device_logs 并创建唯一索引")

        # 3. 优化 entertainment_detections 索引
        print("正在优化 entertainment_detections 索引...")
        cursor.execute("DROP INDEX IF EXISTS idx_ent_det_unique") # 移除旧名
        cursor.execute("DROP INDEX IF EXISTS idx_ent_agg")
        
        # 清理可能存在的重复数据，确保唯一索引能创建成功
        cursor.execute("""
            DELETE FROM entertainment_detections 
            WHERE id NOT IN (
                SELECT MIN(id) FROM entertainment_detections 
                GROUP BY device_id, domain, detection_date
            )
        """)
        
        cursor.execute("""
            CREATE UNIQUE INDEX IF NOT EXISTS idx_ent_agg 
            ON entertainment_detections (device_id, domain, detection_date)
        """)
        print("✓ 已清理重复数据并更新 entertainment_detections 唯一索引")
        
        # 4. 提交之前的变更，为 VACUUM 做准备
        conn.commit()
        
        # 5. 压缩数据库 (VACUUM 不能在事务中执行)
        print("正在压缩数据库 (VACUUM)...")
        conn.isolation_level = None # 开启 autocommit 模式
        conn.execute("VACUUM")
        conn.isolation_level = ""   # 恢复默认模式
        print("✓ 数据库压缩完成")
        
        print("\n✅ 统计架构迁移成功！数据清理将按计划任务执行。")
        return True
        
    except Exception as e:
        conn.rollback()
        print(f"\n❌ 迁移失败: {e}")
        return False
    finally:
        conn.close()

if __name__ == "__main__":
    success = migrate()
    sys.exit(0 if success else 1)
