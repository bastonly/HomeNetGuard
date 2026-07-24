-- -------------------------------------------------------------
-- TablePlus 6.8.0(654)
--
-- https://tableplus.com/
--
-- Database: pc.db
-- Generation Time: 2026-02-23 11:08:07.7480
-- -------------------------------------------------------------


DROP TABLE IF EXISTS "devices";
CREATE TABLE "devices" (
    id INTEGER PRIMARY KEY
  , name TEXT
  , ip TEXT
  , groups_id INTEGER DEFAULT(1)
  , mac TEXT
  , status TEXT DEFAULT('offline')
  , last_seen INTEGER
  , remark TEXT
, "type" integer, "hotpot" integer DEFAULT '1');

DROP TABLE IF EXISTS "policies";
CREATE TABLE "policies" (
    id INTEGER PRIMARY KEY
  , groups_id INTEGER
  , "type" TEXT
  , "from_at" TEXT
  , "end_at" TEXT
);

DROP TABLE IF EXISTS "temporary";
CREATE TABLE "temporary" (
    id INTEGER NOT NULL PRIMARY KEY
  , groups_id INTEGER
  , from_at INTEGER
  , "end_at" INTEGER
);

DROP TABLE IF EXISTS "groups";
CREATE TABLE "groups" ("id" integer,"name" varchar,"remark" varchar, "adguard_client_key" varchar, "kind" integer DEFAULT '1', "allow_vpn" integer DEFAULT '1', enable_video_detection INTEGER DEFAULT 0, enable_gaming_detection INTEGER DEFAULT 0, is_blocked INTEGER DEFAULT 0, PRIMARY KEY (id));

DROP TABLE IF EXISTS "holidays";
CREATE TABLE "holidays" ("id" integer,"date" text,"name" text, PRIMARY KEY (id));

DROP TABLE IF EXISTS "adguard";
CREATE TABLE "adguard" ("id" integer,"stat_dt" datetime,"device_id" integer,"domain" text,"query_count" integer,"block_count" integer DEFAULT '0', PRIMARY KEY (id));

DROP TABLE IF EXISTS "adguard_device_logs";
CREATE TABLE adguard_device_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id INTEGER NOT NULL,
    access_time TEXT NOT NULL,
    domain TEXT NOT NULL,
    reason TEXT,
    is_blocked INTEGER DEFAULT 0, query_type TEXT,
    FOREIGN KEY (device_id) REFERENCES devices(id)
);

DROP TABLE IF EXISTS "sqlite_sequence";
CREATE TABLE sqlite_sequence(name,seq);

DROP TABLE IF EXISTS "sys_messages";
CREATE TABLE sys_messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    type TEXT NOT NULL,           
    level TEXT DEFAULT 'info',    
    source_ip TEXT,               
    source_mac TEXT,              
    content TEXT,                 
    payload TEXT,                 
    occurrence_count INTEGER DEFAULT 1, 
    is_read BOOLEAN DEFAULT 0,    
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
, group_name TEXT, category TEXT NOT NULL DEFAULT 'legacy', event_tag TEXT DEFAULT '', source_user TEXT DEFAULT '');

DROP TABLE IF EXISTS "entertainment_categories";
CREATE TABLE entertainment_categories (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                domain TEXT NOT NULL UNIQUE,
                category TEXT NOT NULL,
                name TEXT,
                source TEXT DEFAULT 'builtin',
                confidence REAL DEFAULT 1.0,
                last_verified TEXT,
                created_at TEXT DEFAULT (datetime('now', 'localtime'))
            );

DROP TABLE IF EXISTS "entertainment_detections";
CREATE TABLE entertainment_detections (
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
            );

