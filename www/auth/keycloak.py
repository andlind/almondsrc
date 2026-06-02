from .oauth_code import OAuthCodeFlow
import inspect

class KeycloakProvider(OAuthCodeFlow):
    name = "keycloak"
    def __init__(self, frontend_base_url, backend_base_url, realm, client_id, client_secret, redirect_uri):
        self.frontend_base = frontend_base_url.rstrip("/")
        self.backend_base = backend_base_url.rstrip("/")
        self.realm = realm

        auth_url = (
            f"{self.frontend_base}/realms/{realm}/protocol/openid-connect/auth"
        )
        token_url = (
            f"{self.backend_base}/realms/{realm}/protocol/openid-connect/token"
        )

        super().__init__(
            auth_url=auth_url,
            token_url=token_url,
            client_id=client_id,
            client_secret=client_secret,
            redirect_uri=redirect_uri
        )

    def logout_url(self, redirect_to="/", id_token=None):
        #print(">>> USING UPDATED KEYCLOAK PROVIDER <<<")
        base = f"{self.frontend_base}/realms/{self.realm}/protocol/openid-connect/logout"

        if id_token:
            return (
                f"{base}"
                f"?id_token_hint={id_token}"
                f"&client_id={self.client_id}"
                f"&post_logout_redirect_uri={redirect_to}"
            )

        # fallback if id_token missing
        return (
            f"{base}"
            f"?client_id={self.client_id}"
            f"&post_logout_redirect_uri={redirect_to}"
        )

    def refresh_token(self, refresh_token):
        return super().refresh(refresh_token)

    def get_scope(self):
        return "openid offline_access"
