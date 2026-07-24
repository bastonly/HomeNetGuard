import sqlite3
import os
import shutil
import logging
from datetime import datetime
from contextlib import contextmanager

# 数据库文件路径 / Database file path
DB_PATH = "/opt/parent-control/backend/data/pc.db"

logger = logging.getLogger("DB")

@contextmanager
def get_conn():
    """
    获取数据库连接上下文管理器 (带 30 秒超时)
    Database Connection Context Manager (with 30s timeout)
    """
    conn = sqlite3.connect(DB_PATH, timeout=30)
    conn.row_factory = sqlite3.Row
    
    try:
        yield conn
        conn.commit()
    except Exception:
        conn.rollback()
        raise
    finally:
        conn.close()

def init_db_settings():
    """
    初始化 SQLite 高性能模式与数据库字段自动升级
    Initialize SQLite WAL mode & perform auto schema migrations
    """
    try:
        # 确保数据目录存在 / Ensure data directory exists
        os.makedirs(os.path.dirname(DB_PATH), exist_ok=True)
        
        with sqlite3.connect(DB_PATH, timeout=10) as conn:
            # 开启 WAL 模式 (Write-Ahead Logging) 提高并发性能 / Enable WAL mode for high concurrency
            conn.execute("PRAGMA journal_mode=WAL;")
            # 优化磁盘刷新频率 / Optimize disk sync frequency
            conn.execute("PRAGMA synchronous=NORMAL;")
            
            conn.row_factory = sqlite3.Row
            # 自动升级 policies 表，检查并添加 is_enabled 字段 / Auto-migrate 'policies' table for 'is_enabled'
            cursor = conn.execute("PRAGMA table_info(policies);")
            columns = [r['name'] for r in cursor.fetchall()]
            if 'is_enabled' not in columns:
                logger.info("Adding is_enabled column to policies table... / 为 policies 表添加 is_enabled 字段")
                conn.execute("ALTER TABLE policies ADD COLUMN is_enabled INTEGER DEFAULT 1;")
                
            # 自动升级 groups 表，检查并添加 holiday_kind 字段 / Auto-migrate 'groups' table for 'holiday_kind'
            cursor = conn.execute("PRAGMA table_info(groups);")
            groups_cols = [r['name'] for r in cursor.fetchall()]
            if 'holiday_kind' not in groups_cols:
                logger.info("Adding holiday_kind column to groups table... / 为 groups 表添加 holiday_kind 字段")
                conn.execute("ALTER TABLE groups ADD COLUMN holiday_kind TEXT DEFAULT 'period';")
                
            logger.info("SQLite settings initialized (WAL=ON, SYNCHRONOUS=NORMAL) / 数据库性能配置已完成")
    except Exception as e:
        logger.error(f"Failed to initialize SQLite settings / 初始化 SQLite 失败: {e}")

def query(sql, params=()):
    """
    执行 SQL 查询并返回字典列表 / Execute SQL query and return list of dicts
    """
    with get_conn() as conn:
        cur = conn.execute(sql, params)
        return [dict(r) for r in cur.fetchall()]

def query_one(sql, params=()):
    """
    执行 SQL 查询并返回单条记录 / Execute SQL query and return single row dict
    """
    rows = query(sql, params)
    return rows[0] if rows else None

def execute(sql, params=()):
    """
    执行 SQL 修改指令，返回最后插入的行 ID / Execute SQL mutation and return lastrowid
    """
    with get_conn() as conn:
        cur = conn.execute(sql, params)
        return cur.lastrowid

def executemany(sql, params_list):
    """
    批量执行 SQL 指令 / Execute batch SQL mutations
    """
    with get_conn() as conn:
        conn.executemany(sql, params_list)

def verify_and_repair_db():
    """
    启动时完整性检查与自动修复逻辑
    Database integrity verification & auto repair procedure on startup
    """
    if not os.path.exists(DB_PATH):
        return True # 新创建的数据库由主逻辑初始化 / New DB will be created

    logger.info(f"Checking integrity of {DB_PATH}... / 检查数据库完整性")
    try:
        with sqlite3.connect(DB_PATH, timeout=10) as conn:
            cursor = conn.execute("PRAGMA integrity_check;")
            result = cursor.fetchone()
            if result and result[0] == "ok":
                logger.info("Database integrity OK / 数据库完整性校验通过")
                return True
            else:
                logger.error(f"Database integrity check FAILED / 数据库损毁: {result}")
    except sqlite3.DatabaseError as e:
        logger.error(f"Database connection failed (corruption suspected) / 连接异常: {e}")
    except Exception as e:
        logger.error(f"Unexpected error during integrity check / 检查数据库异常: {e}")

    # 数据库损坏时的自动隔离与恢复流程 / Corruption repair workflow
    logger.critical("!!! DATABASE IS CORRUPT !!! Initiating repair procedure... / 数据库损坏，启动修复")
    
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    corrupt_path = f"{DB_PATH}.corrupt.{timestamp}"
    
    # 隔离坏库 / Quarantine corrupt DB
    try:
        if os.path.exists(DB_PATH):
            os.rename(DB_PATH, corrupt_path)
            if os.path.exists(f"{DB_PATH}-wal"):
                try: os.rename(f"{DB_PATH}-wal", f"{corrupt_path}-wal")
                except: pass
            if os.path.exists(f"{DB_PATH}-shm"):
                try: os.rename(f"{DB_PATH}-shm", f"{corrupt_path}-shm")
                except: pass
    except Exception as e:
        logger.error(f"Failed to move corrupt DB / 隔离坏库失败: {e}")
        return False

    # 尝试从最近备份恢复 / Attempt restoration from newest backup
    potential_backups = []
    days = ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"]
    for day in days:
        bk_path = f"{DB_PATH}.bak.{day}"
        if os.path.exists(bk_path):
            potential_backups.append(bk_path)
    
    if os.path.exists(f"{DB_PATH}.bak"):
        potential_backups.append(f"{DB_PATH}.bak")
        
    potential_backups.sort(key=lambda x: os.path.getmtime(x), reverse=True)
    
    restored = False
    for backup_path in potential_backups:
        logger.info(f"Attempting restore from {backup_path}... / 尝试从备份恢复")
        try:
            shutil.copy2(backup_path, DB_PATH)
            if verify_and_repair_db():
                logger.info(f"Successfully restored from {backup_path} / 恢复成功")
                restored = True
                break
        except Exception as e:
            logger.error(f"Failed to restore backup {backup_path} / 从备份恢复失败: {e}")

    if not restored:
        logger.warning("Creating fresh database / 创建全新空数据库")
    return True

def backup_db():
    """
    热备份数据库 (按星期轮询) / Hot backup database with weekly rotation
    """
    try:
        execute("PRAGMA wal_checkpoint(TRUNCATE);")
        day = datetime.now().strftime("%A")
        dest = f"{DB_PATH}.bak.{day}"
        shutil.copy2(DB_PATH, dest)
        logger.info(f"Database backed up to {dest} / 备份数据库成功")
        return True
    except Exception as e:
        logger.error(f"Backup failed / 备份数据库失败: {e}")
        return False