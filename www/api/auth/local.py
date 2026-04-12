import json
from werkzeug.security import check_password_hash
from .base import AuthProvider

class LocalProvider(AuthProvider):
    name = "local"
    def __init__(self, users_file="/etc/almond/users.conf"):
        self.users = self._load_users(users_file)

    def _load_users(self, path):
        users = {}
        try:
            with open(path, "r") as f:
                for line in f:
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        entry = json.loads(line)
                        users.update(entry)
                    except json.JSONDecodeError:
                        print(f"Skipping invalid JSON line in {path}: {line}")
        except FileNotFoundError:
            print(f"Local user file not found: {path}")
        return users

    def authenticate(self, **kwargs):
        username = kwargs.get("username")
        password = kwargs.get("password")

        if not username or not password:
            return None

        stored_hash = self.users.get(username)
        if not stored_hash:
            return None

        if not check_password_hash(stored_hash, password):
            return None

        # Return a token-like structure for consistency
        return {
            "username": username,
            "access_token": f"local-{username}",
            "refresh_token": None,
            "id_token": None,
            "provider": "local"
        }

    def capabilities(self):
        return {
            "ropc": True,
            "redirect": False,
            "userinfo": False
        }

    def get_userinfo(self, token):
        #return {
        #    "preferred_username": token.get("username")
        #}
        return {}

    def logout_url(self, redirect_to="/"):
        return redirect_to
