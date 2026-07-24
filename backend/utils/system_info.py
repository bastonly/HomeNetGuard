import hashlib
import subprocess
import platform
import uuid
import socket
import os

def get_cmd_output(cmd):
    try:
        # timeout to prevent hanging forever, stderr to devnull to avoid noise
        return subprocess.check_output(cmd, shell=True, timeout=2, stderr=subprocess.DEVNULL).decode().strip()
    except Exception:
        return ""

def get_file_content(path):
    try:
        if os.path.exists(path):
            with open(path, 'r') as f:
                return f.read().strip()
    except Exception:
        pass
    return ""

def get_device_fingerprint():
    """获取设备唯一标识 (SHA256 of block_info_uuid + mac_eth0 + cpu_serial)"""
    try:
        # 1. Root UUID via block info /dev/mmcblk0p2
        # Expected output format: /dev/mmcblk0p2: UUID="xxxx-xxxx" LABEL="..."
        root_uuid = ""
        try:
            block_info = get_cmd_output("block info /dev/mmcblk0p2")
            if 'UUID="' in block_info:
                root_uuid = block_info.split('UUID="')[1].split('"')[0]
        except:
            pass
            
        # 2. MAC Address (eth0 only)
        mac_addr = get_file_content('/sys/class/net/eth0/address')
        
        # 3. CPU Serial
        cpu_serial = ""
        try:
             if os.path.exists('/proc/cpuinfo'):
                 with open('/proc/cpuinfo', 'r') as f:
                     for line in f:
                         if "Serial" in line:
                             parts = line.split(':', 1)
                             if len(parts) > 1:
                                 cpu_serial = parts[1].strip()
                                 break
        except Exception:
             pass

        # Check validity (at least one component should exist to avoid generic fallback if possible, 
        # but user asked for these specific 3. If all fail, maybe fallback to uuid.getnode?)
        if not any([root_uuid, mac_addr, cpu_serial]):
             mac_node = uuid.getnode()
             return ':'.join(('%012X' % mac_node)[i:i+2] for i in range(0, 12, 2))
        
        # Construct the raw string
        # format: root-uuid:{val}|mac:{val}|serial:{val}
        raw_parts = []
        if root_uuid:  raw_parts.append(f"root-uuid:{root_uuid}")
        if mac_addr:   raw_parts.append(f"mac:{mac_addr}")
        if cpu_serial: raw_parts.append(f"serial:{cpu_serial}")
        
        raw_str = "|".join(raw_parts)
        
        return hashlib.sha256(raw_str.encode()).hexdigest()

    except Exception:
        # Fallback in case of unexpected errors
        mac_node = uuid.getnode()
        return ':'.join(('%012X' % mac_node)[i:i+2] for i in range(0, 12, 2))

def get_model():
    """获取设备型号"""
    try:
        # 1. OpenWrt specific: /tmp/sysinfo/model
        if os.path.exists('/tmp/sysinfo/model'):
            with open('/tmp/sysinfo/model', 'r') as f:
                model = f.read().strip()
                if model: return model

        # 2. Linux: /sys/firmware/devicetree/base/model
        if os.path.exists('/sys/firmware/devicetree/base/model'):
             with open('/sys/firmware/devicetree/base/model', 'r') as f:
                model = f.read().strip().rstrip('\x00')
                if model: return model

        # 3. CPU Info
        if os.path.exists('/proc/cpuinfo'):
            with open('/proc/cpuinfo', 'r') as f:
                for line in f:
                    if "Hardware" in line:
                         return line.split(':', 1)[1].strip()
                    if "Model" in line:
                         return line.split(':', 1)[1].strip()
                    if "model name" in line:
                         return line.split(':', 1)[1].strip()
        
        return platform.machine()
    except Exception:
        return platform.machine()

def get_os_version():
    """获取操作系统版本"""
    try:
        # 1. OpenWrt: /etc/openwrt_release
        if os.path.exists('/etc/openwrt_release'):
            with open('/etc/openwrt_release', 'r') as f:
                for line in f:
                    if line.startswith("DISTRIB_DESCRIPTION="):
                        return line.split('=', 1)[1].strip().strip('"\'')
        
        # 2. Standard Linux: /etc/os-release
        if os.path.exists('/etc/os-release'):
             with open('/etc/os-release', 'r') as f:
                for line in f:
                    if line.startswith("PRETTY_NAME="):
                        return line.split('=', 1)[1].strip().strip('"\'')

        return f"{platform.system()} {platform.release()}"
    except Exception:
        return "Unknown OS"

def get_system_info():
    """聚合系统信息"""
    return {
        "device_fingerprint": get_device_fingerprint(),
        "model": get_model(),
        "os_version": get_os_version()
    }
