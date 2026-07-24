#!/usr/bin/env python3
"""
数据库迁移脚本：添加娱乐网站检测功能
执行方式：python3 backend/migrations/add_entertainment_detection.py
"""
import sqlite3
import os
import sys

# 添加项目根目录到路径
project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, project_root)

DB_PATH = os.path.join(project_root, 'backend/data/pc.db')

def migrate():
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    
    try:
        print("开始数据库迁移...")
        
        # 1. 创建 entertainment_categories 表
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS entertainment_categories (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                domain TEXT NOT NULL UNIQUE,
                category TEXT NOT NULL,
                name TEXT,
                source TEXT DEFAULT 'builtin',
                confidence REAL DEFAULT 1.0,
                last_verified TEXT,
                created_at TEXT DEFAULT (datetime('now', 'localtime'))
            )
        """)
        print("✓ 创建 entertainment_categories 表")
        
        # 2. 创建 entertainment_detections 表
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS entertainment_detections (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_id INTEGER NOT NULL,
                domain TEXT NOT NULL,
                category TEXT NOT NULL,
                visit_count INTEGER DEFAULT 1,
                first_seen TEXT,
                last_seen TEXT,
                detection_date TEXT NOT NULL,
                created_at TEXT DEFAULT (datetime('now', 'localtime')),
                FOREIGN KEY (device_id) REFERENCES devices(id)
            )
        """)
        print("✓ 创建 entertainment_detections 表")
        
        # 3. 为 groups 表添加检测开关字段
        try:
            cursor.execute("ALTER TABLE groups ADD COLUMN enable_video_detection INTEGER DEFAULT 0")
            print("✓ 添加 enable_video_detection 字段")
        except sqlite3.OperationalError as e:
            if "duplicate column" in str(e).lower():
                print("  (enable_video_detection 字段已存在)")
            else:
                raise
        
        try:
            cursor.execute("ALTER TABLE groups ADD COLUMN enable_gaming_detection INTEGER DEFAULT 0")
            print("✓ 添加 enable_gaming_detection 字段")
        except sqlite3.OperationalError as e:
            if "duplicate column" in str(e).lower():
                print("  (enable_gaming_detection 字段已存在)")
            else:
                raise
        
        # 4. 创建索引
        cursor.execute("""
            CREATE UNIQUE INDEX IF NOT EXISTS idx_ent_det_unique 
            ON entertainment_detections (device_id, domain, detection_date)
        """)
        cursor.execute("""
            CREATE INDEX IF NOT EXISTS idx_ent_cat_domain 
            ON entertainment_categories (domain)
        """)
        print("✓ 创建索引")
        
        conn.commit()
        print("\n✅ 数据库迁移完成！")
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
