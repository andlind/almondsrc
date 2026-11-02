provider = None

def set_provider(instance):
    global provider
    provider = instance

def get_provider():
    if provider is None:
        raise RuntimeError("Auth provider has not been initialized")
    return provider
