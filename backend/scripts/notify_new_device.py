#!/usr/bin/env python3
import sys
import json
import smtplib
from email.mime.text import MIMEText
from email.header import Header
import os

# 路径配置（兼容本地和服务器）
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONF_PATH = os.path.join(BASE_DIR, "config", "base.conf")

def send_mail(mac, name, ip):
    try:
        if not os.path.exists(CONF_PATH):
            return
            
        with open(CONF_PATH, 'r', encoding='utf-8') as f:
            config = json.load(f)
            
        email_cfg = config.get("EMAIL", {})
        if not email_cfg.get("ENABLED"):
            return

        smtp_server = email_cfg.get("SMTP_SERVER")
        smtp_port = email_cfg.get("SMTP_PORT", 465)
        user = email_cfg.get("USER")
        password = email_cfg.get("PASSWORD")
        receivers = email_cfg.get("RECEIVERS", [])
        use_ssl = email_cfg.get("USE_SSL", True)

        if not user or not password or not receivers:
            print("Email config incomplete.")
            return

        # 邮件内容
        subject = f"【系统通知】发现新设备接入网络"
        content = f"""
您好，系统监测到有新设备接入网络：

设备名称: {name}
MAC 地址: {mac}
IP 地址: {ip}
发现时间: {os.popen('date').read().strip()}

请及时确认该设备是否合法。
"""
        
        message = MIMEText(content, 'plain', 'utf-8')
        message['From'] = user
        message['To'] = ", ".join(receivers)
        message['Subject'] = Header(subject, 'utf-8')

        if use_ssl:
            smtp = smtplib.SMTP_SSL(smtp_server, smtp_port)
        else:
            smtp = smtplib.SMTP(smtp_server, smtp_port)
            
        smtp.login(user, password)
        smtp.sendmail(user, receivers, message.as_string())
        smtp.quit()
        print(f"Notification sent to {len(receivers)} receivers.")

    except Exception as e:
        print(f"Failed to send email: {e}")

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: notify_new_device.py <mac> <name> <ip>")
        sys.exit(1)
        
    send_mail(sys.argv[1], sys.argv[2], sys.argv[3])
