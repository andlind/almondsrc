from .oauth_code import OAuthCodeFlow

class EntraIDProvider(OAuthCodeFlow):
    name = "entra"
    def __init__(self, tenant_id, client_id, client_secret, redirect_uri):
        self.tenant_id = tenant_id

        auth_url = (
            f"https://login.microsoftonline.com/{tenant_id}/oauth2/v2.0/authorize"
        )
        token_url = (
            f"https://login.microsoftonline.com/{tenant_id}/oauth2/v2.0/token"
        )

        super().__init__(
            auth_url=auth_url,
            token_url=token_url,
            client_id=client_id,
            client_secret=client_secret,
            redirect_uri=redirect_uri
        )

    def logout_url(self, redirect_to="/"):
        return (
            f"https://login.microsoftonline.com/{self.tenant_id}/oauth2/v2.0/logout"
            f"?post_logout_redirect_uri={redirect_to}"
        )

    def get_scope(self):
        return "openid offline_access"
