# /opt/parent-control/backend/utils/logger.py
import os
import datetime

def ensure_dir(path: str) -> None:
    os.makedirs(path, exist_ok=True)

def write_line(file_path: str, line: str) -> None:
    ensure_dir(os.path.dirname(file_path))
    with open(file_path, "a", encoding="utf-8") as f:
        f.write(line + "\n")

def log_login(file_path: str, username: str, result: str, reason: str = "", ip: str = "-", ua: str = "-") -> None:
    """
    登录日志：统一格式，便于后续 grep/统计/可视化
    result 建议值：
      OK / FAIL / LOGOUT
    reason 建议值：
      CAPTCHA_EXPIRED / CAPTCHA_ERROR / PASSWORD_ERROR / ...
    """
    ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    user = (username or "-").strip() or "-"
    ua_short = (ua or "-").strip().replace("\n", " ")[:120]
    ip_val = (ip or "-").strip() or "-"

    line = f"{ts} | user={user} | ip={ip_val} | ua={ua_short} | result={result}"
    if reason:
        line += f" | reason={reason}"
    write_line(file_path, line)

import logging
from logging.handlers import TimedRotatingFileHandler

def setup_logger(name, log_file, level=logging.INFO, keep_days=7):
    """
    配置并返回一个logger实例
    """
    formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
    
    # 确保目录存在
    ensure_dir(os.path.dirname(log_file))

    handler = TimedRotatingFileHandler(log_file, when="midnight", interval=1, backupCount=keep_days, encoding='utf-8')
    handler.setFormatter(formatter)
    
    logger = logging.getLogger(name)
    logger.setLevel(level)
    
    # 避免重复添加handler
    if not logger.handlers:
        logger.addHandler(handler)
        
    return logger
