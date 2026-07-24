import json
import redis
import os
import sys

# Configure paths
# In remote (OpenWrt), this script is in /opt/parent-control/backend/scripts/
CONFIG_FILE = "/opt/parent-control/backend/config/base.conf"
# We now sync i18n files to backend/i18n/locales/
BACKEND_DIR = "/opt/parent-control/backend"
I18N_DIR = os.path.join(BACKEND_DIR, "i18n/locales")

def load_config():
    if os.path.exists(CONFIG_FILE):
        try:
            with open(CONFIG_FILE, 'r') as f:
                return json.load(f)
        except Exception as e:
            print(f"Error parsing JSON config: {e}")
    return {}

def sync_locales():
    full_config = load_config()
    
    # Redis configuration from base.conf
    redis_conf = full_config.get("REDIS", {})
    redis_host = redis_conf.get("HOST", "127.0.0.1")
    redis_port = int(redis_conf.get("PORT", 6379))
    redis_db = int(redis_conf.get("DB", 0))
    redis_pass = redis_conf.get("PASSWORD", None)

    try:
        r = redis.Redis(host=redis_host, port=redis_port, db=redis_db, password=redis_pass, decode_responses=True)
        r.ping()
        print(f"Connected to Redis at {redis_host}:{redis_port}")
    except Exception as e:
        print(f"Error connecting to Redis: {e}")
        sys.exit(1)

    locales = ["zh", "en"]
    for lang in locales:
        file_path = os.path.join(I18N_DIR, f"{lang}.json")
        
        if not os.path.exists(file_path):
             print(f"Warning: {file_path} not found. Skipping.")
             continue
        
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
                r.set(f"pc:i18n:{lang}", json.dumps(data, ensure_ascii=False))
                print(f"Successfully synced {file_path} to Redis key 'pc:i18n:{lang}'")
        except Exception as e:
            print(f"Error processing {file_path}: {e}")

if __name__ == "__main__":
    sync_locales()
