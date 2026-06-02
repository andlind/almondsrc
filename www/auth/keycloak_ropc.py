import requests
from .base import AuthProvider

class KeycloakROPC(AuthProvider):
    name = "keycloak_ropc"
    def __init__(self, token_url, userinfo_url, client_id, client_secret):
        self.token_url = token_url
        self.userinfo_url = userinfo_url
        self.client_id = client_id
        self.client_secret = client_secret

    def authenticate(self, username, password):
        data = {
            "grant_type": "password",
            "client_id": self.client_id,
            "client_secret": self.client_secret,
            "username": username,
            "password": password
        }

        resp = requests.post(self.token_url, data=data)
        #print("Status:", resp.status_code)
        #print("Body:", resp.text)
        if resp.status_code == 200:
            return resp.json()

        return None

    def logout_url(self, redirect_to="/", id_token=None):
        # ROPC has no server-side session to log out from
        return redirect_to
    def get_userinfo(self, token):
        headers = {
            "Authorization": f"Bearer {token}"
        }
        resp = requests.get(self.userinfo_url, headers=headers)
        if resp.status_code == 200:
            return resp.json()
        return {}
