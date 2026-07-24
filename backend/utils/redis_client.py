import redis
import json
from config import get_base_config

class RedisClient:
    _instance = None

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super(RedisClient, cls).__new__(cls)
            cls._instance._init_client()
        return cls._instance

    def _init_client(self):
        conf = get_base_config().get("REDIS", {})
        host = conf.get("HOST", "127.0.0.1")
        port = conf.get("PORT", 6379)
        password = conf.get("PASSWORD", None)
        db = conf.get("DB", 0)

        try:
            self.client = redis.Redis(
                host=host,
                port=port,
                password=password,
                db=db,
                socket_timeout=0.5,
                socket_connect_timeout=0.5,
                decode_responses=True
            )
        except Exception as e:
            print(f"Redis connection failed: {e}")
            self.client = None

    def get(self, key):
        if not self.client: return None
        try:
            return self.client.get(key)
        except:
            return None

    def set(self, key, value, ex=None):
        if not self.client: return False
        try:
            return self.client.set(key, value, ex=ex)
        except:
            return False

    def delete(self, *keys):
        if not self.client: return 0
        try:
            return self.client.delete(*keys)
        except:
            return 0


    def hget(self, name, key):
        if not self.client: return None
        try:
            return self.client.hget(name, key)
        except:
            return None

    def hgetall(self, name):
        if not self.client: return {}
        try:
            return self.client.hgetall(name)
        except:
            return {}

    def hincrby(self, name, key, amount=1):
        if not self.client: return False
        try:
            return self.client.hincrby(name, key, amount)
        except:
            return False

    def hset(self, name, key=None, value=None, mapping=None):
        if not self.client: return False
        try:
            return self.client.hset(name, key, value, mapping)
        except:
            return False

    def smembers(self, name):
        if not self.client: return set()
        try:
            return self.client.smembers(name)
        except:
            return set()

    def srem(self, name, *values):
        if not self.client: return 0
        try:
            return self.client.srem(name, *values)
        except:
            return 0


    def try_lock(self, lock_name, timeout=10):
        if not self.client: return False
        key = f"lock:{lock_name}"
        try:
            # nx=True (SET if Not eXists), ex=timeout (Expiration in seconds)
            return self.client.set(key, 1, nx=True, ex=timeout)
        except:
            return False

    def unlock(self, lock_name):
        if not self.client: return
        key = f"lock:{lock_name}"
        try:
            self.client.delete(key)
        except:
            pass

# 单例对象
rc = RedisClient()
