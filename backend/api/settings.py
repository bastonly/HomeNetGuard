from flask import Blueprint, request, jsonify
from datetime import datetime
import config
import logging
import socket
import json
from utils.db import execute
import hashlib
import os
import subprocess

bp = Blueprint("settings", __name__, url_prefix="/api")

logger = logging.getLogger(__name__)

@bp.route("/settings", methods=["GET"])
def get_settings():
    """获取系统设置"""
    try:
        conf = config.get_base_config()
        
        # 脱敏密码
        if "SYSTEM_USER" in conf:
            conf["SYSTEM_USER"]["PASSWORD"] = "******"
            
        if "ADGUARD" in conf and "SERVER" in conf["ADGUARD"]:
             conf["ADGUARD"]["SERVER"]["PASSWORD"] = "******"
        
        if "EMAIL" in conf:
            conf["EMAIL"]["PASSWORD"] = "******"
        
        res_data = conf

        # 附带系统信息供前端展示
        from utils.system_info import get_system_info
        sys_info = get_system_info()
        logger.info(f"Detected System Info: {sys_info}")
        res_data["SYSTEM_INFO"] = sys_info

        # 添加当前系统时间
        res_data["SYSTEM_TIME"] = datetime.now(config.get_timezone()).strftime("%Y-%m-%d %H:%M:%S")

        return jsonify({"ok": True, "data": res_data})
    except Exception as e:
        logger.error(f"Get settings failed: {e}")
        return jsonify({"ok": False, "msg": str(e)}), 500

