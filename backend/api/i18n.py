from flask import Blueprint, jsonify, request
from utils.redis_client import rc
import json
import os

i18n_bp = Blueprint('i18n', __name__)

# Base directory for the backend to locate locales
I18N_DIR = "/opt/parent-control/backend/i18n/locales"

@i18n_bp.route('/<lang>', methods=['GET'])
def get_translations(lang):
    """
    Fetch translations from Redis for a given language.
    If Redis is empty, try to load from file and sync to Redis.
    """
    if lang not in ['zh', 'en']:
        return jsonify({"ok": False, "msg": "Unsupported language"}), 400

    cache_key = f"pc:i18n:{lang}"
    
    # 1. Try to get from Redis
    try:
        data = rc.get(cache_key)
        if data:
            return jsonify({
                "ok": True, 
                "data": json.loads(data)
            })
    except Exception as e:
        # Redis might be down or connection failed
        pass

    # 2. Redis missed or failed, try to load from file (Auto-sync fallback)
    file_path = os.path.join(I18N_DIR, f"{lang}.json")
    if os.path.exists(file_path):
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                translations = json.load(f)
                
                # Try to feed back to Redis for next time
                try:
                    rc.set(cache_key, json.dumps(translations, ensure_ascii=False))
                except:
                    pass
                
                return jsonify({
                    "ok": True,
                    "data": translations,
                    "source": "file_sync"
                })
        except Exception as e:
            return jsonify({"ok": False, "msg": f"Error loading file: {str(e)}"}), 500

    return jsonify({"ok": False, "msg": "Translations not found in cache or file system"}), 404
