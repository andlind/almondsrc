from dotenv import load_dotenv
import os

load_dotenv()

KEYCLOAK_TOKEN_URL = os.getenv("KEYCLOAK_TOKEN_URL")
KEYCLOAK_USERINFO_URL = os.getenv("KEYCLOAK_USERINFO_URL")
KEYCLOAK_CLIENT_ID = os.getenv("KEYCLOAK_CLIENT_ID")
KEYCLOAK_CLIENT_SECRET = os.getenv("KEYCLOAK_CLIENT_SECRET")
AUTH_PROVIDER_NAME = "keycloak"
LOCAL_USERS = "/etc/almond/users.conf"
PROVIDER_TOKEN_URL = "http://host.docker.internal:8089/realms/almondmonitor/protocol/openid-connect/token"
PROVIDER_FRONTEND_BASE_URL = "http://localhost:8089"
PROVIDER_BACKEND_BASE_URL = "http://host.docker.internal:8089"
PROVIDER_REALM = "almondmonitor"
PROVIDER_CLIENT_ID = "almondadmin"
PROVIDER_CLIENT_SECRET = "9eDIcYdgWZ7Uf0TBpsEfG0t9E6a07U9j"
PROVIDER_AUTH_URL = "http://localhost:8089/realms/almondmonitor/protocol/openid-connect/auth"
PROVIDER_TOKEN_URL= "http://host.docker.internal:8089/realms/almondmonitor/protocol/openid-connect/token"
PROVIDER_REDIRECT_URI = "http://localhost:8015/callback"