@bp.route("/settings", methods=["POST"])
def update_settings():
    """更新系统设置"""
    try:
        new_data = request.get_json() or {}
        current_conf = config.get_base_config()
        
        # 1. 更新系统用户 (System User)
        if "SYSTEM_USER" in new_data:
            sys_user = new_data["SYSTEM_USER"]
            if "USER" in sys_user:
                current_conf["SYSTEM_USER"]["USER"] = sys_user["USER"]
            # 只有当密码不为空且不是掩码时才更新
            if "PASSWORD" in sys_user and sys_user["PASSWORD"] and sys_user["PASSWORD"] != "******":
                # MD5 encryption
                pwd_md5 = hashlib.md5(sys_user["PASSWORD"].encode('utf-8')).hexdigest()
                current_conf["SYSTEM_USER"]["PASSWORD"] = pwd_md5

        # 2. 更新服务器设置 (SERVER) - 只允许通过API修改部分字段? 
        # 用户需求：url和license_id,api_version是不可修改的。
        # 这里只允许修改 API_Token (虽然需求里没明确说API_Token可改，但"除...不可修改"意味着其他的可改)
        # 需求: "第二块：服务器相关设置，其中url和license_id,api_version是不可修改的" -> 意味着 API_Token 可改
        if "SERVER" in new_data:
            srv = new_data["SERVER"]
            # 只更新允许的字段
            if "API_Token" in srv:
                 current_conf["SERVER"]["API_Token"] = srv["API_Token"]
            # Allow updating version info if pushed by server (optional, but keep flexible)
            if "LASTEST_API_VERSION" in srv:
                 current_conf["SERVER"]["LASTEST_API_VERSION"] = srv["LASTEST_API_VERSION"]
            if "EXPIRED_DATE" in srv:
                 current_conf["SERVER"]["EXPIRED_DATE"] = srv["EXPIRED_DATE"]
            if "LICENSE_STATUS" in srv:
                 current_conf["SERVER"]["LICENSE_STATUS"] = srv["LICENSE_STATUS"]

        # 3. JWT 内容都可修改
        if "JWT" in new_data:
            jwt_conf = new_data["JWT"]
            for k in ["JWT_SECRET_KEY", "JWT_ACCESS_TOKEN_EXPIRES_MINUTES", "JWT_REFRESH_TOKEN_EXPIRES"]:
                if k in jwt_conf:
                     current_conf["JWT"][k] = jwt_conf[k]

        # 4. AdGuard - 除 template 外都可修改
        if "ADGUARD" in new_data:
            adg = new_data["ADGUARD"]
            if "SERVER" in adg:
                current_server = current_conf["ADGUARD"].get("SERVER", {})
                new_server = adg["SERVER"]
                if "URL" in new_server:
                    current_server["URL"] = new_server["URL"]
                if "USER" in new_server:
                    current_server["USER"] = new_server["USER"]
                if "PASSWORD" in new_server and new_server["PASSWORD"] and new_server["PASSWORD"] != "******":
                    current_server["PASSWORD"] = new_server["PASSWORD"]
                
                current_conf["ADGUARD"]["SERVER"] = current_server
            
            # Template 不允许修改，忽略

        # 4.1 邮件设置 (EMAIL)
        if "EMAIL" in new_data:
            email_new = new_data["EMAIL"]
            if "EMAIL" not in current_conf:
                current_conf["EMAIL"] = {}
            
            for k in ["ENABLED", "SMTP_SERVER", "SMTP_PORT", "USE_SSL", "USER", "RECEIVERS"]:
                if k in email_new:
                    current_conf["EMAIL"][k] = email_new[k]
            
            if "PASSWORD" in email_new and email_new["PASSWORD"] and email_new["PASSWORD"] != "******":
                current_conf["EMAIL"]["PASSWORD"] = email_new["PASSWORD"]

            # 5. 计划任务 (SCHEDULER)
        if "SCHEDULER" in new_data:
            sched_new = new_data["SCHEDULER"]
            if "SCHEDULER" not in current_conf:
                current_conf["SCHEDULER"] = {}
            
            for k in ["SYNC_NFT_INTERVAL", "DEVICE_UPDATE_INTERVAL", "ADGUARD_ANALYSIS_TIME", "ENTERTAINMENT_COLLECT_TIME", "CLEANUP_TIME"]:
                if k in sched_new:
                    current_conf["SCHEDULER"][k] = sched_new[k]

        # 6. 系统设置 (SYSTEM)
        if "SYSTEM" in new_data:
            sys_new = new_data["SYSTEM"]
            if "SYSTEM" not in current_conf:
                current_conf["SYSTEM"] = {}
            if "TIMEZONE" in sys_new:
                new_tz = sys_new["TIMEZONE"]
                current_conf["SYSTEM"]["TIMEZONE"] = new_tz
                
                # [新增] 修改操作系统时区 (OpenWrt/ImmortalWrt)
                from config import TIMEZONE_MAP
                if new_tz in TIMEZONE_MAP:
                    zonename, tz_str = TIMEZONE_MAP[new_tz]
                    try:
                        # 1. 更新 UCI 配置
                        subprocess.run(["uci", "set", "system.@system[0].zonename=" + zonename], check=False)
                        subprocess.run(["uci", "set", "system.@system[0].timezone=" + tz_str], check=False)
                        subprocess.run(["uci", "commit", "system"], check=False)
                        
                        # 2. 尝试更新 /etc/localtime (如果存在 zoneinfo)
                        zoneinfo_path = f"/usr/share/zoneinfo/{zonename}"
                        if os.path.exists(zoneinfo_path):
                            subprocess.run(["ln", "-sf", zoneinfo_path, "/etc/localtime"], check=False)
                        
                        # 3. 重新加载系统配置以生效
                        subprocess.run(["/etc/init.d/system", "reload"], check=False)

                        logger.info(f"系统时区已同步为: {zonename} ({tz_str})")
                    except Exception as ex:
                        logger.error(f"同步系统时区失败: {ex}")

            if "THEME" in sys_new:
                 current_conf["SYSTEM"]["THEME"] = sys_new["THEME"]

        # 7. 日志保留设置 (RETENTION)
        if "RETENTION" in new_data:
            ret_new = new_data["RETENTION"]
            if "RETENTION" not in current_conf:
                current_conf["RETENTION"] = {}
            
            # 校验与保存
            # AdGuard 明细: 最大 10 天
            if "ADGUARD_DETAIL" in ret_new:
                val = int(ret_new["ADGUARD_DETAIL"])
                current_conf["RETENTION"]["ADGUARD_DETAIL"] = min(val, 10)
            
            # AdGuard 汇总: 最大 30 天
            if "ADGUARD_SUMMARY" in ret_new:
                val = int(ret_new["ADGUARD_SUMMARY"])
                current_conf["RETENTION"]["ADGUARD_SUMMARY"] = min(val, 30)
            
            # 安全审计: 最大 30 天
            if "SYS_MESSAGES" in ret_new:
                val = int(ret_new["SYS_MESSAGES"])
                current_conf["RETENTION"]["SYS_MESSAGES"] = min(val, 30)

        # 保存配置
        if config.save_config(current_conf):
            # 如果更新了系统设置（如时区），自动触发一次签到同步到服务器
            if "SYSTEM" in new_data:
                try:
                    # 异步触发，不阻塞响应
                    subprocess.Popen(["/opt/parent-control/service/pc_service", "--checkin"], 
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                    logger.info("Triggered automatic checkin after SYSTEM settings update")
                except Exception as e:
                    logger.error(f"Failed to trigger auto-checkin: {e}")
            
            return jsonify({"ok": True, "msg": "api.save_success"})
        else:
            return jsonify({"ok": False, "msg": "api.save_failed"}), 500

    except Exception as e:
        logger.error(f"Update settings failed: {e}")
        return jsonify({"ok": False, "msg": str(e)}), 500

@bp.route("/activate", methods=["POST"])
def activate_license():
    """激活 License"""
    try:
        data = request.get_json() or {}
        activation_code = data.get("activation_code")
        input_server_url = data.get("server_url", "").strip()
        input_api_token = data.get("api_token", "").strip()
        
        if not activation_code:
            return jsonify({"ok": False, "msg": "api.require_activation_code"}), 400
        
        if not input_server_url:
             return jsonify({"ok": False, "msg": "api.require_server_url"}), 400
             
        if not input_api_token:
             return jsonify({"ok": False, "msg": "api.require_api_token"}), 400

        # 1. 获取当前配置
        current_conf = config.get_base_config()
        server_conf = current_conf.get("SERVER", {})
        
        # 4. Save server settings to config so C++ can use them
        server_conf["URL"] = input_server_url
        server_conf["API_Token"] = input_api_token
        current_conf["SERVER"] = server_conf
        config.save_config(current_conf)

        # 5. Call C++ service to perform activation
        # 激活逻辑统一由 pc_service 处理，移除 Python 端的直接 HTTP 请求
        import subprocess
        cmd = ["/opt/parent-control/service/pc_service", "--activate", activation_code]
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            
            # Reload config to get the updated status and license_id
            updated_conf = config.get_base_config()
            updated_server = updated_conf.get("SERVER", {})
            
            # 无论成功与否，只要配置有更新最好透传给前端
            # 为了兼容前端，我们自行构造类似原来接口返回的格式
            ret_data = {
                "status": "ok" if result.returncode == 0 else "fail",
                "license_id": updated_server.get("LICENSE_ID", ""),
                "license_status": 1 if updated_server.get("LICENSE_STATUS") == "active" else 0, # rough mapping for frontend
                "device_status": 1 if updated_server.get("DEVICE_STATUS") == "active" else 0,
                "expired_date": updated_server.get("EXPIRED_DATE", ""),
                "lastest_api_version": updated_server.get("LASTEST_API_VERSION", "")
            }

            if result.returncode == 0:
                logger.info(f"Activation via pc_service successful. Output: {result.stdout}")
                return jsonify({"ok": True, "msg": "api.activate_success", "data": ret_data})

            else:
                logger.error(f"Activation via pc_service failed. Output: {result.stdout} Error: {result.stderr}")
                return jsonify({"ok": False, "msg": "api.activate_failed", "details": (result.stdout or "") + (result.stderr or "")}), 400
                
        except subprocess.TimeoutExpired:
            logger.error("Activation via pc_service timed out.")
            return jsonify({"ok": False, "msg": "api.activate_timeout"}), 504

        except Exception as ex:
            logger.error(f"Execution of pc_service --activate failed: {ex}")
            return jsonify({"ok": False, "msg": str(ex)}), 500

    except Exception as e:
        logger.error(f"Activation failed: {e}")
        return jsonify({"ok": False, "msg": str(e)}), 500

@bp.route("/reverify_license", methods=["POST"])
def reverify_license():
    """手动触发 License Check-in"""
    try:
        import subprocess
        # Execute the checkin command
        # command: /opt/parent-control/service/pc_service --checkin
        cmd = ["/opt/parent-control/service/pc_service", "--checkin"]
        
        # Run with timeout to prevent hanging
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        
        output = (result.stdout or "") + (result.stderr or "")

        if result.returncode == 0:
            logger.info("Manual checkin successful.")
            return jsonify({"ok": True, "msg": "api.verify_success", "details": result.stdout})
        else:
            # If the binary returns non-zero, it might still have successfully updated the status
            # Check for specific log messages indicating a valid server response
            if "Checkin API returned fail status" in output or "Checkin skipped" in output or "Failed or Inactive" in output:
                 logger.info("Manual checkin completed (server returned fail status or inactive, config updated).")
                 return jsonify({"ok": True, "msg": "api.verify_complete", "details": output})

            logger.error(f"Manual checkin failed. Return code: {result.returncode}, Stderr: {result.stderr}")
            return jsonify({"ok": False, "msg": "api.verify_failed", "details": output}), 500
            
    except subprocess.TimeoutExpired:
        logger.error("Manual checkin timed out.")
        return jsonify({"ok": False, "msg": "api.verify_timeout"}), 504
    except Exception as e:
        logger.error(f"Manual checkin execution error: {e}")
        return jsonify({"ok": False, "msg": str(e)}), 500

@bp.route("/settings/upgrade", methods=["POST"])
def trigger_upgrade():
    """触发系统自动升级"""
    try:
        from utils.pc_interface import PCInterface
        if PCInterface.System.upgrade():
            logger.info("System upgrade process triggered via PCInterface")
            return jsonify({"ok": True, "msg": "api.upgrade_triggered"})
        else:
            logger.error("Failed to trigger upgrade: script missing or failed")
            return jsonify({"ok": False, "msg": "api.upgrade_script_missing"}), 404
    except Exception as e:
        logger.error(f"Failed to trigger upgrade: {e}")
        return jsonify({"ok": False, "msg": str(e)}), 500
