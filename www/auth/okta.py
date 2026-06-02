from .oauth_code import OAuthCodeFlow

class OktaProvider(OAuthCodeFlow):
    name = "okta"
    def __init__(self, domain, client_id, client_secret, redirect_uri):
        self.domain = domain

        auth_url = f"https://{domain}/oauth2/default/v1/authorize"
        token_url = f"https://{domain}/oauth2/default/v1/token"

        super().__init__(
            auth_url=auth_url,
            token_url=token_url,
            client_id=client_id,
            client_secret=client_secret,
            redirect_uri=redirect_uri
        )

    def logout_url(self, redirect_to="/"):
        return (
            f"https://{self.domain}/oauth2/default/v1/logout"
            f"?post_logout_redirect_uri={redirect_to}"
        )

    def get_scope(self):
        return "openid offline_access"
