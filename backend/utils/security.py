# utils/security.py
import os
import time

BASE_DIR = os.path.dirname(os.path.dirname(__file__))
CONFIG_DIR = os.path.join(BASE_DIR, "config")

FAIL_LOG = os.path.join(CONFIG_DIR, "login_failures.log")
BAN_LIST = os.path.join(CONFIG_DIR, "banned_ips.txt")

MAX_FAILS = 5  # 失败次数阈值

def _ensure():
    os.makedirs(CONFIG_DIR, exist_ok=True)
    if not os.path.exists(FAIL_LOG):
        open(FAIL_LOG, "a").close()
    if not os.path.exists(BAN_LIST):
        open(BAN_LIST, "a").close()


def is_ip_banned(ip: str) -> bool:
    _ensure()
    ip = (ip or "").strip()
    if not ip:
        return False

    with open(BAN_LIST, "r", encoding="utf-8") as f:
        banned = {line.strip() for line in f if line.strip()}

    return ip in banned


def record_fail(ip: str) -> bool:
    """
    记录一次失败
    返回 True 表示：刚刚被封禁
    """
    _ensure()
    ip = (ip or "").strip()
    if not ip:
        return False

    now = int(time.time())

    # 记录失败
    with open(FAIL_LOG, "a", encoding="utf-8") as f:
        f.write(f"{now},{ip}\n")

    # 统计失败次数
    fails = 0
    with open(FAIL_LOG, "r", encoding="utf-8") as f:
        for line in f:
            try:
                _, ip2 = line.strip().split(",", 1)
                if ip2 == ip:
                    fails += 1
            except ValueError:
                continue

    if fails >= MAX_FAILS:
        with open(BAN_LIST, "a", encoding="utf-8") as f:
            f.write(ip + "\n")
        return True

    return False


def clear_ip(ip: str):
    """手动解封：从封禁列表中移除 IP"""
    _ensure()
    ip = (ip or "").strip()
    if not ip:
        return

    with open(BAN_LIST, "r", encoding="utf-8") as f:
        lines = [line.strip() for line in f if line.strip() and line.strip() != ip]

    with open(BAN_LIST, "w", encoding="utf-8") as f:
        for line in lines:
            f.write(line + "\n")
def get_client_ip(request):
    """获取真实客户端 IP (穿透代理)"""
    # 优先从 X-Forwarded-For 获取 (可能有多个 IP，取第一个)
    forwarded = request.headers.get("X-Forwarded-For")
    if forwarded:
        # X-Forwarded-For: client, proxy1, proxy2...
        return forwarded.split(",")[0].strip()
    
    # 其次从 X-Real-IP 获取
    real_ip = request.headers.get("X-Real-IP")
    if real_ip:
        return real_ip.strip()
    
    # 最后回退到 remote_addr (通常是 127.0.0.1)
    return request.remote_addr or "-"
