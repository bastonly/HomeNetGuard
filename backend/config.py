import os
import json
import secrets
import datetime
import pytz
from typing import Dict, Any

# 基础路径配置 / Base Directory Paths
BASE_DIR = os.path.dirname(__file__)
CONFIG_PATH = os.path.join(BASE_DIR, 'config')
BASE_CONF_PATH = os.path.join(CONFIG_PATH, 'base.conf')
LOGIN_LOG = os.path.join(CONFIG_PATH, 'login.log')
FONT_PATH = os.path.join(BASE_DIR, 'ttf', 'DejaVuSans-Bold.ttf')

# 时区名称到 OpenWrt UCI 格式的映射 / Mapping of Timezone Names to OpenWrt UCI Timezone Strings
TIMEZONE_MAP = {
    "America/New_York": ("America/New_York", "EST5EDT,M3.2.0,M11.1.0"),
    "US/Eastern": ("America/New_York", "EST5EDT,M3.2.0,M11.1.0"),
    "US/Central": ("America/Chicago", "CST6CDT,M3.2.0,M11.1.0"),
    "US/Mountain": ("America/Denver", "MST7MDT,M3.2.0,M11.1.0"),
    "US/Pacific": ("America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"),
    "Asia/Shanghai": ("Asia/Shanghai", "CST-8"),
    "Asia/Hong_Kong": ("Asia/Hong_Kong", "HKT-8"),
    "UTC": ("UTC", "UTC0")
}

# 默认通用配置模板 / Default Generic Configuration Template
DEFAULT_CONFIG = {
    "SYSTEM": {
        "TIMEZONE": "Asia/Shanghai",
        "THEME": "auto"
    },
    "SERVER": {
        "URL": "https://verify.example.com",
        "API_Token": "your_api_token",
        "LICENSE_ID": "LIC-XXXX-XXXXXX",
        "CURRENT_API_VERSION": "1.0.0",
        "LASTEST_API_VERSION": "1.0.0",
        "EXPIRED_DATE": "2030-01-01 00:00:00",
        "LICENSE_STATUS": "active"
    },
    "SYSTEM_USER": {
        "USER": "admin",
        "PASSWORD": "21232f297a57a5a743894a0e4a801fc3" # MD5 for "admin"
    },
    "JWT": {
        "JWT_SECRET_KEY": "", # 自动随机生成 / Auto-generated if empty
        "JWT_ACCESS_TOKEN_EXPIRES_MINUTES": 60,
        "JWT_REFRESH_TOKEN_EXPIRES": 7
    },
    "ADGUARD": {
        "SERVER": {
            "URL": "http://127.0.0.1:3000",
            "USER": "admin",
            "PASSWORD": "adguard_password"
        },
        "TEMPLATE": {}
    },
    "SCHEDULER": {
        "SYNC_NFT_INTERVAL": 60,
        "DEVICE_UPDATE_INTERVAL": 30,
        "ADGUARD_ANALYSIS_TIME": "00:05",
        "ENTERTAINMENT_COLLECT_TIME": "00:10",
        "CLEANUP_TIME": "00:20"
    },
    "REDIS": {
        "HOST": "127.0.0.1",
        "PORT": 6379,
        "PASSWORD": "",
        "DB": 0
    },
    "EMAIL": {
        "ENABLED": False,
        "SMTP_SERVER": "smtp.example.com",
        "SMTP_PORT": 465,
        "USE_SSL": True,
        "USER": "",
        "PASSWORD": "",
        "RECEIVERS": []
    }
}

def init_config():
    """
    初始化配置目录和必要文件 / Initialize configuration directory and base files
    """
    if not os.path.exists(CONFIG_PATH):
        os.makedirs(CONFIG_PATH)
        
    # 初始化 base.conf / Initialize base.conf if missing
    if not os.path.exists(BASE_CONF_PATH):
        config = DEFAULT_CONFIG.copy()
        config["JWT"]["JWT_SECRET_KEY"] = secrets.token_urlsafe(32)
        save_config(config)
    else:
        # 检查现有配置中是否存在 JWT 秘钥 / Check if JWT key is missing in existing config
        try:
            with open(BASE_CONF_PATH, 'r', encoding='utf-8') as f:
                current_conf = json.load(f)
            
            jwt_conf = current_conf.get("JWT", {})
            if not jwt_conf.get("JWT_SECRET_KEY"):
                if "JWT" not in current_conf:
                    current_conf["JWT"] = DEFAULT_CONFIG["JWT"].copy()
                
                current_conf["JWT"]["JWT_SECRET_KEY"] = secrets.token_urlsafe(32)
                save_config(current_conf)
        except Exception:
            pass

def get_base_config() -> Dict[str, Any]:
    """
    读取 base.conf 配置文件 / Read base.conf configuration
    """
    init_config()
    try:
        with open(BASE_CONF_PATH, 'r', encoding='utf-8') as f:
            return json.load(f)
    except Exception:
        return {}

def save_config(config_data: Dict[str, Any]):
    """
    保存配置到 base.conf / Save configuration data to base.conf
    """
    try:
        with open(BASE_CONF_PATH, 'w', encoding='utf-8') as f:
            json.dump(config_data, f, indent=4, ensure_ascii=False)
        return True
    except Exception:
        return False

def get_timezone():
    """
    从配置中获取 pytz 时区对象 / Get pytz timezone instance from config
    """
    conf = get_base_config()
    tz_str = conf.get("SYSTEM", {}).get("TIMEZONE", "Asia/Shanghai")
    try:
        return pytz.timezone(tz_str)
    except Exception:
        return pytz.timezone("Asia/Shanghai")

# 导出全局变量供外部调用 / Export global config variables for external module access
_config_data = get_base_config()
JWT_SECRET_KEY = _config_data.get("JWT", {}).get("JWT_SECRET_KEY")
JWT_ACCESS_EXPIRES = datetime.timedelta(minutes=_config_data.get("JWT", {}).get("JWT_ACCESS_TOKEN_EXPIRES_MINUTES", 60))
JWT_REFRESH_EXPIRES = datetime.timedelta(days=_config_data.get("JWT", {}).get("JWT_REFRESH_TOKEN_EXPIRES", 7))