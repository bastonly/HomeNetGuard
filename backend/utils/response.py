from flask import jsonify
from functools import wraps

def success(data=None, msg=None):
    """
    Standard Success Response
    Usage:
      return success(rows)          -> {ok: True, data: [...]}  (if list)
      return success({'id': 1})     -> {ok: True, id: 1}        (if dict, merges)
      return success(msg='Created') -> {ok: True, msg: 'Created'}
    """
    payload = {"ok": True}
    
    if data is not None:
        if isinstance(data, list):
            # If it's a list, usually frontend expects it directly or in 'data'
            # Based on current `jsonify(rows)`, the frontend likely expects the list DIRECTLY?
            # WAIT. Existing code: `return jsonify(rows)` -> [ ... ]
            # My plan said: `{ok: True, data: ...}`.
            # IF I change the return format for lists, I BREAK THE FRONTEND.
            # I must be careful.
            
            # Pattern A: `auth.py` -> returns {ok: True, ...}
            # Pattern B: `devices.py` /list_devices -> returns `jsonify(rows)` (Array)
            
            # Conclusion: I CANNOT uniform everything to {ok: True} without changing frontend.
            # For this optimization task, I should only target the endpoints that ALREADY return {ok: True}.
            # Those that return Arrays should remain returning Arrays, OR I should not use `success()` for them yet.
            pass
        
        if isinstance(data, dict):
            payload.update(data)
        else:
            payload['data'] = data

    if msg:
        payload["msg"] = msg
        
    return jsonify(payload)

def fail(msg, code=400):
    return jsonify({"ok": False, "msg": msg}), code

def handle_errors(f):
    @wraps(f)
    def wrapper(*args, **kwargs):
        try:
            return f(*args, **kwargs)
        except Exception as e:
            # Check if headers sent? Flask handles this usually.
            error_msg = str(e)
            return jsonify({"ok": False, "msg": f"Server Error: {error_msg}"}), 500
    return wrapper
