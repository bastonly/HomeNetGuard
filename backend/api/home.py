import os
from flask import Blueprint, send_from_directory

bp = Blueprint('home', __name__)

BASE_DIR = os.path.dirname(os.path.dirname(os.path.dirname(__file__)))  # /opt/parent-control
DIST_DIR = os.path.join(BASE_DIR, 'frontend', 'dist')

@bp.route('/assets/<path:path>')

def send_assets(path):
    return send_from_directory(os.path.join(DIST_DIR, 'assets'), path, max_age=3600)

@bp.route('/', defaults={'path': ''})
@bp.route('/<path:path>')
def send_index(path):
     # 如果是 API 请求，直接 404，让 Flask 继续匹配别的蓝图
    if path.startswith('api/'):
        return '', 404
    
    if path:
        file_path = os.path.join(DIST_DIR, path)
        if os.path.isfile(file_path):
            return send_from_directory(DIST_DIR, path, max_age=3600)

    return send_from_directory(DIST_DIR, 'index.html', max_age=3600)

