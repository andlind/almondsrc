import time
import jwt

def is_expired(token: str, leeway: int = 30):
    """
    Returns True if the token is expired or will expire within `leeway` seconds.
    """
    try:
        payload = jwt.decode(token, options={"verify_signature": False})
        return payload["exp"] < time.time() + leeway
    except Exception:
        return True

def ensure_fresh_tokens(provider, tokens: dict):
    """
    Ensures access_token is valid. If expired, refreshes it.
    Returns updated tokens or None if refresh failed.
    """
    access_token = tokens.get("access_token")
    refresh_token = tokens.get("refresh_token")

    if not access_token or not refresh_token:
        return None

    if not is_expired(access_token):
        return tokens  # still valid

    # Access token expired → refresh
    new_tokens = provider.refresh(refresh_token)

    if not new_tokens:
        return None  # refresh failed

    return new_tokens
