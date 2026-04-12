#
# To run HowRU as WSGI HTTP Server
# for an example with gunicorn
# gunicorn --user root --bind 0.0.0.0:80 --chdir=/opt/almond/www/api --log-file /opt/almond/www/api/app.log --log-level debug wsgi:app
#
import glob
import random
import socket
import logging
import json
import flask
import time
import threading
import subprocess
import os, os.path
from venv import logger
from api.howru import app
import api.howru as howru
#from howru import app
#import howru

# Global variables to track initialization state
stop_background_thread = False
background_threads_initialized = False

def parse_line(line):
    """Parse a configuration line"""
    key, value = line.strip().split('=', 1)
    return key.strip(), value.strip()

def initialize_wsgi():
    """Initialize all background threads and configuration for WSGI mode"""
    global stop_background_thread, background_threads_initialized
    
    if background_threads_initialized:
        return
    
    # Set up logging
    logging.basicConfig(
        filename='/var/log/almond/howru.log',
        filemode='a',
        format='%(asctime)s | %(levelname)s: %(message)s',
        datefmt='%Y-%m-%d %H:%M:%S'
    )
    logger = logging.getLogger()
    logger.setLevel(logging.DEBUG)
    handler = logging.FileHandler('/var/log/almond/howru.log')
    formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
    handler.setFormatter(formatter)
    app.logger.addHandler(handler)
    app.logger.setLevel(logging.DEBUG)
    
    app.logger.info(f'Starting howru api (WSGI mode)')
    
    # Call howru's load_conf to setup all the configuration
    howru.load_conf()
    
    # Set app config
    app.config['AUTH_TYPE'] = howru.admin_auth_type
    app.config['IS_CONTAINER'] = 'true' if howru.is_container else 'false'
    app.config['AUTH2FA_P'] = 'true' if howru.persistant_2fa else 'false'
    
    app.logger.info("Configuration loaded successfully")
    
    # Start background threads
    stop_background_thread = False
    howru.stop_background_thread = False
    
    # Start config monitoring thread
    tCheck = threading.Thread(target=howru.check_config, daemon=True)
    tCheck.start()
    app.logger.info("Configuration monitoring thread started")
    
    # Start mods thread if enabled
    if howru.enable_mods:
        app.logger.info('Mods are enabled. Starting mods thread.')
        tMods = threading.Thread(target=howru.run_mods, daemon=True)
        tMods.start()
    
    # Start proxy cleaner thread if enabled
    if howru.enable_cleaner:
        app.logger.info('Proxy cleaner enabled. Starting cleaner thread.')
        tCleaner = threading.Thread(target=howru.clean_proxy_cache_loop, daemon=True)
        tCleaner.start()
    
    background_threads_initialized = True
    app.logger.info("WSGI initialization complete")

# Initialize on module import
try:
    initialize_wsgi()
except Exception as e:
    app.logger.error(f"Error during WSGI initialization: {e}", exc_info=True)

if __name__ == "__main__":
    # For direct execution (though typically gunicorn is used)
    app.run(host='0.0.0.0', port=howru.bindPort, threaded=True)
