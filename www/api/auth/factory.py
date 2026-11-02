from api.auth.keycloak_ropc import KeycloakROPC
from api.auth.keycloak import KeycloakProvider
from api.auth.entra import EntraIDProvider
from api.auth.okta import OktaProvider
from api.auth.auth0 import Auth0Provider
from api.auth.local import LocalProvider
from api import auth_config as config

class ProviderFactory:
    @staticmethod
    def create(provider_name, redirect_enabled, config):
        if provider_name == "keycloak":
            if redirect_enabled:
                return KeycloakProvider(
                    frontend_base_url=config.PROVIDER_FRONTEND_BASE_URL,
                    backend_base_url=config.PROVIDER_BACKEND_BASE_URL,
                    realm=config.PROVIDER_REALM,
                    client_id=config.PROVIDER_CLIENT_ID,
                    client_secret=config.PROVIDER_CLIENT_SECRET,
                    redirect_uri=config.PROVIDER_REDIRECT_URI
                )
            else:
                return KeycloakROPC(
                    token_url=config.KEYCLOAK_TOKEN_URL,
                    userinfo_url=config.KEYCLOAK_USERINFO_URL,
                    client_id=config.KEYCLOAK_CLIENT_ID,
                    client_secret=config.KEYCLOAK_CLIENT_SECRET
                )

        if provider_name == "entra":
            return EntraIDProvider(
                tenant_id=config.PROVIDER_TENANT_ID,
                client_id=config.PROVIDER_CLIENT_ID,
                client_secret=config.PROVIDER_CLIENT_SECRET,
                redirect_uri=config.PROVIDER_REDIRECT_URI
            )

        if provider_name == "okta":
            return OktaProvider(
                domain=config.OKTA_DOMAIN,
                client_id=config.PROVIDER_CLIENT_ID,
                client_secret=config.PROVIDER_CLIENT_SECRET,
                redirect_uri=config.PROVIDER_REDIRECT_URI
            )

        if provider_name == "auth0":
            return Auth0Provider(
                domain=config.AUTH0_DOMAIN,
                client_id=config.PROVIDER_CLIENT_ID,
                client_secret=config.PROVIDER_CLIENT_SECRET,
                redirect_uri=config.PROVIDER_REDIRECT_URI
            )

        if provider_name == "local":
            #return LocalProvider()
            return LocalProvider(users_file=config.LOCAL_USERS)

        raise ValueError(f"Unknown provider: {provider_name}")
