import sqlite3
import os

DB_PATH = "/opt/parent-control/data/pc.db"
os.makedirs(os.path.dirname(DB_PATH), exist_ok=True)

conn = sqlite3.connect(DB_PATH)
cursor = conn.cursor()

# 设备表
cursor.execute("""
CREATE TABLE IF NOT EXISTS devices (
 id INTEGER PRIMARY KEY,
 name TEXT,
 mac TEXT UNIQUE,
 remark TEXT,
 group_name TEXT,
 online INTEGER DEFAULT 0
)
""")

# 节假日表
cursor.execute("""
CREATE TABLE IF NOT EXISTS holidays (
 id INTEGER PRIMARY KEY,
 date TEXT UNIQUE,
 name TEXT
)
""")

# 策略表
cursor.execute("""
CREATE TABLE IF NOT EXISTS policies (
 id INTEGER PRIMARY KEY,
 group_name TEXT,
 day_type TEXT,
 start TEXT,
 end TEXT
)
""")

# 临时授权表
cursor.execute("""
CREATE TABLE IF NOT EXISTS temp_access (
 mac TEXT,
 expire_at INTEGER
)
""")

conn.commit()
conn.close()
print("Database initialized successfully!")

