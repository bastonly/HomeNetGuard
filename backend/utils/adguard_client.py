import json
import requests
import tldextract
import os
import sys
import logging
from datetime import datetime, timedelta
from backend.utils.logger import setup_logger

# 导入配置工具
try:
    from backend.config import get_base_config
except ImportError:
    from config import get_base_config

# 导入数据库工具
try:
    from backend.utils.db import query
except ImportError:
    from utils.db import query

logger = setup_logger(
    name="AdGuardClient",
    log_file="/opt/parent-control/service/logs/adguard_client.log",
    level=logging.WARNING,
    keep_days=7
)

class AdGuardClient:
    """Read-only client for AdGuard Home Stats"""
    def __init__(self):
        self._refresh_config()
        self._stats_cache = None
        self._stats_cache_time = None
        self.CACHE_TTL = 10 # 缓存 10 秒
        
    def _refresh_config(self):
        """刷新内存中的配置"""
        global_conf = get_base_config()
        self.adguard_conf = global_conf.get('ADGUARD', {})
        self.server_conf = self.adguard_conf.get('SERVER', {})
        
        self.base_url = self.server_conf.get('URL', '').rstrip('/')
        self.auth = (self.server_conf.get('USER'), self.server_conf.get('PASSWORD'))

    # --- AGH 接口通信 ---
    def get_remote_stats(self, force=False):
        self._refresh_config()
        
        # 缓存逻辑
        now = datetime.now()
        if not force and self._stats_cache and self._stats_cache_time:
            if (now - self._stats_cache_time).total_seconds() < self.CACHE_TTL:
                return self._stats_cache

        try:
            resp = requests.get(f"{self.base_url}/control/stats", auth=self.auth, timeout=5)
            if resp.status_code == 200:
                self._stats_cache = resp.json()
                self._stats_cache_time = now
                return self._stats_cache
            return None
        except Exception as e:
            logger.error(f"❌ 获取统计失败: {str(e)}")
            return None

    def get_device_logs_by_ip(self, ip_or_mac, limit=100):
        """从 AGH 获取原始日志并统一字段名为 query_type"""
        if not ip_or_mac: return []
        params = {"search": ip_or_mac, "limit": limit, "response_status": "all"}
        try:
            resp = requests.get(f"{self.base_url}/control/querylog", auth=self.auth, params=params, timeout=10)
            if resp.status_code != 200: return []
            
            raw_data = resp.json().get('data', [])
            processed_logs = []
            for item in raw_data:
                reason = item.get('reason', 'NotFilteredNotFound')
                # 判定拦截状态
                is_blocked = reason not in ['NotFilteredNotFound', 'AllowList', 'Rewrite']
                
                status_text = reason
                if is_blocked and item.get('rules'):
                    rule_id = item['rules'][0].get('filter_list_id')
                    status_text = f"{reason} (规则ID: {rule_id})"

                # 统一使用 query_type
                q_type = item.get('question', {}).get('type', 'A')
                processed_logs.append({
                    "time": item.get('time'),
                    "domain": item.get('question', {}).get('name'),
                    "status": status_text,
                    "is_blocked": is_blocked,
                    "query_type": q_type 
                })
            return processed_logs
        except Exception as e:
            logger.error(f"❌ 获取日志失败 ({ip_or_mac}): {str(e)}")
            return []

    # 获取最近的查询和拦截总数
    def get_stats_to_home(self):
        raw_stats = self.get_remote_stats()
        if not raw_stats:
            return {"summary": {"total": 0, "blocked": 0}}
            
        # 适配元组返回
        if isinstance(raw_stats, tuple):
            raw_stats = raw_stats[0]

        num_dns_queries = raw_stats.get('num_dns_queries') or 0
        num_blocked_queries = raw_stats.get('num_blocked_filtering') or 0
        return {
            "summary" : {
                "total": num_dns_queries,
                "blocked": num_blocked_queries
            }            
        }

    # --- 数据处理逻辑 ---
    def get_processed_overview(self):
        # 1. 获取 AGH 原始统计数据
        raw_stats = self.get_remote_stats()
        if not raw_stats:
            logger.error("无法从 AdGuard Home 获取统计数据")
            return None

        # --- 核心修复：处理元组和嵌套结构 ---
        # 如果返回的是 ( {'summary':...}, ) 这种元组，先取第一个
        if isinstance(raw_stats, tuple):
            raw_stats = raw_stats[0]
        
        # 提取 summary 部分，如果 summary 键不存在，则 fallback 到自身
        summary_node = raw_stats.get('summary', {})
        
        # 2. 获取数据库基准设备
        sql = """
            SELECT d.id, d.name, d.ip, d.type, g.name as group_key, g.remark as group_name 
            FROM devices d 
            LEFT JOIN groups g ON d.groups_id = g.id
        """
        db_devices = query(sql) 

        # 3. 处理 Top Clients
        exclude_ips = ['127.0.0.1', '::1', 'localhost']
        agh_counts = {}
        for c in raw_stats.get('top_clients', []):
            if not c: continue
            # 适配断点中看到的字典格式 {'ip': count} 或 {ip: count}
            ip = (c.get('ip') or c.get('client') or list(c.keys())[0]).lower()
            if ip in exclude_ips:
                continue
            count = c.get('count') if 'count' in c else list(c.values())[0]
            agh_counts[ip] = count

        # 4 & 5. 组合设备数据 (数据库设备 + 未记录设备)
        device_ranking = []
        db_ips = set()
        
        for d in db_devices:
            ip_str = str(d['ip']).lower()
            db_ips.add(ip_str)
            device_ranking.append({
                "device_id": d['id'],
                "name": d['name'],
                "group_key": d['group_key'],
                "group_name": d['group_name'] or "未分配",
                "ip": d['ip'],
                "type": d['type'],
                "query_count": agh_counts.get(ip_str, 0)
            })

        for ip, count in agh_counts.items():
            if ip not in db_ips:
                device_ranking.append({
                    "device_id": None,
                    "name": f"未记录({ip})",
                    "group_key": "unknown",
                    "group_name": "未知分组",
                    "ip": ip,
                    "query_count": count
                })

        # 6. 处理被拦截域名排行 (适配断点数据: [{'domain': count}, ...])
        blocked_ranking = []
        for b in raw_stats.get('top_blocked_domains', []):
            if not b: continue
            domain = b.get('domain') or list(b.keys())[0]
            count = b.get('count') if 'count' in b else list(b.values())[0]
            blocked_ranking.append({"domain": domain, "count": count})

        # 7. 组装最终结果
        # 优先从 summary 节点拿数据，拿不到再从外层尝试，最后兜底用排行榜总和
        total_queries = summary_node.get('total') or raw_stats.get('num_dns_queries') or 0
        blocked_queries = summary_node.get('blocked') or raw_stats.get('num_blocked_filtering') or 0
        
        # 兜底逻辑：如果 summary 显示为 0 但排行榜有数，说明 API 汇总延迟，直接用排行榜求和
        if blocked_queries == 0 and blocked_ranking:
            blocked_queries = sum(item['count'] for item in blocked_ranking)

        return {
            "summary": {
                "total": total_queries,
                "blocked": blocked_queries
            },
            "device_ranking": sorted(device_ranking, key=lambda x: x['query_count'], reverse=True),
            "blocked_ranking": sorted(blocked_ranking, key=lambda x: x['count'], reverse=True)
        }

    # --- 同步逻辑 (联动 C++ Service 或 直接调用 AGH) ---
    def fetch_remote_to_local(self):
        """
        从 AdGuard Home 获取所有已配置的客户端，并同步其信息。
        将获取到的数据更新到 backend/config/base.conf 的 TEMPLATE 中。
        """
        self._refresh_config()
        try:
            resp = requests.get(f"{self.base_url}/control/clients", auth=self.auth, timeout=10)
            if resp.status_code != 200:
                logger.error(f"Failed to fetch AGH clients: {resp.status_code}")
                return False
            
            data = resp.json()
            clients = data.get('clients', [])
            
            # 1. 整理远程数据为以名称为 Key 的字典
            remote_templates = {}
            for c in clients:
                name = c.get('name')
                if not name: continue
                remote_templates[name] = c

            if not remote_templates:
                logger.warning("No clients found in AdGuard Home to pull.")
                return True

            # 2. 读取并更新本地 base.conf
            try:
                from backend.config import get_base_config, save_config
            except ImportError:
                from config import get_base_config, save_config
                
            current_conf = get_base_config()
            
            if "ADGUARD" not in current_conf:
                current_conf["ADGUARD"] = {}
            
            if "TEMPLATE" not in current_conf["ADGUARD"]:
                current_conf["ADGUARD"]["TEMPLATE"] = {}
            
            # 合并策略：以远程为准更新
            for name, cfg in remote_templates.items():
                current_conf["ADGUARD"]["TEMPLATE"][name] = cfg
            
            # 3. 保存到文件
            if save_config(current_conf):
                logger.info(f"Successfully pulled {len(remote_templates)} clients and updated base.conf TEMPLATE.")
                return True
            else:
                logger.error("Failed to save base.conf after pulling remote config.")
                return False

        except Exception as e:
            logger.error(f"fetch_remote_to_local failed: {str(e)}")
            return False

    def get_agh_clients(self):
        """获取 AGH 中现有的客户端列表 (用于判断添加还是更新)"""
        self._refresh_config()
        try:
            resp = requests.get(f"{self.base_url}/control/clients", auth=self.auth, timeout=10)
            if resp.status_code == 200:
                clients = resp.json().get('clients', [])
                return {c['name']: c for c in clients if 'name' in c}
            return {}
        except Exception:
            return {}

    def _push_client_to_agh(self, client_data, is_update=False):
        """推送客户端配置到 AGH"""
        try:
            headers = {"Content-Type": "application/json"}
            
            # 确保 ids 是列表且不包含空值
            if 'ids' in client_data:
                client_data['ids'] = [str(x) for x in client_data['ids'] if x]
                
            if is_update:
                url = f"{self.base_url}/control/clients/update"
                payload = {
                    "name": client_data['name'],
                    "data": client_data
                }
            else:
                url = f"{self.base_url}/control/clients/add"
                payload = client_data
                
            resp = requests.post(url, auth=self.auth, json=payload, headers=headers, timeout=10)
            
            if resp.status_code == 200:
                return True, "OK"
            else:
                # 尝试解析错误信息
                try:
                    err = resp.json()
                    msg = err.get('message') or resp.text
                except:
                    msg = resp.text
                return False, f"Status {resp.status_code}: {msg}"
                
        except Exception as e:
            return False, str(e)

    def sync_all_groups(self):
        """
        全量同步：将本地分组和设备清单推送到 AdGuard Home (通过 C++ 服务)
        1. 从数据库查询每个分组的设备 MAC 地址
        2. 更新 base.conf 模板中的 ids
        3. 保存 base.conf
        4. 调用 C++ service 推送到 AdGuard
        """
        from backend.utils.pc_interface import PCInterface
        
        try:
            # 1. 读取配置
            try:
                from backend.config import get_base_config, save_config
            except ImportError:
                from config import get_base_config, save_config
                
            current_conf = get_base_config()
            if "ADGUARD" not in current_conf: current_conf["ADGUARD"] = {}
            if "TEMPLATE" not in current_conf["ADGUARD"]: current_conf["ADGUARD"]["TEMPLATE"] = {}
                
            templates = current_conf["ADGUARD"]["TEMPLATE"]
            
            # 2. 从数据库查询设备
            try:
                from backend.utils.db import query
            except ImportError:
                from utils.db import query
            
            # 获取所有设备分组 (从 groups 表获取)
            # 优先使用 remark 作为显示名称，name 作为系统标识
            group_rows = query("SELECT id, name, remark FROM groups WHERE name IS NOT NULL AND name != ''")
            
            for group in group_rows:
                client_name = group['name'] 
                group_id = group['id']
                
                # 获取该组下的设备 MAC
                mac_rows = query("SELECT mac FROM devices WHERE groups_id = ?", (group_id,))
                mac_addresses = [row['mac'] for row in mac_rows if row['mac']]
                
                # 初始化或更新模板
                if client_name not in templates:
                     templates[client_name] = {
                         "name": client_name,
                         "ids": [],
                         "filtering_enabled": True, 
                         "parental_enabled": False,
                         "safebrowsing_enabled": False,
                         "safe_search": {"enabled": False},
                         "blocked_services": []
                     }
                
                templates[client_name]['ids'] = mac_addresses
                logger.info(f"Updated Group {client_name}: {len(mac_addresses)} devices")
            
            # 3. 保存更新后的 base.conf
            current_conf['ADGUARD']['TEMPLATE'] = templates
            if save_config(current_conf):
                logger.info("base.conf updated successfully")
            else:
                logger.error("Failed to save base.conf")
                return False
            
            # 4. 调用 C++ service 推送到 AdGuard
            return PCInterface.AGH.sync()
            
        except Exception as e:
            logger.error(f"sync_all_groups failed: {str(e)}")
            return False, str(e)

    def sync_single_group(self, group_id):
        """
        同步单个分组（暂通过全量同步替代，保证一致性）。
        """
        return self.sync_all_groups()
