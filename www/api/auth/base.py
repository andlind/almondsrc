class AuthProvider:
    """
    Base class for all authentication providers.
    Providers may implement:
      - authenticate() for ROPC (username/password)
      - get_authorization_url() for redirect-based login
      - exchange_code_for_token() for OAuth callback
      - get_userinfo() for profile lookup
    """
    name = "unknown"
    # --- ROPC (username/password) ---
    def authenticate(self, **kwargs):
        """Return token dict or None."""
        raise NotImplementedError("This provider does not support ROPC")

    # --- Redirect-based login ---
    def get_authorization_url(self, state=None):
        """Return URL for redirect login, or None if unsupported."""
        return None

    def exchange_code_for_token(self, code):
        """Exchange OAuth code for token. Return token dict or None."""
        return None

    # --- User info lookup ---
    def get_userinfo(self, token):
        """Return user info dict or None."""
        raise NotImplementedError("Userinfo not implemented for this provider")

    def logout_url(self, redirect_to="/"):
        # Default: no remote logout, just return redirect target
        return redirect_to
