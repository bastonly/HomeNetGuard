# /opt/parent-control/backend/extensions.py
from flask_cors import CORS
import config # 确保导入了你的 config 文件

def init_extensions(app):
    # 使用你 config.py 里的密钥
    app.secret_key = config.JWT_SECRET_KEY

    # 1. 修复 CORS 配置
    CORS(app,
         resources={
             r"/api/*": {
                 # 必须指定明确的域名，不能用 "*"
                 "origins": [
                     "http://localhost:5173", 
                    #  "http://127.0.0.1:5173",
                    #  "http://localhost:5000",
                     "https://bastonly.top",
                     "https://www.bastonly.top"
                ],
                 "supports_credentials": True, # 允许 Cookie
                 "allow_headers": ["Content-Type", "Authorization"] # 允许 JWT 请求头
             }
         })

    # 2. 优化 Cookie 配置
    app.config.update(
        SESSION_COOKIE_SAMESITE='Lax',
        SESSION_COOKIE_SECURE=False,
        SESSION_COOKIE_HTTPONLY=True,
        # 增加跨域 Cookie 的相关配置
        REMEMBER_COOKIE_SAMESITE='Lax',
        REMEMBER_COOKIE_SECURE=False
    )