import json
import os
from utils.redis_client import rc

# Base directory for the backend to locate locales
I18N_DIR = "/opt/parent-control/backend/i18n/locales"

def sync_translations_to_redis():
    """
    Scans the locales directory and pushes all JSON translations to Redis.
    Typically called on application startup.
    """
    if not os.path.exists(I18N_DIR):
        print(f"I18n directory not found: {I18N_DIR}")
        return

    locales = ["zh", "en"]
    count = 0
    for lang in locales:
        file_path = os.path.join(I18N_DIR, f"{lang}.json")
        if os.path.exists(file_path):
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                    cache_key = f"pc:i18n:{lang}"
                    rc.set(cache_key, json.dumps(data, ensure_ascii=False))
                    count += 1
            except Exception as e:
                print(f"Failed to sync {lang}.json to Redis: {e}")
    
    if count > 0:
        print(f"Successfully synced {count} i18n files to Redis on startup.")
