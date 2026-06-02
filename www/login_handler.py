#!/usr/bin/python3
"""
Login Handler Module - Cleaner authentication and authorization logic
Separates concerns: authentication, role extraction, and session management
"""

from functools import wraps
from jose import jwt
from flask import session, abort
import logging

logger = logging.getLogger(__name__)

# Optional: Custom role mapping for providers that don't include roles in token
# Format: {"username": ["role1", "role2"], ...}
# Only used if no roles are found in the token
CUSTOM_ROLE_MAPPING = {}

def set_custom_role_mapping(mapping):
    """
    Set custom username-to-roles mapping for providers without role support.
    
    Example:
        set_custom_role_mapping({
            "admin_user": ["admin"],
            "operator_user": ["operator", "viewer"],
            "viewer_user": ["viewer"],
        })
    """
    global CUSTOM_ROLE_MAPPING
    CUSTOM_ROLE_MAPPING = mapping
    logger.info(f"Custom role mapping configured for {len(mapping)} users")


# ============================================================================
# ROLE MANAGEMENT
# ============================================================================

def extract_roles_from_token(access_token):
    """
    Extract all roles (realm + client roles) from a JWT access token.
    
    Args:
        access_token (str): JWT access token from OAuth provider
        
    Returns:
        list: List of role names, or empty list if token is invalid
    """
    try:
        decoded = jwt.get_unverified_claims(access_token)
        roles = set()

        logger.debug(f"[extract_roles] Full token claims keys: {decoded.keys()}")
        
        # Realm roles (Keycloak realm-level roles)
        realm_roles = decoded.get("realm_access", {}).get("roles", [])
        logger.debug(f"[extract_roles] Realm roles: {realm_roles}")
        roles.update(realm_roles)

        # Client roles (application-specific roles)
        resource_access = decoded.get("resource_access", {})
        logger.debug(f"[extract_roles] Resource access clients: {resource_access.keys()}")
        for client, data in resource_access.items():
            client_roles = data.get("roles", [])
            logger.debug(f"[extract_roles] Client '{client}' roles: {client_roles}")
            roles.update(client_roles)

        # Check for alternate role claim names (some providers use different names)
        if not roles:
            # Check direct "roles" claim
            if "roles" in decoded:
                roles.update(decoded.get("roles", []))
                logger.debug(f"[extract_roles] Found direct 'roles' claim: {decoded['roles']}")
            
            # Check "groups" claim
            if "groups" in decoded:
                roles.update(decoded.get("groups", []))
                logger.debug(f"[extract_roles] Found 'groups' claim: {decoded['groups']}")

        logger.debug(f"[extract_roles] Final extracted roles: {sorted(list(roles))}")
        return sorted(list(roles))
    except Exception as e:
        logger.warning(f"[extract_roles] Failed to extract roles from token: {e}")
        logger.debug(f"[extract_roles] Error details: {type(e).__name__}: {e}", exc_info=True)
        return []


def user_has_role(required_role):
    """
    Check if the current user has a specific role.
    
    Args:
        required_role (str): Role name to check
        
    Returns:
        bool: True if user has the role, False otherwise
    """
    user = session.get("user", {})
    user_roles = user.get("roles", [])
    return required_role in user_roles


def user_has_any_role(required_roles):
    """
    Check if the current user has any of the specified roles.
    
    Args:
        required_roles (list): List of role names to check
        
    Returns:
        bool: True if user has at least one of the roles
    """
    user = session.get("user", {})
    user_roles = user.get("roles", [])
    return any(role in user_roles for role in required_roles)


def get_user_roles():
    """
    Get all roles for the current user.
    
    Returns:
        list: List of role names for current user, or empty list if not logged in
    """
    user = session.get("user", {})
    return user.get("roles", [])


def is_admin(user_dict):
    """
    Check if user has admin rights (has 'admin' role).
    
    Args:
        user_dict (dict): User dictionary from session
        
    Returns:
        bool: True if user is admin
    """
    roles = user_dict.get("roles", [])
    return "admin" in roles or "realm_admin" in roles


# ============================================================================
# AUTHENTICATION LOGIC
# ============================================================================

