from venv import logger

class AuthService:
    def __init__(self, provider):
        self.provider = provider

    def login(self, **kwargs):
        try:
            token_data = self.provider.authenticate(**kwargs)
        except Exception as e:
            print ("Provider authentication failed:" , e)
            logger.warning("Failed login with provider.")
            return None
        if not token_data:
            logger.warning("Failed getting token data from provider.")
            return None
        
        logger.info("User logged in with provider.")
        return {
            "access_token": token_data.get("access_token"),
            "refresh_token": token_data.get("refresh_token"),
            "expires_in": token_data.get("expires_in"),
            "provider": self.provider.__class__.__name__
        }
