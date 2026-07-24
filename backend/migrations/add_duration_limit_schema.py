#!/usr/bin/env python3
"""
数据库迁移脚本：为 policies 增加时长控制字段，并创建上网历史会话表 group_internet_sessions
执行方式：python3 backend/migrations/add_duration_limit_schema.py
"""
import sqlite3
import os
import sys

# 添加项目根目录到路径
project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, project_root)

DB_PATH = os.path.join(project_root, 'client/backend/data/pc.db')
if not os.path.exists(DB_PATH):
    # 尝试另一个常见的路径以防是在 backend 下直接运行
    DB_PATH = os.path.join(project_root, 'backend/data/pc.db')

def column_exists(cursor, table, column):
    cursor.execute(f"PRAGMA table_info({table})")
    for row in cursor.fetchall():
        if row[1] == column:
            return True
    return False

def migrate():
    if not os.path.exists(DB_PATH):
        print(f"错误: 数据库文件不存在 {DB_PATH}")
        return False

    print(f"正在连接数据库: {DB_PATH}")
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    
    try:
        print("开始升级数据库 Schema...")
        
        # 1. 升级 policies 表
        if not column_exists(cursor, "policies", "limit_type"):
            cursor.execute("ALTER TABLE policies ADD COLUMN limit_type TEXT DEFAULT 'period'")
            print("✓ 已添加 policies.limit_type 字段")
        else:
            print("- policies.limit_type 字段已存在")

        if not column_exists(cursor, "policies", "duration_limit"):
            cursor.execute("ALTER TABLE policies ADD COLUMN duration_limit INTEGER DEFAULT 0")
            print("✓ 已添加 policies.duration_limit 字段")
        else:
            print("- policies.duration_limit 字段已存在")

        # 2. 创建 group_internet_sessions 表
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS group_internet_sessions (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                groups_id INTEGER NOT NULL,
                date TEXT NOT NULL,
                start_time INTEGER NOT NULL,
                end_time INTEGER,
                duration INTEGER DEFAULT 0,
                trigger_reason TEXT
            )
        """)
        print("✓ 已检查/创建 group_internet_sessions 表")
        
        # 3. 创建索引优化查询
        cursor.execute("CREATE INDEX IF NOT EXISTS idx_gis_groups_date ON group_internet_sessions (groups_id, date)")
        print("✓ 已创建索引 idx_gis_groups_date")

        conn.commit()
        print("\n✅ 数据库迁移成功完成！")
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
