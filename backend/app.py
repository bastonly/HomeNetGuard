import os
import sys
import pytz
import datetime
from flask import Flask, request, jsonify 
from flask_jwt_extended import JWTManager, verify_jwt_in_request 

# --- 1. 环境路径配置 / Environment Path Configuration ---
ROOT_PATH = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if ROOT_PATH not in sys.path:
    sys.path.append(ROOT_PATH)

# --- 2. 业务模块导入 / Business Module Imports ---
import config  

from extensions import init_extensions
from utils.db import verify_and_repair_db, init_db_settings
from utils.i18n_sync import sync_translations_to_redis

# 蓝图导入 / Blueprint Imports
from api.home import bp as home_bp
from api.devices import bp as devices_bp
from api.groups import bp as groups_bp
from api.policies import bp as policies_bp
from api.temporary import bp as temporary_bp
from api.holidays import bp as holidays_bp
from api.stats import bp as stats_bp
from api.messages import bp as messages_bp
from api.auth import bp as auth_bp
from api.entertainment import bp as entertainment_bp
from api.settings import bp as settings_bp
from api.i18n import i18n_bp
from api.usage import bp as usage_bp

def create_app():
    """
    Flask 应用工厂函数 / Flask Application Factory Function
    """
    app = Flask(__name__)
    
    # --- 0. 数据库完整性自检与修复 / DB Integrity Self-check & Auto Repair ---
    try:
        if not verify_and_repair_db():
             app.logger.critical("Database repair failed / 数据库修复失败")
        else:
             init_db_settings()
    except Exception as e:
        app.logger.critical(f"Critical error during DB check / 数据库检测发生严重错误: {e}")

    # --- 0.1 多语言 Redis 强制同步 / Force i18n Sync to Redis ---
    try:
        sync_translations_to_redis()
    except Exception as e:
        app.logger.error(f"Failed to sync translations on startup / 启动时同步多语言失败: {e}")

    # --- 1. 配置管理 / Configuration Initialization ---
    config.init_config()
    jwt_key = config.get_base_config().get("JWT", {}).get("JWT_SECRET_KEY")
    jwt_access_expires = datetime.timedelta(minutes=config.get_base_config().get("JWT", {}).get("JWT_ACCESS_TOKEN_EXPIRES_MINUTES", 60))
    jwt_refresh_expires = datetime.timedelta(days=config.get_base_config().get("JWT", {}).get("JWT_REFRESH_TOKEN_EXPIRES", 7))

    app.config["JWT_SECRET_KEY"] = jwt_key
    app.config["JWT_ACCESS_TOKEN_EXPIRES"] = jwt_access_expires
    app.config["JWT_REFRESH_TOKEN_EXPIRES"] = jwt_refresh_expires
    app.secret_key = jwt_key
    app.url_map.strict_slashes = False

    # --- 2. 初始化扩展与鉴权钩子 / Initialize Extensions & Auth Hook ---
    jwt = JWTManager(app)
    init_extensions(app)

    @app.before_request
    def check_jwt_authentication():
        """
        请求前统一鉴权检查 / Unified Authentication Check Before Processing Request
        """
        if request.method == "OPTIONS":
            return None
        if not request.path.startswith('/api/'):
            return None
        # 开放无需 JWT 校验的白名单接口 / Whitelisted Endpoints Exempt from JWT
        white_list = ['/api/login', '/api/logout', '/api/captcha', '/api/refresh', '/api/i18n', '/api/usage']
        if any(request.path == p or request.path.startswith(p) for p in white_list):
            return None
        try:
            verify_jwt_in_request()
        except Exception as e:
            return jsonify({"ok": False, "msg": "身份验证失效 / Authentication expired", "details": str(e)}), 401

    # --- 3. 注册 API 蓝图 / Register API Blueprints ---
    blueprints = [
        (auth_bp, None),
        (devices_bp, '/api/devices'),
        (groups_bp, '/api/groups'),
        (policies_bp, '/api/policies'),
        (temporary_bp, '/api/temporary'),
        (holidays_bp, '/api/holidays'),
        (messages_bp, '/api/messages'),
        (stats_bp, '/api/stats'),
        (entertainment_bp, '/api/entertainment'),
        (home_bp, None),
        (settings_bp, None),
        (i18n_bp, '/api/i18n'),
        (usage_bp, '/api/usage')
    ]
    for bp, prefix in blueprints:
        app.register_blueprint(bp, url_prefix=prefix)

    return app

app = create_app()

if __name__ == '__main__':
    # 启动 Flask 开发服务器 / Launch Flask Dev Server
    app.run(host='0.0.0.0', port=5000, debug=False, use_reloader=False, threaded=True)