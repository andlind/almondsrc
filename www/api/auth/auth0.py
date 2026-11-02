from .oauth_code import OAuthCodeFlow

class Auth0Provider(OAuthCodeFlow):
    name = "auth0"
    def __init__(self, domain, client_id, client_secret, redirect_uri):
        self.domain = domain
        auth_url = f"https://{domain}/authorize"
        token_url = f"https://{domain}/oauth/token"

        super().__init__(
            auth_url=auth_url,
            token_url=token_url,
            client_id=client_id,
            client_secret=client_secret,
            redirect_uri=redirect_uri
        )

    def logout_url(self, redirect_to="/"):
        return (
            f"https://{self.domain}/v2/logout"
            f"?returnTo={redirect_to}"
            f"&client_id={self.client_id}"
        )
   
    def get_scope(self):
        return "openid offline_access" 