def handle_oauth_login(provider, token_data, provider_name):
    """
    Handle OAuth/external provider login and create session.
    
    Args:
        provider: Auth provider instance
        token_data (dict): Token response from provider
        provider_name (str): Name of the auth provider (keycloak, entra, okta, etc.)
        
    Returns:
        dict: User session dictionary or None if login failed
    """
    if not token_data:
        logger.warning(f"No token data from provider '{provider_name}'")
        return None

    try:
        access_token = token_data.get("access_token")
        id_token = token_data.get("id_token")
        roles = token_data.get("roles") or []

        logger.debug(f"[{provider_name}] Token data keys: {token_data.keys()}")
        logger.debug(f"[{provider_name}] Has access_token: {bool(access_token)}")
        logger.debug(f"[{provider_name}] Has id_token: {bool(id_token)}")
        logger.debug(f"[{provider_name}] Initial roles from token_data: {roles}")
        
        # Log access token structure
        if access_token:
            try:
                access_token_claims = jwt.get_unverified_claims(access_token)
                logger.debug(f"[{provider_name}] Access token keys: {access_token_claims.keys()}")
                logger.debug(f"[{provider_name}] Access token claims: {access_token_claims}")
            except Exception as e:
                logger.warning(f"[{provider_name}] Failed to parse access token: {e}")

        # Extract user info from userinfo endpoint if available
        userinfo = {}
        if hasattr(provider, 'get_userinfo') and access_token:
            try:
                userinfo = provider.get_userinfo(access_token) or {}
                logger.debug(f"[{provider_name}] Userinfo retrieved from endpoint")
                logger.debug(f"[{provider_name}] Userinfo keys: {userinfo.keys()}")
            except NotImplementedError:
                logger.debug(f"[{provider_name}] Userinfo endpoint not implemented, will use token claims")
            except Exception as e:
                logger.warning(f"[{provider_name}] Failed to get userinfo: {e}")
        
        # Extract claims from ID token
        id_token_claims = {}
        if id_token:
            try:
                id_token_claims = jwt.get_unverified_claims(id_token)
                logger.debug(f"[{provider_name}] ID token claims extracted")
                logger.debug(f"[{provider_name}] ID token keys: {id_token_claims.keys()}")
                logger.debug(f"[{provider_name}] ID token claims: {id_token_claims}")
            except Exception as e:
                logger.warning(f"[{provider_name}] Failed to extract ID token claims: {e}")
        
        # Try to extract username from various sources (priority order)
        username = (
            token_data.get("username") or
            userinfo.get("preferred_username") or
            id_token_claims.get("preferred_username") or
            userinfo.get("email") or
            id_token_claims.get("email") or
            userinfo.get("name") or
            id_token_claims.get("name") or
            userinfo.get("sub") or
            id_token_claims.get("sub")
        )

        if not username:
            logger.error(
                f"[{provider_name}] Could not determine username. "
                f"Token: username={token_data.get('username')}, "
                f"Userinfo: preferred_username={userinfo.get('preferred_username')}, email={userinfo.get('email')}, "
                f"ID Token: preferred_username={id_token_claims.get('preferred_username')}, email={id_token_claims.get('email')}, sub={id_token_claims.get('sub')}"
            )
            return None

        # Extract roles from access token or id token
        if not roles and access_token:
            roles = extract_roles_from_token(access_token)
        if not roles and id_token:
            roles = extract_roles_from_token(id_token)

        logger.debug(f"[{provider_name}] Extracted roles after claim parsing: {roles}")
        
        # If no roles found in token, try custom mapping
        if not roles and username in CUSTOM_ROLE_MAPPING:
            roles = CUSTOM_ROLE_MAPPING[username]
            logger.info(f"[{provider_name}] Assigned custom roles for user '{username}': {roles}")

        # Local provider fallback support
        if not roles and provider_name == "local":
            roles = ["admin"]
            logger.info(f"[{provider_name}] Assigned default local role to '{username}'")

        if not roles and provider_name != "local":
            logger.warning(
                f"[{provider_name}] No roles found in token or custom mapping for '{username}'. "
                f"Keycloak must be configured to include roles in the token. "
                f"See KEYCLOAK_ROLE_CONFIGURATION.md for setup."
            )

        user_session = {
            "username": username,
            "provider": provider_name,
            "source": "external",
            "roles": roles,
            "id_token": id_token,
            "access_token": access_token,
        }

        logger.info(f"User '{username}' logged in via {provider_name}. Roles: {roles}")
        return user_session

    except Exception as e:
        logger.error(f"[{provider_name}] Error processing OAuth login: {type(e).__name__}: {e}", exc_info=True)
        return None


