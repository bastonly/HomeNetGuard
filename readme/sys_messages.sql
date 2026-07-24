-- -------------------------------------------------------------
-- TablePlus 6.8.0(654)
--
-- https://tableplus.com/
--
-- Database: pc.db
-- Generation Time: 2026-02-13 11:46:30.7980
-- -------------------------------------------------------------


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
, group_name TEXT);

