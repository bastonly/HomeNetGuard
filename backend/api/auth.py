# /opt/parent-control/backend/api/auth.py
from flask import Blueprint, request, jsonify, make_response
from io import BytesIO
from PIL import Image, ImageDraw, ImageFont
from uuid import uuid4

import base64
import random, string, time, json, os, datetime

# 导入自定义配置和工具
import config
from utils.logger import log_login
from utils.security import is_ip_banned, record_fail, get_client_ip
import hashlib

# 导入 JWT 核心组件
from flask_jwt_extended import (
    create_access_token, 
    create_refresh_token, 
    jwt_required, 
    get_jwt_identity
)

bp = Blueprint("auth", __name__, url_prefix="/api")

# ---------- 验证码（内存存储） ----------
CAPTCHA_STORE = {}   # captcha_id -> {"code": "AB12", "ts": 173...}
CAPTCHA_EXPIRE = 300 # 5分钟过期

def _load_users():
    """从 base.conf 加载用户"""
    # 兼容旧逻辑：返回 {username: password} 字典
    conf = config.get_base_config()
    sys_user = conf.get("SYSTEM_USER", {})
    u = sys_user.get("USER")
    p = sys_user.get("PASSWORD")
    if u and p:
        return {u: p}
    return {}

def _gc_captcha():
    """清理过期验证码"""
    now = time.time()
    expired = [k for k, v in CAPTCHA_STORE.items() if now - v.get("ts", 0) > CAPTCHA_EXPIRE]
    for k in expired:
        CAPTCHA_STORE.pop(k, None)

def _gen_code(length=4):
    return "".join(random.choices(string.ascii_uppercase + string.digits, k=length))

def _gen_image(code: str) -> bytes:
    width, height = 120, 40
    img = Image.new("RGB", (width, height), (255, 255, 255))
    d = ImageDraw.Draw(img)

    # 使用 config 中定义的字体路径
    if os.path.exists(config.FONT_PATH):
        font = ImageFont.truetype(config.FONT_PATH, 24)
    else:
        font = ImageFont.load_default()

    bbox = d.textbbox((0, 0), code, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    d.text(((width - tw) // 2, (height - th) // 2), code, font=font, fill=(0, 0, 0))

    for _ in range(60):
        d.point((random.randint(0, width - 1), random.randint(0, height - 1)), 
                fill=(random.randint(0, 255), random.randint(0, 255), random.randint(0, 255)))

    buf = BytesIO()
    img.save(buf, format="PNG")
    return buf.getvalue()

# ---------- API 接口实现 ----------

@bp.route("/captcha", methods=["GET"])
def captcha():
    _gc_captcha()
    code = _gen_code()
    captcha_id = uuid4().hex
    CAPTCHA_STORE[captcha_id] = {"code": code, "ts": time.time()}

    # 生成图片字节流
    png_bytes = _gen_image(code)

    # 核心修改：将字节流转为 Base64 字符串
    base64_data = base64.b64encode(png_bytes).decode('utf-8')
    img_src = f"data:image/png;base64,{base64_data}"

    # 直接通过 JSON 返回数据
    return jsonify({
        "ok": True,
        "captcha_id": captcha_id,
        "captcha_img": img_src
    })

@bp.route("/login", methods=["POST"])
def login():
    data = request.get_json(silent=True) or {}
    username = (data.get("username") or "").strip()
    password = data.get("password") or ""
    user_captcha = (data.get("captcha") or "").strip().upper()
    captcha_id = data.get("captcha_id") or ""

    ip = get_client_ip(request)
    ua = request.headers.get("User-Agent", "-")

    # 1. IP 安全检查
    if is_ip_banned(ip):
        log_login(config.LOGIN_LOG, "-", "FAIL", "IP_BANNED", ip=ip, ua=ua)
        return jsonify({"ok": False, "msg": "api.login_ip_banned"}), 403

    # 2. 验证码校验
    item = CAPTCHA_STORE.get(captcha_id)
    if not item or item.get("code", "").upper() != user_captcha:
        record_fail(ip)
        return jsonify({"ok": False, "msg": "api.login_captcha_error"})

    CAPTCHA_STORE.pop(captcha_id, None)

    # 3. 账户密码校验
    users = _load_users()
    
    # Calculate MD5 for the input password
    password_md5 = hashlib.md5(password.encode('utf-8')).hexdigest()
    
    if users.get(username) != password_md5:
        just_banned = record_fail(ip)
        log_login(config.LOGIN_LOG, username, "FAIL", "PWD_ERROR" + ("_BANNED" if just_banned else ""), ip=ip, ua=ua)
        return jsonify({"ok": False, "msg": "api.login_failed"})

    # 4. 登录成功：颁发双 Token
    access_token = create_access_token(identity=username)
    refresh_token = create_refresh_token(identity=username)

    log_login(config.LOGIN_LOG, username, "OK", ip=ip, ua=ua)
    
    return jsonify({
        "ok": True,
        "access_token": access_token,
        "refresh_token": refresh_token,
        "username": username
    })

@bp.route("/refresh", methods=["POST"])
@jwt_required(refresh=True)
def refresh():
    """使用 refresh_token 换取新的 access_token"""
    current_user = get_jwt_identity()
    new_access_token = create_access_token(identity=current_user)
    return jsonify({
        "ok": True, 
        "access_token": new_access_token
    })

@bp.route("/session", methods=["GET"])
@jwt_required()
def check_session():
    """验证 access_token 是否有效"""
    current_user = get_jwt_identity()
    return jsonify({"ok": True, "username": current_user})

@bp.route("/logout", methods=["POST"])
@jwt_required()
def logout():
    """退出登录日志记录"""
    current_user = get_jwt_identity()
    log_login(config.LOGIN_LOG, current_user, "LOGOUT", ip=request.remote_addr, ua=request.headers.get("User-Agent"))
    # 注意：JWT 登出主要靠前端清除 localStorage 里的 token
    return jsonify({"ok": True})