def handle_local_login(username, roles=None):
    """
    Handle local authentication and create session.
    
    Args:
        username (str): Local username
        roles (list, optional): Roles assigned to the local user
        
    Returns:
        dict: User session dictionary
    """
    if roles is None:
        roles = ["admin"]

    user_session = {
        "username": username,
        "provider": "local",
        "source": "local",
        "roles": roles,
    }

    logger.info(f"User '{username}' logged in locally")
    return user_session


def create_session(user_dict, tokens_dict=None):
    """
    Create Flask session for authenticated user.
    
    Args:
        user_dict (dict): User information dictionary
        tokens_dict (dict, optional): OAuth tokens (access_token, refresh_token, id_token)
    """
    session["login"] = "true"
    session["user"] = user_dict
    
    if tokens_dict:
        session["tokens"] = tokens_dict

    logger.debug(f"Session created for user '{user_dict.get('username')}'")


def clear_session():
    """Clear user session."""
    username = session.get("user", {}).get("username", "unknown")
    session.pop("login", None)
    session.pop("user", None)
    session.pop("tokens", None)
    session.clear()
    logger.info(f"Session cleared for user '{username}'")


def is_logged_in():
    """
    Check if a user is currently logged in.
    
    Returns:
        bool: True if user has valid session
    """
    return session.get("login") == "true" and "user" in session


def get_current_user():
    """
    Get current logged-in user information.
    
    Returns:
        dict: User dictionary or None if not logged in
    """
    if is_logged_in():
        return session.get("user")
    return None


# ============================================================================
# AUTHORIZATION DECORATORS
# ============================================================================

def require_login(f):
    """
    Decorator to require user login for a route.
    Returns 403 if user is not logged in.
    
    Usage:
        @require_login
        def my_route():
            ...
    """
    @wraps(f)
    def decorated_function(*args, **kwargs):
        if not is_logged_in():
            logger.warning("Unauthorized access attempt - user not logged in")
            abort(403)
        return f(*args, **kwargs)
    return decorated_function


def require_role(required_role):
    """
    Decorator to require a specific role for a route.
    Returns 403 if user doesn't have the required role.
    
    Usage:
        @require_role('admin')
        def admin_only_route():
            ...
    """
    def decorator(f):
        @wraps(f)
        def decorated_function(*args, **kwargs):
            if not is_logged_in():
                logger.warning("Unauthorized access attempt - user not logged in")
                abort(403)
            
            if not user_has_role(required_role):
                user = get_current_user()
                logger.warning(f"Unauthorized access - user '{user.get('username')}' lacks required role '{required_role}'")
                abort(403)
            
            return f(*args, **kwargs)
        return decorated_function
    return decorator


def require_any_role(*required_roles):
    """
    Decorator to require any of multiple roles for a route.
    Returns 403 if user doesn't have at least one of the required roles.
    
    Usage:
        @require_any_role('admin', 'operator')
        def management_route():
            ...
    """
    def decorator(f):
        @wraps(f)
        def decorated_function(*args, **kwargs):
            if not is_logged_in():
                logger.warning("Unauthorized access attempt - user not logged in")
                abort(403)
            
            if not user_has_any_role(list(required_roles)):
                user = get_current_user()
                logger.warning(f"Unauthorized access - user '{user.get('username')}' lacks any of required roles {required_roles}")
                abort(403)
            
            return f(*args, **kwargs)
        return decorated_function
    return decorator


def require_admin(f):
    """
    Decorator to require admin role for a route.
    Returns 403 if user is not admin.
    
    Usage:
        @require_admin
        def admin_panel():
            ...
    """
    @wraps(f)
    def decorated_function(*args, **kwargs):
        if not is_logged_in():
            logger.warning("Unauthorized access attempt - user not logged in")
            abort(403)
        
        user = get_current_user()
        if not is_admin(user):
            logger.warning(f"Unauthorized access - user '{user.get('username')}' is not admin")
            abort(403)
        
        return f(*args, **kwargs)
    return decorated_function


def check_authorization(required_roles):
    """
    Check if user has authorization for an action (can be used in inline code).
    Raises 403 if not authorized.
    
    Args:
        required_roles (list or str): Single role or list of roles required
        
    Returns:
        bool: True if authorized
        
    Raises:
        HTTPException: 403 Forbidden if not authorized
    """
    if not is_logged_in():
        logger.warning("Authorization check failed - user not logged in")
        abort(403)
    
    if isinstance(required_roles, str):
        required_roles = [required_roles]
    
    if not user_has_any_role(required_roles):
        user = get_current_user()
        logger.warning(f"Authorization check failed - user '{user.get('username')}' lacks required roles")
        abort(403)
    
    return True
