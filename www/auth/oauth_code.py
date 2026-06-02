import base64
import hashlib
import os
import requests
from urllib.parse import quote
from .base import AuthProvider

class OAuthCodeFlow(AuthProvider):
    def __init__(self, auth_url, token_url, client_id, client_secret, redirect_uri):
        if redirect_uri is None:
            raise ValueError("redirect_uri can not be none.")
        self.auth_url = auth_url
        self.token_url = token_url
        self.client_id = client_id
        self.client_secret = client_secret
        self.redirect_uri = redirect_uri

    def generate_pkce(self):
        verifier = base64.urlsafe_b64encode(os.urandom(40)).rstrip(b'=').decode()
        challenge = base64.urlsafe_b64encode(
            hashlib.sha256(verifier.encode()).digest()
        ).rstrip(b'=').decode()
        return verifier, challenge

    def build_auth_redirect(self, state, challenge):
        #print("redirect_uri:", self.redirect_uri, type(self.redirect_uri))
        return (
            f"{self.auth_url}"
            f"?response_type=code"
            f"&client_id={self.client_id}"
            f"&redirect_uri={quote(self.redirect_uri, safe='')}"
            f"&scope={quote(self.get_scope())}"
            f"&state={state}"
            f"&code_challenge={challenge}"
            f"&code_challenge_method=S256"
        )

    def authenticate(self, **kwargs):
        code = kwargs.get("code")
        verifier = kwargs.get("verifier")
        data = {
            "grant_type": "authorization_code",
            "code": code,
            "redirect_uri": self.redirect_uri,
            "client_id": self.client_id,
            "client_secret": self.client_secret,
            "code_verifier": verifier
        }

        resp = requests.post(self.token_url, data=data)
        if resp.status_code == 200:
            return resp.json()

        print("Token exchange failed:", resp.text)
        return None

    def refresh(self, refresh_token: str):
        """
        Exchange a refresh token for a new access token.
        Works for Keycloak and any standard OIDC provider.
        """
        data = {
            "grant_type": "refresh_token",
            "refresh_token": refresh_token,
            "client_id": self.client_id,
            "client_secret": self.client_secret,
        }

        resp = requests.post(self.token_url, data=data)

        if resp.status_code != 200:
            print("Refresh token exchange failed:", resp.text)
            return None

        return resp.json()

    def get_scope(self):
        return "openid"
