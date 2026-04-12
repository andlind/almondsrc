#!/usr/bin/python3
import secrets
import glob
import random
import socket
import logging
import json
import flask 
import time
import datetime
import threading
import subprocess
import hashlib
import re
import shutil
import os, os.path
import jwt
import matplotlib.pyplot as plt
from venv import logger
from flask import Flask, request, abort, jsonify, render_template, redirect, url_for, send_from_directory, session, make_response
from werkzeug.datastructures import MultiDict
from api.admin_page import admin_page
from api.otel.nagios_to_otel import nagios_to_otel
from api.otel.otel_exporter import export_otel_data
from api.otel.otel_exporter import CollectorConnectionError, set_otlp_endpoint
from api.auth2fa import auth_blueprint
from api.auth.provider_instance import get_provider

app = flask.Flask(__name__)
app.config["DEBUG"] = True
app.config.setdefault('IS_CONTAINER', 'false')
app.config.setdefault('AUTH_TYPE', None)
app.config.setdefault('AUTH2FA_P', 'false')
app_started = False
data = None
settings = None
bindPort = None
multi_server = False
multi_metrics = False
enable_file = False
enable_ssl = False
enable_gui = False
enable_mods = False
enable_cleaner = False
enable_otlp_exporter = False
enable_otlp_file_watch = False
enable_otlp_periodic_export = False
ausername = ''
apassword = ''
admin_auth_type='2fa'
server_list_loaded = 0
server_list = []
mods_list = []
file_found = 1
data_dir="/opt/almond/data"
export_file="/opt/almond/data/howru_export.prom"
file_name = ''
data_file = "/opt/almond/data/monitor.json"
metrics_dir="/opt/almond/data/metrics"
full_metrics_file_name="monitor.metrics"
start_page = 'api'
ssl_certificate="/opt/almond/www/api/certificate.pem"
ssl_key="/opt/almond/www/api/certificate.key"
almond_conf_file="/etc/almond/almond.conf"
howru_conf_file="/etc/almond/api.conf"
enable_scraper=False
stop_background_thread = False
run_with_wsgi=False
wsgi_init=False
is_container=False
otlp_export_interval = 60
persistant_2fa=False
show_dashboard=False
sleep_time = 5
proxy_cleaner_seconds = 360
aliases = []
valid_aliases = []

ok_quotes = ["I'm ok, thanks for asking!", "I'm all fine, hope you are too!", "I think I never felt better!", "I feel good, I knew that I would", "I feel happy from head to feet"]
warn_quotes = ["I'm so so", "I think someone should check me out", "Something is itching, scratch my back!", "I think I'm having a cold", "I'm not feeling all well"]
crit_quotes = ["I'm not fine", "I feel sick, please call the doctor", "Not good, please get a technical guru to check me out", "Code red, code red", "I have fever, an aspirin needed"]

MAX_META_READ = 64 * 1024  # read first 64 KB for metadata

current_version = '0.9.28'
__STARTED__ = datetime.datetime.now()

app.secret_key = 'BAD_SECRET_KEY'
app.register_blueprint(admin_page)
app.register_blueprint(auth_blueprint)

def file_hash(path):
    with open(path, "rb") as f:
        return hashlib.md5(f.read()).hexdigest()

def findPos(entry):
    pos = entry.find('=')
    return pos + 1

def parse_line(line):
    key, value = line.strip().split('=', 1)
    return key.strip(), value.strip()

def make_filename(hostname, datatype):
    ts = time.strftime("%Y%m%d-%H%M%S")
    safe = "".join(c for c in hostname if c.isalnum() or c in "-_.")
    if datatype == "json":
    	return os.path.join(data_dir, f"{safe}.json")
    else:
        return os.path.join(metrics_dir, f"{safe}.metrics")
    #return os.path.join(data_dir, f"{safe}-{ts}.json")

def clean_proxy_directory(path, max_age_seconds):
    global proxy_cleaner_seconds, multi_server
    global logger
    now = time.time()
    for filename in os.listdir(path):
        full_path = os.path.join(path, filename)
        if not os.path.isfile(full_path):
            continue
        file_age = now - os.path.getmtime(full_path)
        if file_age > proxy_cleaner_seconds:
            try:
                os.remove(full_path)
                logger.info(f"[proxy_cleaner] Proxy cleaner removed file: {full_path}")
            except Exception as e:
                logger.warning(f"[proxy_cleaner] Proxy cleaner failed to remove {full_path}: {e}")

def check_config():
    """Background thread to monitor the configuration file."""
    global stop_background_thread
    global almond_conf_file
    global howru_conf_file
    global sleep_time
    global logger
    use_api_conf = False
    logger.info("[Check_conf_thread] Starting...")
    howru_last_modified = 0
    threshold = 5
    almond_last_modified = os.path.getmtime(almond_conf_file)
    if (os.path.isfile(howru_conf_file)):
        use_api_conf = True
        howru_last_modified = os.path.getmtime(howru_conf_file)
    while not stop_background_thread:

        change_detected = False
        logger.info("[Check_conf_thread] Check for configuration changes in almond.conf")
        try:
            current_modified = os.path.getmtime(almond_conf_file)
            if abs(current_modified != almond_last_modified) > threshold:
                print("Config change detected. Reload config.")
                logger.info("Change of configuration detected.")
                change_detected = True
                almond_last_modified = current_modified
        except Exception as e:
            logger.error(f"Failed to access {almond_conf_file}: {e}")

        if use_api_conf:
            logger.info("[Check_conf_thread] Check for configuration changes in api.conf")
            try:
                current_modified = os.path.getmtime(howru_conf_file)
                if abs(current_modified != howru_last_modified) > threshold:
                    logger.info("[Check_conf_thread] Change in api.conf detected")
                    change_detected = True
                    howru_last_modified = current_modified
            except:
                logger.error(f"Failed to access {howru_conf_file}: {e}")
        
        if change_detected:
            logger.info("[Check_conf_thread] Reload configurations")
            change_detected = False
        time.sleep(sleep_time)  


def load_allowed_hosts(filename="allowed_hosts.txt"):
    if not os.path.exists(filename):
        print(f"⚠️ Allowed hosts file '{filename}' not found. Defaulting to allowing all hosts.")
        return None
    with open(filename) as f:
        return {line.strip() for line in f if line.strip()}

ALLOWED_HOSTS = load_allowed_hosts()

def clean_proxy_cache_loop():
    global stop_background_thread
    global proxy_cleaner_seconds, data_dir
    while not stop_background_thread:
        clean_proxy_directory(data_dir, proxy_cleaner_seconds)
        time.sleep(30)

def run_mods():
    global stop_background_thread
    global logger
    global sleep_time
    global mods_list

    mods_dir = '/opt/almond/www/api/mods/enabled'

    while not stop_background_thread:
        for x in mods_list:
            c_file = mods_dir + x
            if (os.path.isfile(c_file)):
                logger.info('Found mod ' + x)
                try:
                    mod = subprocess.Popen(c_file, stdout=subprocess.PIPE)
                    output, _ = mod.communicate()
                    rc = mod.returncode
                    if rc != 0:
                        logger.critical(f'Mod {x} returned error: {output.decode()}.')
                    else:
                        logger.info(f'Mod {x} has run successfully.')

                except FileNotFoundError:
                    logger.error('Could not find mod{x}.')
                except Exception as e:
                    logger.exception('An error occurred while running mod {x}: {str(e)}')
        time.sleep(sleep_time)

def load_aliases():
    global aliases, logger
    if os.path.isfile('/etc/almond/aliases.conf'):
        logger.info("Reading aliases from '/etc/almond/aliases'")
        df = open('/etc/almond/aliases.conf', 'rt')
        lines = df.read().split('\n')
        for l in lines:
            l = l.strip()
            if not l:
                continue
            try:
                data = json.loads(l)
            except json.decoder.JSONDecodeError:
                logger.warning("json.decoder.JSONDecodeError")
                continue 
            aliases.append(data)
            valid_aliases.append(data["alias"])

def load_conf():
    global bindPort, multi_server, multi_metrics, metrics_dir, enable_file, enable_cleaner, data_file, data_dir, enable_ssl,start_page, enable_gui, enable_mods, export_file,full_metrics_file_name
    global ssl_certificate, run_with_wsgi, ssl_key, enable_scraper, proxy_cleaner_seconds, mods_list, admin_auth_type, is_container, persistant_2fa, show_dashboard, enable_otlp_exporter
    global enable_otlp_file_watch, enable_otlp_periodic_export, otlp_export_interval
 
    config = {}
    if os.path.isfile('/etc/almond/api.conf'):
        with open("/etc/almond/api.conf", "r") as conf:
            for line in conf:
                key, value = parse_line(line)
                config[key] = value
    else:
        try:
           with open("/etc/almond/almond.conf", "r") as conf:
               for line in conf:
                   key, value = parse_line(line)
                   config[key] = value
        except OSError:
            logger.error("Could not load configuration file. It does not seem to exist!")
            print ("Could not open configutation file.")
            return 80
    
    json_file = config.get('data.jsonFile', '/opt/almond/data/almond_monitor.json')
    data_dir = config.get('api.dataDir', '/opt/almond/data') 
    metrics_dir = config.get('api.metricsDir', '/opt/almond/data/metrics')
    full_metrics_file_name = config.get('data.metricsFile', 'monitor.metrics')
    #enable_mods = bool(int(config.get('api.enableMods', 0)))
    raw_value = config.get('api.enableMods', 'false').strip().lower()
    if (raw_value) in ["1", "true", "yes"]:
        enable_mods = True
    else:
        enable_mods = False
    mods_list = config.get('api.activeMods', '').split(',')
    bindPort = int(config.get('api.bindPort', 80))
    logger.info("Howru will use port " + str(bindPort))
    proxy_cleaner_seconds = int(config.get('api.proxyCleanerSeconds', 360))
    if config.get('api.multiServer') is not None:
        is_proxy = config.get('api.multiServer', '0').strip().lower()
    	#multi_server = bool(int(config.get('api.multiServer',0)))
        if is_proxy in ["1", "true", "yes", "on"]:
            multi_server = True
        else:
            multi_server = False
    elif config.get('api.isProxy') is not None: 
        #multi_server = bool(int(config.get('api.isProxy', 0)))
        is_proxy = config.get('api.isProxy', '0')
        if is_proxy.strip() in ["1", "true", "yes", "sure"]:
            multi_server = True
        else:
            multi_server = False
    else:
        multi_server = False
    if config.get('api.multiMetrics') is not None:
        m_proxy = config.get('api.multiMetrics', '0').strip().lower()
        if m_proxy in ["1", "true", "yes", "on"]:
            multi_metrics = True
        else:
            multi_metrics = False
        #multi_metrics = bool(int(config.get('api.multiMetrics', 0)))
    elif config.get('api.isMetricsProxy') is not None:
        #multi_metrics = bool(int(config.get('api.isMetricsProxy', 0)))
        m_proxy = config.get('api.isMetricsProxy', '0').strip().lower()
        if m_proxy in ["1", "true", "yes", "sure"]:
            multi_metrics = True
        else:
            multi_metrics = False
    else:
        multi_metrics = False
    if config.get('api.wsgi') is not None:
        #run_with_wsgi = bool(int(config.get('api.wsgi', 0)))
        wsgi_val = config.get('api.wsgi', 'false').strip().lower()
        if wsgi_val in ["1", "true", "yes", "on"]:
            run_with_wsgi = True
        else:
            run_with_wsgi = False
    #enable_file = bool(int(config.get('api.enableFile', 0)))
    #enable_ssl = bool(int(config.get('api.enableSSL', 0)))
    raw_enable_file = config.get('api.enableFile', 'false')
    if raw_enable_file in ["1", "true", "yes"]:
        enable_file = True
    else:
        enable_file = False
    r_enable_ssl = config.get('api.enableSSL', 'false')
    if r_enable_ssl in ["1", "true", "yes"]:
        enable_ssl = True
    else:
        enable_ssl = False 
    if config.get('api.enableGUI') is not None:
        #enable_gui = bool(int(config.get('api.enableGUI', 1)))
        use_gui = config.get('api.enableGUI', 'true')
    else:
        #if config.get('api.useGUI') is None:
        #    enable_gui = True
        #else:
        #    enable_gui = bool(int(config.get('api.useGUI', 1)))
        use_gui = 'true'
    if use_gui in ["1", "true", "yes"]:
        enable_gui = True
    else:
        enable_gui = False
    raw_enable_cleaner = config.get('api.enableProxyCleaner', 'false')
    if raw_enable_cleaner in ["1", "true", "yes"]:
        enable_cleaner = True
    else:
        enable_cleaner = False
    if config.get('api.sslCertificate') is not None:
        ssl_certificate = config.get('api.sslCertificate', '/opt/almond/www/api/certificate.pem')
    if config.get('api.sslKey') is not None:
        ssl_key = config.get('api.sslKey', '/opt/almond/www/api/certificate.key')
    start_page = config.get('api.startPage', 'api')
    #enable_scraper = bool(int(config.get('api.enableScraper', 1)))
    r_scraper = config.get('api.enableScraper', 'true')
    if r_scraper in ["1", "true", "yes", "on"]:
        enable_scraper = True
    else:
        enable_scaper = False
    #is_container = bool(int(config.get('api.isContainer', 0)))
    r_is_container = config.get('api.isContainer', 'false')
    if r_is_container in ["1", "true", "yes"]:
        is_container = True
    else:
        is_container = False
    #persistant_2fa = bool(int(config.get('api.persistant2fa',0)))
    two_factor_auth_per = config.get('api.persistant2fa', 'false')
    if two_factor_auth_per in ["1", "true", "yes", "on"]:
        persistant_2fa = True
    else:
        pesistant_2fa = False
    proxy_dashboard = config.get('api.showDashboard', 'true')
    if proxy_dashboard in ["0", "false", "no", "off"]:
        show_dashboard = False
    else:
        show_dashboard = True
    otlp_endpoint = config.get('api.otlpEndpoint', 'http://localhost:4318')
    set_otlp_endpoint(otlp_endpoint)
    r_otlp_exporter = config.get('api.enableOtelExporter', 'false')
    if r_otlp_exporter in ["1", "true", "yes", "on"]:
        enable_otlp_exporter = True
    else:
        enable_otlp_exporter = False
    r_otlp_periodic = config.get('api.enablePeriodicOtelExport', 'false')
    if r_otlp_periodic in ["1", "true", "yes", "on"]:
        enable_otlp_periodic_export = True
    else:
        enable_otlp_periodic_export = False
    r_otlp_filewatch = config.get('api.enableOtelFileWatcher', 'false')
    if r_otlp_filewatch in ["1", "true", "yes", "on"]:
        enable_otlp_file_watch = True
    else:
        enable_otlp_file_watch = False
    otlp_export_interval  = int(config.get('api.otelExportInterval', 60))
    if otlp_export_interval < 30:
        otlp_export_interval = 30
    #enable_aliases = bool(int(config.get('api.enableAliases', 0)))
    r_aliases = config.get('api.enableAliases', 'false')
    if r_aliases in ["1", "true", "yes", "on"]:
        enable_aliases = True
    else:
        enable_aliases = False
    if enable_aliases:
        load_aliases()
    if config.get('api.adminUser') is not None:
        ausername = config.get('api.adminuser')
    if config.get('api.adminPassword') is not None:
        apassword = config.get('api.adminpassword')
    admin_auth_type = config.get('api.authType', 'basic')
    
        
    data_file = data_dir 
    data_file = data_dir + "/" + json_file
    
    return bindPort

def useCertificate():
    global enable_ssl
    return enable_ssl

def getCertificates():
    global ssl_certificate, ssl_key
    ret_val = "('" + ssl_certificate + "', '" + ssl_key + "')"
    return eval(ret_val)

def check_config():
    """Background thread to monitor the configuration file."""
    global stop_background_thread
    global almond_conf_file
    global howru_conf_file
    global sleep_time
    global logger
    use_api_conf = False
    is_reloading = False
    logger.info("[Check_conf_thread] Starting...")
    howru_last_modified = 0
    almond_last_modified = os.path.getmtime(almond_conf_file)
    if (os.path.isfile(howru_conf_file)):
        use_api_conf = True
        howru_last_modified = os.path.getmtime(howru_conf_file)
    while not stop_background_thread:
        change_detected = False
        if is_reloading:
            is_reloading = False
        logger.info("[Check_conf_thread] Check for configuration changes in almond.conf")
        try:
            current_modified = os.path.getmtime(almond_conf_file)
            if current_modified != almond_last_modified:
                print("Config change detected. Reload config.")
                logger.info("Change of configuration detected.")
                change_detected = True
                almond_last_modified = current_modified
        except Exception as e:
            logger.error(f"Failed to access {almond_conf_file}: {e}")

        if use_api_conf:
            logger.info("[Check_conf_thread] Check for configuration changes in api.conf")
            try:
                current_modified = os.path.getmtime(howru_conf_file)
                if current_modified != howru_last_modified:
                    logger.info("[Check_conf_thread] Change in api.conf detected")
                    change_detected = True
                    howru_last_modified = current_modified
            except:
                logger.error(f"Failed to access {howru_conf_file}: {e}")

        if change_detected and not is_reloading:
            logger.info("[Check_conf_thread] Reload configurations")
            change_detected = False
            load_conf()
            logger.info("Configuration reloaded")
            time.sleep(0.2)
            is_reloading = True
        time.sleep(sleep_time)

def get_sys_info():
    now = datetime.datetime.now()
    uptime = str(now - __STARTED__)
    uptime = uptime.split('.', 1)[0]

    uname = platform.uname()
    proc_type = uname[5]
    if proc_type == '':
        proc_type = uname[4];

    return { 'uptime': uptime,
             'python_version': sys.version,
             'ssl_version': ssl.OPENSSL_VERSION,
             'psutil_version': psutil.__version__,
             'processor': proc_type,
             'node': uname[1],
             'system': uname[0],
             'release': uname[2],
             'version': uname[3]
           }

def load_data():
    global data, data_dir, multi_server, enable_file, file_name, data_file, file_found, logger
    os.chdir(data_dir)
    if (enable_file == True):
        if (len(file_name) > 5):
            this_file = data_dir + "/" + file_name
            if (os.path.exists(this_file)):
                count = 0
                f = open(this_file, "r")
                data = json.loads(f.read())
                f.close()
                file_found = 1
                return
            else:
                print ("Could not find serverfile ", this_file)
                logger.warning("Could not find serverfile '" + this_file + "'")
                file_found = 0
    if (multi_server):
       #print("Running in mode multi");
       logger.info("Load data in multi_server mode")
       count = 0 
       data = {
                  "server": [
                      ]
              }

       for file in glob.glob("*.json"):
            f = open(file, "r")
            data_set = json.loads(f.read())
            data["server"].append(data_set)
            f.close()
            count = count + 1
    else:
       #print("Running in single mode");
       #f = open("monitor_data.json", "r")
       logger.info("Loading single node data.")
       if os.path.isfile(data_file):
           f = open(data_file, "r")
           data = json.loads(f.read())
           f.close()
       else:
           print ("DEBUG: No data file found")
           data = {}
    os.chdir("/opt/almond/www/api")

def load_settings():
    global settings, logger
    logger.info("Reading settings in '/etc/almond/plugins.conf'")
    f = open("/etc/almond/plugins.conf", "r")
    try:
        settings = f.readlines()
    finally:
        f.close()

def load_scheduler_settings():
    global settings, logger
    logger.info("Reading settings in '/etc/almond/almond.conf'")
    f = open("/etc/almond/almond.conf", "r")
    try:
        settings = f.readlines()
    finally:
        f.close()

def set_file_name():
    global file_name
    json_file = request.args.get("whichjson")
    if json_file is None:
        file_name = ''
    else:
        file_name = json_file + ".json"

def rand_quote(severity):
    ok_quotes = ["I'm ok, thanks for asking!", "I'm all fine, hope you are too!", "I think I never felt better!", "I feel good, I knew that I would", "I feel happy from head to feet"]
    warn_quotes = ["I'm so so", "I think someone should check me out", "Something is itching, scratch my back!", "I think I'm having a cold", "I'm not feeling all well"]
    crit_quotes = ["I'm not fine", "I feel sick, please call the doctor", "Not good, please get a technical guru to check me out", "Code red, code red", "I have fever, an aspirin needed"]
    unknown_quotes = ["I'm not completly sure to be honest", "Some things are unknown to me", "I'm not really certain", "I don´t actually no", "Not sure, maybe good - maybe not!"]
    if (severity == 0):
        return random.choice(ok_quotes)
    if (severity == 1):
        return random.choice(warn_quotes)
    if (severity == 2):
        return random.choice(crit_quotes)
    else:
        return random.choice(unknown_quotes)

def parse_value(value: str):
    """
    Parse a string like '0.089ms', '90%', or '0.75' into (number, unit).
    Returns (float, str).
    """
    match = re.match(r"([\d.]+)([a-zA-Z%]*)", value.strip())
    if match:
        number, unit = match.groups()
        return float(number), unit or None
    return None, None

def api_list_jobs(sorted=False):
    global multi_server, data, logger
    x = 0
    logger.info("Running api_list_jobs")
    load_data()
    check_wsgi_init()
    if (multi_server):
        server_jobs = []
        servers = data['server']
        for host in servers:
            x = 0
            server = {}
            jobs = [] 
            name = host.get('host', {}).get('name')
            #print(f"Name: {name}")
            mon = host['monitoring']
            for dictionary in mon:
                if (sorted):
                    jobs.append(dictionary["name"])
                else:
                    job_obj = {'id': x, 'name': dictionary["name"], 'description': dictionary["pluginName"]}
                    jobs.append(job_obj)
                    x += 1
            if (sorted):
                jobs.sort()
            server = {'server': name, 'jobs': jobs}
            server_jobs.append(server)
        return jsonify(server_jobs)
    else:
        mon = data['monitoring']
        jobs = []
        for dictionary in mon:
             if (sorted):
                 jobs.append(dictionary["name"])
             else:
                 obj = {'id': x, 'name': dictionary["name"], 'description': dictionary["pluginName"]}
                 jobs.append(obj)
                 x += 1
        if (sorted):
            jobs.sort()
    return jsonify(jobs)

def api_get_plugin_run(last=True):
    global data, multi_server, logger
    logger.info("Running api_get_last_plugin_run")
    load_data()
    if (multi_server):
        server_rts = []
        servers = data['server']
        for host in servers:
            server = {}
            runtimes = []
            name = host.get('host', {}).get('name')
            mon = host['monitoring']
            for dictionary in mon:
                runtimes.append(dictionary["lastRun"])
            if (last):
                runtimes.sort(reverse=True)
                server = {'server': name, 'lastpluginrun': runtimes[0]}
            else:
                runtimes.sort()
                server = {'server': name, 'oldestpluginrun': runtimes[0]}
            server_rts.append(server)
        return jsonify(server_rts)
    else:
        mon = data['monitoring']
        runtimes = []
        for dictionary in mon:
            runtimes.append(dictionary["lastRun"])
        if (last):
            runtimes.sort(reverse=True)
        else:
            runtimes.sort()
    return jsonify(runtimes[0])

def api_get_maintenance_list(showAll=True):
    global multi_server, data, logger
    x = 0
    logger.info("Running api_get_maintenance_list")
    load_data()
    if (multi_server):
        maintenance_list = []
        servers = data['server']
        for host in servers:
            x = 0
            server = {}
            maintenance = []
            name = host.get('host', {}).get('name')
            mon = host['monitoring']
            for dictionary in mon:
                maint_obj = {'id': x, 'name': dictionary["name"], 'maintenance': dictionary["maintenance"]}
                if (showAll):                
                    maintenance.append(maint_obj)
                else:
                    if dictionary["maintenance"] == "true":
                        maintenance.append(maint_obj)
                x += 1
            server = {'server': name, 'status': maintenance}
            maintenance_list.append(server)
        return jsonify(maintenance_list)
    else:
        mon = data['monitoring']
        maintenance_list = []
        for dictionary in mon:
            obj = {'id': x,'name': dictionary["name"], 'maintenance': dictionary["maintenance"]}
            if (showAll):
                maintenance_list.append(obj)
            else:
                if dictionary["maintenance"] == "true":
                    maintenance_list.append(obj)
            x += 1
        if not (showAll) and not maintenance_list:
            return {'maintenance_list':'empty'}
    return jsonify(maintenance_list)

def extract_all_checks():
    """
    Helper to normalize single and multi-server data into a single
    flat list of (check_dict, hostname).
    """
    global data

    load_data()
    normalized_checks = []

    # Case A: Proxied/Multi-server mode
    if "server" in data and isinstance(data["server"], list):
        for srv in data["server"]:
            host_name = srv.get("host", {}).get("name", "unknown")
            for check in srv.get("monitoring", []):
                normalized_checks.append((check, host_name))

    # Case B: Single-server mode
    elif "monitoring" in data and isinstance(data["monitoring"], list):
        host_name = data.get("host", {}).get("name", "unknown")
        for check in data["monitoring"]:
            normalized_checks.append((check, host_name))

    return normalized_checks

def periodic_otlp_exporter():
    global otlp_export_interval, logger
    while True:
        try:
            all_checks = extract_all_checks()
            if not all_checks:
                logger.warning("Periodic exporter: no checks found, retrying...")
                #time.sleep(otlp_export_interval)
                #continue
            else:
                otel_objects = [nagios_to_otel(check, host) for check, host in all_checks]
                export_otel_data(otel_objects)
                logger.info("Periodic exporter: exported metrics")
        except Exception as e:
            print(f"[Periodic Export] Error: {e}")

        time.sleep(otlp_export_interval)

def otlp_file_watcher():
    global data_file, logger
    last_mtime = 0
    last_hash = None
    last_export_time = 0
    DEBOUNCE_SECONDS = 1

    while True:
        try:
            mtime = os.path.getmtime(data_file)
            if mtime != last_mtime:
                last_mtime = mtime
                now = time.time()
                if now - last_export_time < DEBOUNCE_SECONDS:
                    continue
                last_export_time = now
                current_hash = file_hash(data_file)
                if current_hash == last_hash:
                    continue
                last_hash = current_hash

                all_checks = extract_all_checks()
                if not all_checks:
                    logger.warning("Otlp file watcher could not extract all checks.")
                else:
                    otel_objects = [nagios_to_otel(check, host) for check, host in all_checks]
                    export_otel_data(otel_objects)
                    logger.info("[File Watcher] Exported due to file change")
        except Exception as e:
            print(f"[File Watcher] Error: {e}")

        time.sleep(1)

def check_wsgi_init():
    global wsgi_init
    if not (wsgi_init):
        init_wsgi()

def init_wsgi():
    global multi_server, server_list_loaded, server_list, data, wsgi_init
    load_conf()
    os.chdir(data_dir)
    if (multi_server):
        if (server_list_loaded == 0):
            set_file_name()
            load_data()
            s_data = data["server"]
            for host in s_data:
                server_name = host["host"]["name"]
                server_list.append(server_name)
            server_list_loaded = 1
    wsgi_init = True

@app.before_request
def limit_remote_addr():
    global ALLOWED_HOSTS
    if ALLOWED_HOSTS is not None:
        host = request.host.split(":")[0]
        if host not in ALLOWED_HOSTS:
            abort(403)

@app.route('/', methods=['GET'])
def home():
    global data
    global server_list
    global server_list_loaded
    global data_file
    global enable_gui
    global logger

    # If used with WSIG, like Gunicorn we need to load conf again
    load_conf()
    full_filename = '/static/howru.png'
    almond_image = '/static/almond_small.png'
    if not enable_gui:
        return render_template("403.html")
    if (start_page == 'status'):
        return api_show_status()
    if (start_page == 'metrics'):
        return api_show_metrics()
    if (start_page == 'admin'):
        return redirect("/almond/admin") 
    os.chdir(data_dir)
    if (multi_server):
        if (server_list_loaded == 0):
            set_file_name()
            load_data()
            s_data = data["server"]
            for host in s_data:
                server_name = host["host"]["name"]
                server_list.append(server_name)
            server_list_loaded = 1
        logger.info("Render index template.")
        if (len(server_list) > 0):
            return render_template("index_m.html", len = len (server_list), server_list = server_list, user_image = full_filename)
        else:
            return render_template("index_e.html", user_image = almond_image)
        server_list_loaded = 0
        server_list.clear()
    else:
        if enable_gui:
            logger.info("Render index template")
            if (os.path.exists(data_file)):
                return render_template("index.html", user_image = full_filename)
            else:
                return render_template("index_w.html", user_image = almond_image)
        else:
            logger.info("No HTML GUI is enabled.")
            return "No GUI enabled"
    os.chdir('/opt/almond/www/api')

@app.route("/login")
def login():
    logger.info(f"Try to login with provider '{get_provider().name}'.")
    state = secrets.token_urlsafe(16)
    verifier, challenge = get_provider().generate_pkce()

    session["oauth_state"] = state
    session["pkce_verifier"] = verifier

    redirect_url = get_provider().build_auth_redirect(state, challenge)
    return redirect(redirect_url)

@app.route("/callback")
def callback():
    code = request.args.get("code")
    state = request.args.get("state")

    if state != session.get("oauth_state"):
        logger.warning(f"Received invalid state from provider '{get_provider().name}'.")
        return "Invalid state", 400

    verifier = session.get("pkce_verifier")

    token_data = get_provider().authenticate(code=code, verifier=verifier)
    if not token_data:
        logger.warning(f"Could not get token data from  provider '{get_provider().name}'.")
        return "Token exchange failed", 400

    #print("TOKEN DATA:", token_data)
    # Create your session
    session["tokens"] = {
        "access_token": token_data["access_token"],
        "refresh_token": token_data.get("refresh_token"),
        "id_token": token_data.get("id_token"),
    }

    decoded = jwt.decode(token_data["id_token"], options={"verify_signature": False})
    #print("DECODED:", decoded)
    username = (
        decoded.get("preferred_username")
        or decoded.get("email")
        or decoded.get("name")
        or decoded.get("given_name")
        or decoded.get("sub")
    )
    session["user"] = {
        "username": username,
        "id_token": token_data.get("id_token"), 
        "provider": get_provider().name 
    }
    session["login"] = "true"

    logger.info(f"User {username} logged in via {get_provider().name}.")

    return redirect("/almond/admin")

@app.route("/logged_out")
def logged_out():
        return "You have been logged out."

@app.route("/logout")
def logout(username=None):
    #print("LOGOUT ROUTE HIT")
    user = session.get("user") or {}
    if username is None:
        username = user.get("username", "unknown")
    id_token = user.get("id_token") if user else None
    url = get_provider().logout_url(redirect_to="http://localhost:8015/almond/admin", id_token=id_token)
    session.clear()
    logger.info(f"User {username} logged out")
    return redirect(url)

@app.route('/docs', methods=['GET'])
def documentation():
    full_filename = '/static/howru.png'
    if not enable_gui:
        return render_template("403.html")
    return render_template("documentation.html", user_image = full_filename)
 
@app.route('/pluginSearch', methods=['GET'])
def search_plugin():
    full_filename = '/static/howru.png'
    if not enable_gui:
        return render_template("403.html")
    if (multi_server):
        return render_template("plugin_query_mm.html", user_image = full_filename)
    else:
        return render_template("plugin_query.html", user_image = full_filename)

@app.route("/api/v1/otel/export", methods=["GET"])
def export_otel():
    all_checks = extract_all_checks()
    if not all_checks:
        return jsonify({"error": "No monitoring data found"}), 404
    otel_objects = [nagios_to_otel(check, host) for check, host in all_checks]
    try:
        export_otel_data(otel_objects)
    except CollectorConnectionError as e:
        return jsonify({"error": str(e)}), 503

    return jsonify({"status": "exported", "count": len(otel_objects)})

@app.route('/api/v1/otel', methods=['GET'])
def get_otel_data():
    all_checks = extract_all_checks()
    
    if not all_checks:
        return jsonify({"error": "No monitoring data found"}), 404
    otel_output = [nagios_to_otel(check, host) for check, host in all_checks]
    return jsonify(otel_output)

@app.route("/api/v1/otel/metrics", methods=["GET"])
def get_otel_metrics():
    all_checks = extract_all_checks()
    if not all_checks:
        return jsonify({"error": "No monitoring data found"}), 404
    otel_objects = [nagios_to_otel(check, host) for check, host in all_checks]
    all_metrics = []
    for obj in otel_objects:
        if "metrics" in obj:
            all_metrics.extend(obj["metrics"])
    return jsonify(all_metrics)

@app.route("/api/v1/otel/logs", methods=["GET"])
def get_otel_logs():
    all_checks = extract_all_checks()
    if not all_checks:
        return jsonify({"error": "No monitoring data found"}), 404
    otel_objects = [nagios_to_otel(check, host) for check, host in all_checks]
    all_logs = []
    for obj in otel_objects:
        if "logs" in obj:
            all_logs.extend(obj["logs"])
    return jsonify(all_logs)

@app.route('/api/monitoring', methods=['GET'])
@app.route('/api/v1/monitoring', methods=['GET'])
@app.route('/howru/api/monitoring', methods=['GET'])
def api_monitoring(response = True):
   global data
   global logger
   global multi_server
   server_found = 0
   set_file_name()
   load_data()
   check_wsgi_init()
   logger.info("Running api_monitoring")
   if not ('server' in request.args):
       if (response):
           if ('name' in request.args or 'item' in request.args):
               if request.args.get('item') is None:
                   searchname = request.args.get('name')
               else:
                   searchname = request.args.get('item')
               for mon in data['monitoring']:
                   if mon["name"] == searchname:
                       if ('check' in request.args):
                           if (('warning' in request.args and 'critical' in request.args) or 'help' in request.args):
                               popt = mon['pluginOutput']
                               try:
                                   status, metrics = popt.split("|", 1)
                               except ValueError as e :
                                   logger.warning("Failed to create custom check: %s", e)
                                   logger.info("Can not create check without metrics.")
                                   status, metrics = popt, ""  
                                   outputstr = "Can not create custom check. " + searchname + " does not provide metrics."
                                   result = {
                                       'returncode' : -1,
                                       'output' : outputstr
                                   }
                                   return jsonify(result)    
                               parts = metrics.strip().split()
                               values = {}
                               for part in parts:
                                   name, rest = part.split("=", 1)
                                   value = rest.split(";", 1)[0]
                                   values[name] = value
                               if 'help' in request.args:
                                   parsed = []
                                   for key, value in values.items():
                                       match = re.match(r"([\d.]+)([a-z%]*)", value)
                                       if match:
                                           number, unit = match.groups()
                                           if unit == "":
                                               unit = "number"
                                           parsed.append({
                                               "option": key,
                                               "current_value": number,
                                               "unit": unit
                                           })
                                           #print(json.dumps(parsed, indent=2))
                                   return jsonify(parsed)
                               else:
                                   try:
                                       warn_val = float(request.args['warning'])
                                   except (KeyError, ValueError):
                                       warn_val = None
                                   try:
                                       crit_val = float(request.args['critical'])
                                   except (KeyError, ValueError):
                                       crit_val = None
                                   if warn_val == None or crit_val == None:
                                       logger.warning("Either warning or critical parameters could not be converted to float values")
                                       logger.info("Can not create custom check result.")
                                       outputstr = "Can not create custom check for " + searchname + ". Parameters not convertable to float."
                                       result = {
                                           'returncode' : -2,
                                           'output' : outputstr
                                       }
                                       return jsonify(result)
                                   if 'option' in request.args:
                                       option_val = request.args['option']
                                       if option_val in values:
                                           check_val, unit = parse_value(values[option_val])
                                       else:
                                           logger.warning("Invalid option string given for custom check %s: %s", searchname, option_val)
                                           logger.info("Can not create custom check, because the option value does not exist.")
                                           outputstr = "Could not create custom check for " + searchname + ". Option invalid."
                                           result = {
                                               'returncode': -2,
                                               'output': outputstr
                                           }
                                           return jsonify(result)
                                   else:
                                       f_key = next(iter(values))
                                       check_val, unit = parse_value(values[f_key])
                                   return_code = 0
                                   if check_val > crit_val:
                                       return_code = 2
                                       outputstr = re.sub(r"\b(OK|WARNING|UNKNOWN)\b", "CRITICAL", popt)
                                   elif check_val > warn_val:
                                      return_code = 1 
                                      outputstr = re.sub(r"\b(OK|CRITICAL|UNKNOWN)\b", "WARNING", popt)
                                   else:
                                       outputstr = re.sub(r"\b(CRITICAL|WARNING|UNKNOWN)\b", "OK", popt)
                                   result = {
                                      'returncode' : return_code,
                                      'output' : outputstr
                                   }
                                   return jsonify(result)
                           else:
                               result = { 
                                         'returncode' : mon['pluginStatusCode'],
                                         'output': mon['pluginOutput']
                               }
                               return jsonify(result)
                       else:
                           return jsonify(mon)
               outputstr = searchname + " not found"
               result = {
                        'returncode' : 3,
                        'output': outputstr
               }
               return jsonify(result) 
           else:
               if multi_server:
                   return jsonify(data)
               return jsonify(data['monitoring'])
       else:
           return data
   else:
       servername = request.args['server']
       for serv in data['server']:
           name_o = serv['host']
           name = name_o['name']
           if (name == servername):
               result = serv
               server_found = 1
       if (server_found == 1):
           if (response):
               if ('name' in request.args or 'item' in request.args):
                   if request.args.get('item') is None:
                       searchname = request.args.get('name')
                   else:
                       searchname = request.args.get('item')
                   for m in result['monitoring']:
                       if m["name"] == searchname:
                           if ('check' in request.args):
                               if (('warning' in request.args and 'critical' in request.args) or 'help' in request.args):
                                   popt = m['pluginOutput']
                                   try:
                                       status, metrics = popt.split("|", 1)
                                   except ValueError as e :
                                       logger.warning("Failed to create custom check: %s", e)
                                       logger.info("Can not create check without metrics.")
                                       status, metrics = popt, ""
                                       outputstr = "Can not create custom check. " + searchname + " does not provide metrics."
                                       result = {
                                           'returncode' : -1,
                                           'output' : outputstr
                                       }
                                       return jsonify(result)
                                   parts = metrics.strip().split()
                                   values = {}
                                   for part in parts:
                                       name, rest = part.split("=", 1)
                                       value = rest.split(";", 1)[0]
                                       values[name] = value
                                   if 'help' in request.args:
                                       parsed = []
                                       for key, value in values.items():
                                           match = re.match(r"([\d.]+)([a-z%]*)", value)
                                           if match:
                                               number, unit = match.groups()
                                               if unit == "":
                                                   unit = "number"
                                               parsed.append({
                                                   "option": key,
                                                   "current_value": number,
                                                   "unit": unit
                                               })
                                       return jsonify(parsed)
                                   else:
                                       try:
                                           warn_val = float(request.args['warning'])
                                           crit_val = float(request.args['critical'])
                                       except (KeyError, ValueError):
                                           warn_val = None
                                           crit_val = None
                                       if warn_val == None or crit_val == None:
                                           logger.warning("Either warning or critical parameters could not be converted to float values")
                                           logger.info("Can not create custom check result.")
                                           outputstr = "Can not create custom check for " + searchname + ". Parameters not convertable to float."
                                           result = {
                                               'returncode' : -2,
                                               'output' : outputstr
                                           }
                                           return jsonify(result)
                                       if 'option' in request.args:
                                           option_val = request.args['option']
                                           if option_val in values:
                                               check_val, unit = parse_value(values[option_val])
                                           else:
                                               logger.warning("Invalid option string given for custom check %s: %s", searchname, option_val)
                                               logger.info("Can not create custom check, because the option value does not exist.")
                                               outputstr = "Could not create custom check for " + searchname + ". Option invalid."
                                               result = {
                                                   'returncode': -2,
                                                   'output': outputstr
                                               }
                                               return jsonify(result)
                                       else:
                                           f_key = next(iter(values))
                                           check_val, unit = parse_value(values[f_key])
                                       return_code = 0
                                       if check_val > crit_val:
                                           return_code = 2
                                           outputstr = re.sub(r"\b(OK|WARNING|UNKNOWN)\b", "CRITICAL", popt)
                                       elif check_val > warn_val:
                                          return_code = 1
                                          outputstr = re.sub(r"\b(OK|CRITICAL|UNKNOWN)\b", "WARNING", popt)
                                       else:
                                           outputstr = re.sub(r"\b(CRITICAL|WARNING|UNKNOWN)\b", "OK", popt)
                                       result = {
                                          'returncode' : return_code,
                                          'output' : outputstr
                                       }
                                       return jsonify(result)
                               else:
                                   result = {
                                         'returncode' : m['pluginStatusCode'],
                                         'output': m['pluginOutput']
                                   }
                                   return jsonify(result)
                           else:
                               return jsonify(m)
               return jsonify(result['monitoring'])
           else:
               return result
       else:
           result = [
                    { 'returnCode' :'3',
                       'monitoring':
                               {'info': 'Server not found'
                            }
                        }
                    ]
           headers = {"Content-Type": "application/json"}
           return make_response(jsonify(result), 200, headers)

@app.route('/api/all', methods=['GET'])
@app.route('/api/json', methods=['GET'])
@app.route('/api/showall', methods=['GET'])
@app.route('/api/v1/all', methods=['GET'])
@app.route('/api/v1/json', methods=['GET'])
@app.route('/api/v1/showall', methods=['GET'])
@app.route('/howru/api/showall', methods=['GET'])
@app.route('/howru/api/all', methods=['GET'])
@app.route('/howru/api/json', methods=['GET'])
def api_json(response = True):
   global data
   global logger
   server_found = 0
   set_file_name()
   load_data()
   check_wsgi_init()
   logger.info("Running api_json")
   if not ('server' in request.args): 
       if (response):
           return jsonify(data)
       else:
           return data
   else:
       servername = request.args['server']
       for serv in data['server']:
           name_o = serv['host']
           name = name_o['name']
           if (name == servername):
               result = serv
               server_found = 1
       if (server_found == 1):
           if (response):
               return jsonify(result)
           else:
               return result
       else:
           result = [
                    { 'returnCode' :'3',
                       'monitoring':
                               {'info': 'Server not found'
                            }
                        }
                    ]
           headers = {"Content-Type": "application/json"}
           return make_response(jsonify(result), 200, headers)

@app.route('/monitoring/json', methods=['GET'])
@app.route('/howru/monitoring/json', methods=['GET'])
def api_old_json():
    if not ('server' in request.args):
        return redirect(url_for('api_json'))
    else:
        servername = request.args['server']
        return redirect(url_for('api_json', server=servername))

@app.route('/api/status', methods=['GET'])
@app.route('/api/summary', methods=['GET'])
@app.route('/api/howru', methods=['GET'])
@app.route('/api/howareyou', methods=['GET'])
@app.route('/howru/api/howareyou', methods=['GET'])
@app.route('/api/v1/status', methods=['GET'])
@app.route('/api/v1/summary', methods=['GET'])
@app.route('/api/v1/howru', methods=['GET'])
@app.route('/api/v1/howareyou', methods=['GET'])
def api_howareyou(response=True):
    global data, multi_server, file_found, logger
    set_file_name()
    load_data()
    ok = 0
    warn = 0
    crit = 0
    results = []
    unknown = 0
    res = "I am not sure"
    servername = ""
    
    check_wsgi_init()
    if (multi_server):
        logger.info("Running api_howareyou (multi server mode)")
        results = {
            "server": [
                      ]
            }

        if ('server' in request.args):
            servername = request.args['server']
        if (len(servername) > 2 ):
            for serv in data['server']:
                name_o = serv['host']
                name = name_o['name']
                if (name == servername):
                    mon = serv['monitoring']
                    for status in mon:
                        if (status['pluginStatusCode'] == "0"):
                            ok = ok + 1
                        elif (status['pluginStatusCode'] == "1"):
                            warn = warn + 1
                        elif (status['pluginStatusCode'] == "2"):
                            crit = crit + 1
                        else:
                            unknown = unknown + 1
                    if (crit > 0):
                        ret_code = 2
                    else:
                        if (warn > 0):
                            ret_code = 1
                        else:
                            if (unknown > 0):
                                ret_code = 3
                            else:
                                ret_code = 0
                    res = rand_quote(ret_code)
                    result = [
                    {  'name' : name,
                       'answer' : res,
                       'return_code' : ret_code,
                       'monitor_results':
                           {'ok': ok,
                            'warning' : warn,
                            'critical' : crit,
                            'unknown': unknown
                           }
                    }
                    ]

                    if (response):
                        return jsonify(result)
                    else:
                        return result
            
        if (len(file_name) > 2):
            if (file_found == 0):
                res = "File not found"
                ret_code = 3
                result = [
                        {    'name': file_name,
                             'answer': res,
                             'return_code': ret_code
                        }
                ]
                return jsonify(result)
            name_o = data['host']
            name = name_o['name']
            obj = data['monitoring']
            for status in obj:
                if (status['pluginStatusCode'] == "0"):
                    ok = ok + 1
                elif (status['pluginStatusCode'] == "1"):
                    warn = warn + 1
                elif (status['pluginStatusCode'] == "2"):
                    crit = crit + 1
                else:
                    unknown = unknown + 1
            if (crit > 0):
                ret_code = 2
            else:
                if (warn > 0):
                    ret_code = 1
                else:
                    if (unknown > 0):
                        ret_code = 3
                    else:
                        ret_code = 0
            res = rand_quote(ret_code)
            server = {  'name' : name,
                        'answer' : res,
                        'return_code' : ret_code,
                        'monitor_results':
                           {'ok': ok,
                            'warning' : warn,
                            'critical' : crit,
                            'unknown': unknown
                        }
                    }
            results["server"].append(server)
        else:
            for serv in data['server']:
               name_o = serv['host']
               name =  name_o['name']
               obj = serv['monitoring']
               for status in obj:
                   if (status['pluginStatusCode'] == "0"):
                       ok = ok + 1
                   elif (status['pluginStatusCode'] == "1"):
                       warn = warn + 1
                   elif (status['pluginStatusCode'] == "2"):
                       crit = crit +1
                   else:
                       unknown = unknown +1
               if (crit > 0):
                   ret_code = 2
               else:
                   if (warn > 0):
                       ret_code = 1
                   else:
                       if (unknown > 0):
                           ret_code = 3
                       else:
                           ret_code = 0
               res = rand_quote(ret_code)
               server = [
                    {  'name' : name,
                       'answer' : res,
                       'return_code' : ret_code,
                       'monitor_results':
                           {'ok': ok,
                            'warning' : warn,
                            'critical' : crit,
                            'unknown': unknown
                       }
                   }
               ]

               results["server"].append(server);
               ok = 0
               warn = 0
               crit = 0
               unknown = 0
    else:
        logger.info("Running api_howareyou (single server mode)")
        obj = data['monitoring']
        for status in obj:
            if (status['pluginStatusCode'] == "0"):
                ok = ok + 1
            elif (status['pluginStatusCode'] == "1"):
                warn = warn + 1
            elif (status['pluginStatusCode'] == "2"):
                crit = crit +1
            else:
                unknown = unknown +1
        if (crit > 0):
            ret_code = 2
        else:
            if (warn > 0):
                ret_code = 1
            else:
                if (unknown > 0):
                    ret_code = 3
                else:
                    ret_code = 0
        res = rand_quote(ret_code)
        results = [
            { 'answer' : res,
                'return_code' : ret_code,    
            'monitor_results':
                   {'ok': ok,
                    'warning' : warn,
                    'critical' : crit,
                    'unknown': unknown
                }  
            }
        ]
    if (response):
        return jsonify(results)
    else:
        return results

@app.route('/monitoring/howareyou', methods=['GET'])
@app.route('/howru/monitoring/howareyou', methods=['GET'])
def api_old_howareyou():
    if ('server' in request.args):
        servername = request.args['server']
        return_redirect(url_for('api_howareyou', server=servername))
    else:
        return redirect(url_for('api_howareyou'))

@app.route('/api/oks', methods=['GET'])
@app.route('/api/ok', methods=['GET'])
@app.route('/howru/api/ok', methods=['GET'])
@app.route('/api/v1/oks', methods=['GET'])
@app.route('/api/v1/ok', methods=['GET'])
def api_show_oks():
    global data, multi_server
    global logger
    set_file_name()
    load_data()
    multi_set = 0
    servername = ""
    name_is_set = False
    name_found = False

    check_wsgi_init()
    results = []
    logger.info("Running api_show_oks")
    if (multi_server):
        try:
            host = data['host']
        except:
            multi_set = 1
    if (multi_set > 0):
        serv = data['server']
        for s in serv:
            res_set = []
            if ('server' in request.args):
                servername = request.args['server']
                if (len(servername) > 2):
                    name_is_set = True
            if (name_is_set == True):
                 if (s['host']['name'] == servername):
                    res_set.append({'name':s['host']['name']})
                    obj = s['monitoring']
                    for i in obj:
                        if (i['pluginStatusCode'] == "0"):
                            res_set.append(i)
                    results.append(res_set)
                    server_found = True
                    return jsonify(results)
                 else:
                     server_found = False
            else:
                res_set.append({'name':s['host']['name']})
                obj = s['monitoring']
                for i in obj:
                    if (i['pluginStatusCode'] == "0"):
                        res_set.append(i)
                results.append(res_set);
        if (name_is_set == True and server_found == False):
            results = [
                { 'returnCode' :'3',
                   'monitoring':
                           {'info': 'Server not found'
                        }
                    }
                ]
    else:
        results.append({'name':data['host']['name']})
        obj = data['monitoring']
        for i in obj:
            if (i['pluginStatusCode'] == "0"):
                results.append(i)

    return jsonify(results)

@app.route('/api/notoks', methods=['GET'])
@app.route('/api/notok', methods=['GET'])
@app.route('/howru/api/notok', methods=['GET'])
@app.route('/api/v1/notoks', methods=['GET'])
@app.route('/api/v1/notok', methods=['GET'])
def api_show_not_oks():
    global data, multi_server
    global logger
    set_file_name()
    load_data()
    multi_set = 0
    servername = ""
    name_is_set = False
    name_found = False

    check_wsgi_init()
    results = []
    logger.info("Running api_show_oks")
    if (multi_server):
        try:
            host = data['host']
        except:
            multi_set = 1
    if (multi_set > 0):
        serv = data['server']
        for s in serv:
            res_set = []
            if ('server' in request.args):
                servername = request.args['server']
                if (len(servername) > 2):
                    name_is_set = True
            if (name_is_set == True):
                 if (s['host']['name'] == servername):
                    res_set.append({'name':s['host']['name']})
                    obj = s['monitoring']
                    for i in obj:
                        j = int(i['pluginStatusCode'])
                        if (j > 0):
                            res_set.append(i)
                    results.append(res_set)
                    server_found = True
                    return jsonify(results)
                 else:
                     server_found = False
            else:
                res_set.append({'name':s['host']['name']})
                obj = s['monitoring']
                for i in obj:
                    if (i['pluginStatusCode'] != "0"):
                        res_set.append(i)
                results.append(res_set);
        if (name_is_set == True and server_found == False):
            results = [
                { 'returnCode' :'3',
                   'monitoring':
                           {'info': 'Server not found'
                        }
                    }
                ]
    else:
        results.append({'name': data['host']['name']})
        obj = data['monitoring']
        for i in obj:
            if (int(i['pluginStatusCode']) >0):
                results.append(i)

    return jsonify(results)

@app.route('/monitoring/ok', methods=['GET'])
@app.route('/howru/monitoring/ok', methods=['GET'])
def api_old_ok():
    if ('server' in request.args):
        servername = request.args['server']
        return redirect(url_for('api_show_oks', server=servername))
    else:
        return redirect(url_for('api_show_oks'))

@app.route('/api/warnings', methods=['GET'])
@app.route('/api/v1/warnings', methods=['GET'])
@app.route('/howru/api/warnings', methods=['GET'])
def api_show_warnings():
    global data, multi_server 
    global logger
    set_file_name()
    load_data()
    multi_set = 0
    results = []
    servername = ""
    name_is_set = False
    name_found = False

    check_wsgi_init()
    logger.info("Running api_show_warnings")
    if (multi_server):
        try:
            host = data['host']
        except:
             multi_set = 1
    if (multi_set > 0):
        serv = data['server']
        for s in serv:
            res_set = []
            if ('server' in request.args):
                servername = request.args['server']
                if (len(servername) > 2):
                    name_is_set = True
            if (name_is_set == True):
                if (s['host']['name'] == servername):
                    res_set.append({'name':s['host']['name']})
                    obj = s['monitoring']
                    for i in obj:
                        if (i['pluginStatusCode'] == "1"):
                            res_set.append(i)
                    results.append(res_set)
                    server_found = True
                    return jsonify(results)
                else:
                    server_found = False
            else:
                res_set.append({'name': s['host']['name']})
                obj = s['monitoring']
                for i in obj:
                   if (i['pluginStatusCode'] == "1"):
                       res_set.append(i)
                results.append(res_set);
        if (name_is_set == True and server_found == False):
            results = [
                    { 'returnCode' :'3',
                       'monitoring':
                               {'info': 'Server not found'
                            }
                        }
                    ]
    else:
        results.append({'name': data['host']['name']})
        obj = data['monitoring']
        for i in obj:
            if (i['pluginStatusCode'] == "1"):
                results.append(i)

    return jsonify(results)

@app.route('/monitoring/warnings', methods=['GET'])
@app.route('/howru/monitoring/warnings', methods=['GET'])
def api_old_warnings():
    if ('server' in request.args):
        servername = request.args['server']
        return redirect(url_for('api_show_warnings', server=servername))
    else:
        return redirect(url_for('api_show_warnings'))

@app.route('/api/criticals', methods=['GET'])
@app.route('/api/v1/criticals', methods=['GET'])
@app.route('/howru/api/criticals', methods=['GET'])
def api_show_criticals():
    global data, multi_server, logger
    set_file_name()
    load_data()
    multi_set = 0
    results = []
    servername = ""
    name_is_set = 0
    name_found = 0

    check_wsgi_init()
    logger.info("Running api_show_criticals")
    if (multi_server):
        try:
            host = data['host']
        except:
            multi_set = 1
    if (multi_set > 0):
        serv = data['server']
        for s in serv:
            res_set = []
            if ('server' in request.args):
                servername = request.args['server']
                if (len(servername) > 2):
                    name_is_set = 1
            if (name_is_set > 0):
                if (s['host']['name'] == servername):
                    res_set.append({'name': s['host']['name']})
                    obj = s['monitoring']
                    for i in obj:
                        if (i['pluginStatusCode'] == "2"):
                            res_set.append(i)
                    results.append(res_set)
                    name_found = 1
                    return jsonify(results) 
                else:
                    name_found = 0
            else:
                res_set.append({'name': s['host']['name']})
                obj = s['monitoring']
                for i in obj:
                    if (i['pluginStatusCode'] == "2"):
                        res_set.append(i)
                results.append(res_set);
        if (name_is_set > 0 and name_found == 0):
            result = [
                    { 'returnCode' :'3',
                       'monitoring':
                               {'info': 'Server not found'
                            }
                        }
                    ]
            return jsonify(result)
    else:
       results.append({'name': data['host']['name']})
       obj = data['monitoring']
       for i in obj:
           if (i['pluginStatusCode'] == "2"):
               results.append(i)

    return jsonify(results)

@app.route('/monitoring/criticals', methods=['GET'])
@app.route('/howru/monitoring/criticals', methods=['GET'])
def api_old_criticals():
    if ('server' in request.args):
        servername = request.args['server']
        return redirect(url_for('api_show_criticals', server=servername))
    else:
        return redirect(url_for('api_show_criticals'))

@app.route('/api/changes', methods=['GET'])
@app.route('/api/v1/changes', methods=['GET'])
@app.route('/howru/api/changes', methods=['GET'])
def api_show_changes():
    global data, multi_server, logger
    set_file_name()
    load_data()
    multi_set = 0
    results = []

    check_wsgi_init()
    logger.info("Running api_show_changes")
    if (multi_server):
        try:
            host = data['host']
        except:
            multi_set = 1
    if (multi_set > 0):
        serv = data['server']
        for s in serv:
            res_set = []
            res_set.append({'name': s['host']['name']})
            obj = s['monitoring']
            for i in obj:
               if (i['pluginStatusChanged'] == "1"):
                   res_set.append(i)
            results.append(res_set);
    else:
       results.append({'name': data['host']['name']})
       obj = data['monitoring']
       for i in obj:
           if (i['pluginStatusChanged'] == "1"):
               results.append(i)

    return jsonify(results)

@app.route('/monitoring/changes', methods=['GET'])
@app.route('/howru/monitoring/changes', methods=['GET'])
def api_old_changes():
    return redirect(url_for('api_show_changes'))

@app.route('/api/search', methods=['GET'])
@app.route('/api/querysearch', methods=['GET'])
@app.route('/howru/api/search', methods=['GET'])
@app.route('/api/v1/search', methods=['GET'])
@app.route('/api/v1/querysearch', methods=['GET'])
def api_search_plugin():
    return api_show_plugin(1)

@app.route('/api/query', methods=['GET'])
@app.route('/api/plugin', methods=['GET'])
@app.route('/api/v1/query', methods=['GET'])
@app.route('/api/v1/plugin', methods=['GET'])
@app.route('/howru/api/plugin', methods=['GET'])
def api_show_plugin(search=0, id=-1):
    global data, multi_server, logger
    set_file_name()
    load_data()
    do_start = True
    info = []
    name = ""
    index_name = ""
    server = ""
    server_found = 0
    id_count = 0
    results = []

    check_wsgi_init()
    logger.info("Running api_show_plugins")
    if (multi_server):
        if (search == 0):
            if 'server' in request.args:
                server = request.args['server']
            else:
                server = 'all'
            if 'name' in request.args:
                name = request.args['name']
            elif 'index' in request.args:
                index_name = request.args['index']
            elif 'id' in request.args:
                id = int(request.args['id'])
        else:
            if 'server' in request.args:
                server = request.args['server']
            else:
                server = 'all'
            if 'name' in request.args:
                name = request.args.getlist('name')[0]
            elif 'index' in request.args:
                index_name = request.args.getlist('index')[0]
            elif 'query' in request.args:
                name = request.args.getlist('query')[0]
            elif 'find' in request.args:
                name = request.args.getlist('find')[0]
            if (name == "") and (index_name == ""):
                if 'id' in request.args:
                    id = int(request.args['id']) 
                else:
                    print ("DEBUG: Nothing specified.")
        
        #for x in data.get('server', []):
        #    this_server = x.get('host', {}).get('name', '')
        #    if (this_server == server) or (server == 'all'):
        #        server_found = 1
        #        if not name and not index_name and id < 0 and search == 0:
        #            results.append({'name': this_server})
        #        if (not name and not index_name and id < 0):
        #            do_start = False
        #            info = [{
        #                'returnCode': '2',
        #                'monitoring': {'info': 'No id or name provided for plugin'}
        #            }]
        #            results.append(info)
        #            return jsonify(results)
        #        monitor_obj = x.get('monitoring', [])
        #        for i in monitor_obj:
        #            plugin_name = i.get('pluginName', '')
        #            name_field = i.get('name', '')
        #            if (search == 0):
        #                if ((id_count == id) or (name and name == plugin_name) or (index_name and index_name == name_field)):
        #                    results.append(i)
        #                    break
        #            else:
        #                if (name and (name.lower() in name_field.lower() or name.lower() in plugin_name.lower())) or (index_name and index_name.lower() in name_field.lower()):
        #                    results.append(i)
        #                elif (id_count == id):
        #                    results.append(i)
        #                    break
        #            id_count += 1
        for x in data['server']:
            this_server = x['host']['name']
            if (this_server == server) or (server == 'all'):
                server_found = 1
                s_name = {
                    'name': this_server
                    }
                results.append(s_name)
                if (not name and not index_name and id < 0):
                    do_start = False
                    info = [
                        { 'returnCode' :'2',
                            'monitoring':
                                {'info': 'No id or name provided for plugin'
                            }
                        }
                    ]
                    results.append(info)
                    return jsonify(results)
                monitor_obj = x['monitoring']
                for i in monitor_obj:
                    if (search == 0):
                        if ((id_count == id) or (name and name == i['pluginName']) or (index_name and index_name == i['name'])):
                            results.append(i)
                            break
                    else:
                        if ((id_count == id) or
                            (name and name.lower() in i.get('name', '').lower()) or
                            (name and name.lower() in i.get('pluginName', '').lower()) or
                            (index_name and index_name.lower() in i.get('name', '').lower())):

                            results.append(i)
                    id_count = id_count + 1
        if (server_found == 0):
            s_j = "server_search: " + server
            results.append(s_j)
            info = {
                    'info': 'Server not found in api'
                }
            results.append(info)
            return jsonify(results)
        return jsonify(results)

    # else
    results.append({'name': data['host']['name']})
    if (search == 0):
        if 'name' in request.args:
            name = request.args['name']
        elif 'index' in request.args:
            index_name = request.args['index']
        elif 'id' in request.args:
            id = int(request.args['id'])
        else:
            if (id < 0):
                do_start = False
    else:
        if 'find' in request.args:
            name = request.args['find']
        elif 'query' in request.args:
            name = request.args['query']
        elif 'name' in request.args:
            name = request.args['name']
        elif 'index' in request.args:
            index_name = request.args['index']
        elif 'id' in request.args:
            id = int(request.args['id'])
        else:
            if (id < 0):
                do_start = False
    if (do_start):
       obj = data.get('monitoring', [])
       for i in obj:
           plugin_name = i.get('pluginName', '')
           name_field = i.get('name', '')
           if (search == 0):
               if ((id_count == id) or (name and name == plugin_name) or (index_name and index_name == name_field)):
                   results.append(i)
                   break
           else:
               if (name and (name.lower() in name_field.lower() or name.lower() in plugin_name.lower())) or (index_name and index_name.lower() in name_field.lower()):
                   results.append(i)
               elif (id_count == id):
                   results.append(i)
                   break
           id_count += 1
    else:
        info = [
           { 'returnCode' :'2',
                'monitoring':
                   {'info': 'No id or name provided for plugin'
                   }
           }
        ]
        results.append(info)
    return jsonify(results)

@app.route('/monitoring/plugin', methods=['GET'])
@app.route('/howru/monitoring/plugin', methods=['GET'])
def api_old_plugin():
    if ('server' in request.args):
        servername = request.args['server']
        if ('name' in request.args):
            plugin_name = request.args['name']
            return redirect(url_for('api_show_plugin', server=servername, name=plugin_name))
        if ('id' in request.args):
            this_id = request.args['id']
            return redirect(url_for('api_show_plugin', server=servername, id=this_id))
        return redirect(url_for('api_show_plugin', server=servername))
    if ('name' in request.args):
        name = request.args['name']
        return redirect(url_for('api_show_plugin', name=name))
    elif ('id' in request.args):
        this_id = int(request.args['id'])
        return redirect(url_for('api_show_plugin', id=this_id))
    else:
        return redirect(url_for('api_show_plugin'))

@app.route('/api/countservers', methods=['GET'])
@app.route('/api/v1/countservers', methods=['GET'])
@app.route('/howru/api/countservers', methods=['GET'])
def api_count_servers():
    global data_dir, multi_server, logger

    check_wsgi_init()
    logger.info("Running api_count_servers")
    os.chdir(data_dir)
    if (multi_server):
       count = 0
       for file in glob.glob("*.json"):
            count = count + 1
    else:
        count = 1
    os.chdir("/opt/almond/www/api")
    return { 'servercount' : count }

@app.route('/api/listservers', methods=['GET'])
@app.route('/api/v1/listservers', methods=['GET'])
@app.route('/howru/api/listservers', methods=['GET'])
def api_list_servers():
    global data, multi_server, logger
    global server_list_loaded, server_list

    check_wsgi_init()
    logger.info("Running api_list_servers")
    load_data()
    this_data = api_howareyou(False)
    if (multi_server):
        servers = []
        server_data = this_data['server']
        for server in server_data:
            name = server[0]['name'] 
            servers.append(name)
        servers.sort()
    else:
        hostname = data['host']['name']
        return { 'servers': {'name':hostname}}
    return jsonify(servers)

@app.route('/api/countjobs', methods=['GET'])
@app.route('/api/v1/countjobs', methods=['GET'])
@app.route('/howru/api/countjobs', methods=['GET'])
def api_count_plugin_jobs():
    global multi_server, logger, data

    check_wsgi_init()
    logger.info("Running api_count_plugin_jobs")
    count = 0
    if (multi_server):
       load_data()

       for server in data['server']:
           monobjs = server['monitoring']
           count += len(monobjs)
    else:
        with open("/etc/almond/plugins.conf", "r") as fp:
            for count, line in enumerate(fp):
                pass
    return { 'plugincount' : count }

@app.route('/api/listjobs', methods=['GET'])
@app.route('/api/v1/listjobs', methods=['GET'])
@app.route('/howru/api/listjobs', methods=['GET'])
def return_jobs():
    jobs = api_list_jobs()
    return jobs

@app.route('/api/listjobnames', methods=['GET'])
@app.route('/api/v1/listjobnames', methods=['GET'])
@app.route('/howru/api/listjobnames', methods=['GET'])
def return_job_names():
    jobs = api_list_jobs(True)
    return jobs

@app.route('/api/unixupdatetime', methods=['GET'])
@app.route('/api/v1/unixupdatetime', methods=['GET'])
@app.route('/howru/api/unixupdatetime', methods=['GET'])
def api_get_plugin_file_timestamp(localOnly = False):
    global logger, multi_server, data

    check_wsgi_init()
    if (multi_server and not localOnly):
        logger.info("Running api_get_plugin_file_timestamp in multi mode")
        load_data()
        ts = []
        for server in data['server']:
            try:
                ts.append(server['host']['pluginfileupdatetime']) 
            except KeyError:
                logger.warning(server['host']['name'] + "does not supply updatetime.")
                continue
        ts.sort(reverse=True);
        value = int(ts[0])
        return { 'lastmodifiedtimestamp' : value }
    else:
        logger.info("Running api_get_plugin_file_timestamp")
        try:
            value = int(os.path.getmtime('/etc/almond/plugins.conf'))
        except OSError:
            logger.warning("Could not find plugins.conf file")
            value = -1
    return { 'lastmodifiedtimestamp' : value }

@app.route('/api/localunixupdatetime', methods=['GET'])
@app.route('/api/v1/localunixupdatetime', methods=['GET'])
@app.route('/howru/api/localunixupdatetime', methods=['GET'])
def api_get_local_plugin_file_timestamp():
    return api_get_plugin_file_timestamp(True)

@app.route('/api/unixdeploytime', methods=['GET'])
@app.route('/api/v1/unixdeploytime', methods=['GET'])
@app.route('/howru/api/unixdeploytime', methods=['GET'])
def api_get_config_file_timestamp():
    global logger
    config_file = '/etc/almond/api.conf'
    logger.info("Running api_get_config_file_timestamp")
    if os.path.exists(config_file):
        logger.info('Checking the api.conf file only')
    else:
        config_file = '/etc/almond/almond.conf'
    try:
        value = int(os.path.getmtime(config_file))
    except OSError:
        logger.warning("Could not find almond.conf file")
        value = -1
    return { 'configtimestamp' : value }

@app.route('/api/lastpluginrun', methods=['GET'])
@app.route('/api/v1/lastpluginrun', methods=['GET'])
@app.route('/howru/api/lastpluginrun', methods=['GET'])
def api_get_last_plugin_run():
    return api_get_plugin_run()

@app.route('/api/elderpluginrun', methods=['GET'])
@app.route('/api/v1/elderpluginrun', methods=['GET'])
@app.route('/howru/api/elderpluginrun', methods=['GET'])
def api_get_first_plugin_run():
    return api_get_plugin_run(False)
    
@app.route('/api/getmaintenancelist', methods=['GET'])
@app.route('/api/v1/getmaintenancelist', methods=['GET'])
@app.route('/howru/api/getmaintenancelist', methods=['GET'])
def api_get_maintenance_list_full():
    return api_get_maintenance_list()

@app.route('/api/getmaintenance', methods=['GET'])
@app.route('/api/v1/getmaintenance', methods=['GET'])
@app.route('/howru/api/getmaintenance', methods=['GET'])
def api_get_maintenance():
    return api_get_maintenance_list(False)

@app.route('/api/servers', methods=['GET'])
@app.route('/api/v1/servers', methods=['GET'])
@app.route('/howru/api/servers', methods=['GET'])
def api_show_server():
    global data
    global server_list
    global server_list_loaded
    global logger
    id = -1
    results = []
    set_file_name()
    load_data()

    check_wsgi_init()
    logger.info("Running api_show_server")
    if 'id' in request.args:
        id = int(request.args['id'])

    if (id == -1 ):
        if 'host' in request.args:
            # Search host
            server_name = request.args['host']
            s_data = data["server"]
            for host in s_data:
                if (server_name == host["host"]["name"]):
                    server_data = host
                    results.append(server_data)
        else:
            results = [
                    { 'returnCode' : '2',
                        'servers':
                            {
                             'info': 'No server id provided'
                            }
                    }
            ]
    else:
        # results by id
        # BUG: server_list loaded in home, must be loaded here if not
        if (server_list_loaded == 0):
            try:
                s_data = data["server"]
            except:
                results = [
                        { 'returnCode' : '3',
                            'servers':
                               { 
                                'info': 'Server api not enabled in single mode'
                               }
                        }
                ]
                return jsonify(results)
            for host in s_data:
                server_name = host["host"]["name"]
                server_list.append(server_name)
            server_list_loaded = 1
        server_name = server_list[id]
        s_data = data["server"]
        count = 0
        for host in s_data:
            if (count == id):
                server_name = host["host"]["name"]
                server_data = host
            count = count + 1
        results.append(server_data)
    
    return jsonify(results)

@app.route('/monitoring/servers', methods=['GET'])
@app.route('/howru/monitoring/servers', methods=['GET'])
def api_old_servers():
    if ('id' in request.args):
        return redirect(url_for('api_show_server', id=request.args['id']))
    elif ('host' in request.args):
        return redirect(url_for('api_show_server', host=request.args['host']))
    else:
        return redirect(url_for('api_show_server'))

@app.route('/howru/settings/plugins', methods=['GET'])
def api_show_settings():
    global settings, logger
    load_settings()
    logger.info("Running api_show_settings")
    results = []
    #results = settings
    for s in settings:
        if (s[0] != '#'):
            s_arr = s.split(';')
            pos = s_arr[0].find(']')
            p_name = s_arr[0][1:pos]
            p_des = s_arr[0][pos+2:]
            p_info = [
                    { 'pluginName' : p_name,
                        'settings':
                        {'description': p_des,
                         'command': s_arr[1],
                         'active': s_arr[2],
                         'interval': s_arr[3].rstrip()
                        }
               }
            ]
            results.append(p_info)

    return jsonify(results)

@app.route('/howru/settings/scheduler', methods=['GET'])
def api_show_scheduler_settings():
    global settings, logger
    logger.info("Running api_show_scheduler_settings")
    load_scheduler_settings()
    results = []

    #results = settings
    for s in settings:
        s.rstrip()
        s_arr = s.split('=')
        pos = s_arr[0].find('.')
        configType = s_arr[0][:pos]
        configName = s_arr[0][pos+1:]
        pos = s.find('=')
        configValue = s[pos+1:].rstrip()
        if (len(configType) > 2):
            s_info = [
                    { 'configType': configType,
                      'name': configName,
                      'value': configValue
                    }
            ]
            results.append(s_info)

    return jsonify(results)

@app.route('/howru/metrics', methods=['GET'])
def api_show_metric_lists():
    global metrics_dir
    global full_metrics_file_name
    global logger
    logger.info("Running api_show_metric_lists")
    full_filename = '/static/howru.png'
    metrics_list = []
    #onlyfiles = [f for f in os.listdir(metrics_dir) if os.path.isfile(join(metrics_dir, f))]
    for f in os.listdir(metrics_dir):
        if (f == full_metrics_file_name):
            f = "Current metrics"
        metrics_list.append(f)
    metrics_list.sort()
    if not enable_gui:
        return render_template("403.html")
    else:
        return render_template("metrics.html", user_image = full_filename, metrics_list=metrics_list)

@app.route('/howru/show_metrics', methods=['GET'])
def api_show_metrics():
    global metrics_dir, full_metrics_file_name
    global logger
    metric_selection = ''
    file_name = ''
    return_list = []
    logger.info("Running api_show_metrics")
    if not enable_gui:
        return render_template("403.html")
    if 'metric' in request.args:
        metric_selection = request.args['metric']
        if metric_selection == '-1':
            return redirect(url_for('api_show_metric_lists'))
        else:
            file_name = metrics_dir + '/' +  metric_selection
            if metric_selection == 'Current metrics':
                file_name = metrics_dir + '/' + full_metrics_file_name
            with open(file_name) as f:
                return_list = f.readlines()
                if not metric_selection == 'Current metrics':
                    return_list.reverse()
                f.close()
    else:
        #Error: no metric selection
        return redirect(url_for('api_show_metric_lists'))
    #return jsonify(return_list)
    return render_template("show_metrics.html", b_lines=return_list)

@app.route('/monitoring/status', methods=['GET'])
@app.route('/howru/monitoring/status', methods=['GET'])
def api_show_status():
    global multi_server
    global data_file
    global logger
    global show_dashboard
    almond_image = '/static/almond_small.png'
    logger.info("Running api_show_status")
    load_data()
    if not os.path.isfile(data_file):
        return render_template("index_w.html", user_image = almond_image)
    this_data = api_howareyou(False)
    full_filename = '/static/howru.png'
    image_icon = '/static/green.png'

    if not enable_gui:
        return render_template("403.html")

    if (multi_server):
        servers = [] 
        check_count = 0
        error_count = 0
        green_percentage = 0
        red_percentage = 0
        icons = ['/static/green.png', '/static/yellow.png', '/static/red.png']
        server_data = this_data['server']
        for server in server_data:
            err_checks = server[0]['monitor_results']['warning'] + server[0]['monitor_results']['critical'] + server[0]['monitor_results']['unknown']
            checks = err_checks + server[0]['monitor_results']['ok']
            check_count = check_count + checks
            error_count = error_count + err_checks
            obj = server[0]
            obj['total_count'] = sum(obj['monitor_results'].values())
            servers.append(obj) 
        green_percentage = f"{(check_count-error_count)/check_count:.2%}"
        red_percentage = f"{error_count/check_count:.2%}"
        servers_sorted = sorted(servers, key=lambda n: n['name'])
        if show_dashboard:
            return render_template("monitor.html", user_image = full_filename, total_ok = check_count, ok_percentage = green_percentage, e_percentage = red_percentage, total_non_ok = error_count, servers = servers_sorted)
        else:
            return render_template("status_mm.html", user_image = full_filename, servers = servers_sorted, icons = icons)
    else:
        if not os.path.isfile(data_file):
            return "No data file"
        hostname = socket.getfqdn()
        ret_code = this_data[0]['return_code']
        if (ret_code == 2):
            image_icon = '/static/red.png'
        elif (ret_code == 1):
            image_icon = '/static/yellow.png'
        else:
            image_icon = '/static/green.png'
        mon_res = this_data[0]['monitor_results']
        num_of_ok = mon_res['ok']
        num_of_warnings = mon_res['warning']
        num_of_criticals = mon_res['critical']
        num_of_unknown = mon_res['unknown']
        # only in json mode -> show server status
    return render_template("status.html", user_image = full_filename, server = hostname, icon = image_icon, oks = num_of_ok, warnings = num_of_warnings, criticals = num_of_criticals, unknown = num_of_unknown)

@app.route('/monitoring/details', methods=['GET', 'POST'])
@app.route('/howru/monitoring/details', methods=['GET', 'POST'])
def api_show_details():
    global multi_server
    global logger
    this_data = api_json(False)
    full_filename = '/static/howru.png'

    if not enable_gui:
        return render_template("403.html")
    
    if (multi_server):
        monitoring = []
        server = request.args['name']
        monitoring_data = this_data['server']
        for obj in monitoring_data:
            logger.info("Object hostname is: " + obj['host']['name'])
            #print (obj['host']['name'])
            if (obj['host']['name'] == server):
                #print ("Found server: " + server)
                logger.info("Found server '" + server + "'")
                #print (obj['monitoring'])
                monitoring = obj['monitoring']
                #print ("Monitoring is of type:", type(monitoring))
                logger.debug(f"Monitoring is of type: {type(monitoring)}")
                break
        if len(monitoring) == 0:
            logger.warning("No monitoring data found for server '" + server + "'")  
            return "No monitoring data found for server: " + server
        logger.info("Rendering details template.")
        return render_template("details.html", user_image=full_filename, server=server, monitoring=monitoring)
    else:
        hostname = this_data['host']['name']
        monitoring = this_data['monitoring']
        return render_template("details.html", user_image=full_filename, server=hostname, monitoring=monitoring)

@app.route('/monitoring/graph', methods=['GET', 'POST'])
@app.route('/howru/monitoring/graph', methods=['GET', 'POST'])
def api_show_graph():
    global logger
    logger.info("Running api_show_graph")
    full_filename = '/static/howru.png'
    current_plot = "check_memory"
    x_axis =[10, 20, 30, 40, 50, 60, 70, 80, 90, 100]
    data = [22.35, 21.15, 35.42, 88.12, 27.95, 28.35, 42, 75.33, 93.12, 25.87]
    plt.plot(x_axis, data)
    plt.title('Memory usage')
    plt.xlabel('Memory percentage')
    plt.ylabel('Memory used')
    plt.savefig('static/charts/chart.png')
    return render_template("graph.html", user_image = full_filename, name="Memory usage", url="/static/charts/chart.png")

@app.route('/metrics', methods=['GET'])
def api_prometheus_export():
    global enable_scraper, multi_metrics, metrics_dir, full_metrics_file_name
    global logger
    logger.info("Running api_prometheus_export")
    current_metrics_files = []
    ret_list = []
    if multi_metrics:
        for file in os.listdir(metrics_dir):
            if file.endswith('.metrics') or file.endswith('.prom'):
                file_name = metrics_dir + '/' + file
                current_metrics_files.append(file_name)
    else:
        file_name = metrics_dir + '/' + full_metrics_file_name
        current_metrics_files.append(file_name)
    #print (current_metrics_files)
    for file_name in current_metrics_files:
        #print (file_name)
        logger.info("Generating metrics from file '" + file_name + "'")
        if os.path.isfile(file_name):
            with open(file_name) as f:
                return_list = f.readlines()
                f.close()
                #print (return_list)
            if enable_scraper:
                ret_val = []
                for line in return_list:
                    line = line.strip()
                    last_char = line[len(line)-1]
                    if (last_char == '%'):
                        line = line[:-1]
                        last_char = line[len(line)-1]
                    if (line.find('(') !=  -1):
                        line = line.replace("(", "_")
                        line = line.replace(")", "")
                    while (last_char.isalpha()):
                        line = line[:-1]
                        last_char = line[len(line)-1]
                    # Check if a time thing
                    time_thing = line[-8:].strip()
                    if (time_thing.find(':') != -1):
                        time_in_secs = 0
                        time_vals = time_thing.split(':')
                        time_in_secs = int(time_vals[0])*3600 + int(time_vals[1])*60 + int(time_vals[2])
                        line = line[:-8] + ' ' + str(time_in_secs)
                    line = line + '\n'
                    ret_val.append(line)
                #print (ret_val)
                return_list = ret_val.copy()
        ret_list.extend(return_list)
    #response = app.response_class(response=ret_list, status=200, mimetype='application/txt')
    response = app.response_class(response=ret_list, status=200, headers={'Content-Type': 'text/plain; version=0.0.4; charset=utf-8'})
    return response
    # only in prometheus mode -> return metrics file
    return redirect(url_for('api_show_metric_lists'))

#@app.route('/howru/monitoring/graph', methods=['GET'])
#def get_graph():
#    x = [1,2,3]
#    # corresponding y axis values
#    y = [2,4,1]
#
#    # plotting the points 
#    plt.plot(x, y)
#
#    # naming the x axis
#    plt.xlabel('x - axis')
#    # naming the y axis
#    plt.ylabel('y - axis')
#
#    # giving a title to my graph
#    plt.title('My first graph!')
#
#    # function to show the plot
#    #return(plt.show())
#    fig = Figure()
#    ax = fig.subplots()
#    ax.plot([1, 2])
#    # Save it to a temporary buffer.
#    buf = BytesIO()
#    #fig.savefig(buf, format="png")
#    plt.savefig(buf, format="png")
#    #  plt.savefig('/static/images/new_plot2.png')
#    return render_template('graph.html', name='new_plot', url='/static/images/new_plot.png')
#    # Embed the result in the html output.
#    #data = base64.b64encode(buf.getbuffer()).decode("ascii")
#    #return f"<img src='data:image/png;base64,{data}'/>"

@app.route("/receive", methods=["POST"])
def receive():
    global multi_server, multi_metrics
    datatype = "text"
    
    if not (multi_server or multi_metrics):
        abort(404, description="Service is not a proxy")
    # Optional: require JSON content-type
    ctype = request.headers.get("Content-Type", "")
    if "application/json" not in ctype:
        # allow other content types if you prefer; otherwise reject
        if "text/plain" not in ctype:
            logger.warning("Received got payload with incorrect headers")
            abort(415, description="Expected application/json")
    else:
        datatype = "json"
    meta_bytes = request.stream.read(MAX_META_READ)
    
    if datatype == "json":
    	try:
        	meta_json = json.loads(meta_bytes)
    	except Exception:
        	abort(400, description="Invalid JSON in first chunk")
    
    	hostname = (
        	meta_json.get("host", {}).get("name")
        	or "unknown-host"
    	)
    	logger.info(f"Received payload from '{hostname}'")
    else:
        meta_string = meta_bytes.decode('utf-8', errors='ignore')
        match = re.search(r'hostname="([^"]+)"', meta_string)
        if match:
            hostname = match.group(1)
        else:
            hostname = "unknown-host"
        logger.info(f"Received payload from '{hostname}'")
    # Stream request body to a file to avoid large memory usage
    out_path = make_filename(hostname, datatype)
    try:
        with open(out_path, "wb") as f:
            # request.stream is a file-like object (werkzeug)
            f.write(meta_bytes)
            shutil.copyfileobj(request.stream, f) 
            #shutil.copyfileobj(request.stream, f)
    except Exception as e:
        logger.warning("Failed to write payload")
        abort(500, description=str(e))

    # Optionally parse/validate JSON here (e.g., json.load) if small enough
    return jsonify({"status": "ok", "saved": out_path}), 201

@app.route('/get-file/<path:path>', methods=['GET'])
def get_files(path):
    global logger
    logger.info("Running get_files(" + path + ")")
    DOWNLOAD_DIRECTORY = "/opt/almond/data"
    try:
        return send_from_directory(DOWNLOAD_DIRECTORY, path, as_attachment=True)
    except FileNotFoundError:
        logger.info("File '" + path + "' not found.")
        abort(404)

@app.route('/api/<string:Alias>', methods=['GET'])
@app.route('/api/v1/<string:Alias>', methods=['GET'])
def api_show_alias(Alias):
    global aliases, valid_aliases, logger

    if Alias in valid_aliases:
        logger.info("Collect data for alias '" + Alias + "'")
        this_id = valid_aliases.index(Alias)
        #return redirect(url_for('api_show_plugin', id=this_id))
        return api_show_plugin(0, id=this_id)
    else:
        logger.warning("Asked to collect data from unknown alias '" + Alias + "'")
        err_res = {"howru-version":current_version, "alias": Alias, "result":"Unknown api call. Call or alias does not exists."}
        return jsonify(err_res)

#def create_app():
#    global logger
#    global current_version
#    global sleep_time
#    global enable_mods
#    global app_started
#    global admin_auth_type
#
#    app = Flask(__name__)
#    app.secret_key = 'BAD_SECRET_KEY'
#    app.register_blueprint(admin_page)
#    app.register_blueprint(auth_blueprint)
#    logging.basicConfig(filename='/var/log/almond/howru.log', filemode='a', format='%(asctime)s | %(levelname)s: %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
#    logger = logging.getLogger()
#    logger.setLevel(logging.DEBUG)
#    handler = logging.FileHandler('/var/log/almond/howru.log')
#    formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
#    handler.setFormatter(formatter)
#    app.logger.addHandler(handler)
#    app.logger.setLevel(logging.DEBUG)
#    app.logger.info('Starting howru api (version:' + current_version + ')')
#    use_port = load_conf()
#    app.logger.info("Configuration read.")
#    app.config['AUTH_TYPE'] = admin_auth_type
#    if is_container:
#        app.config['IS_CONTAINER'] = 'true'
#    else:
#        app.config['IS_CONTAINER'] = 'false'
#    if persistant_2fa:
#        app.config['AUTH2FA_P'] = 'true'
#    else:
#        app.config['AUTH2FA_P'] = 'false'
#    use_ssl = useCertificate()
#    context = getCertificates()
#    return app

def main():
    global logger
    global current_version
    global sleep_time
    global enable_mods 
    global enable_cleaner, multi_server
    global app_started
    global admin_auth_type
    global enable_otlp_exporter, enable_otlp_file_watch, enable_otlp_periodic_export
    global periodic
    global ALLOWED_HOSTS
    logging.basicConfig(filename='/var/log/almond/howru.log', filemode='a', format='%(asctime)s | %(levelname)s: %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
    logger = logging.getLogger()
    logger.setLevel(logging.DEBUG)
    handler = logging.FileHandler('/var/log/almond/howru.log')
    formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
    handler.setFormatter(formatter)
    app.logger.addHandler(handler)
    app.logger.setLevel(logging.DEBUG)
    app.logger.info('Starting howru api (version:' + current_version + ')')
    use_port = load_conf()
    app.logger.info("Configuration read.")
    app.logger.info("Checking allowed_hosts file.")
    ALLOWED_HOSTS = load_allowed_hosts()
    app.config['AUTH_TYPE'] = admin_auth_type
    if is_container:
        app.config['IS_CONTAINER'] = 'true'
    else:
        app.config['IS_CONTAINER'] = 'false'
    if persistant_2fa:
        app.config['AUTH2FA_P'] = 'true'
    else:
        app.config['AUTH2FA_P'] = 'false'
    use_ssl = useCertificate()
    context = getCertificates()
    tCheck = threading.Thread(target=check_config, daemon=True)
    tCheck.start()
    if enable_mods:
        logger.info('Mods are enabled.')
        logger.info('Looking for mods to be executed.')
        tMods = threading.Thread(target=run_mods, daemon=True)
        tMods.start()
    if enable_cleaner:
        if multi_server == False:
            logger.warning('Proxy cleaner will be disabled. HowRU not running as proxy.')
            enable_cleaner = False
        else:
            logger.info('Proxy cleaner is enabled.')
            tClean = threading.Thread(target=clean_proxy_cache_loop, daemon=True)
            tClean.start()
    load_conf()
    if enable_otlp_exporter:
        logger.info('OTEL exporter is enabled.')
        if enable_otlp_periodic_export:
            logger.info("Starting periodic OTEL exporter thread.")
            tPeriodic = threading.Thread(target=periodic_otlp_exporter, daemon=True)
            tPeriodic.start()
        elif enable_otlp_file_watch:
            logger.info("Starting OTEL file watcher thread.")
            tWatcher = threading.Thread(target=otlp_file_watcher, daemon=True)
            tWatcher.start()
        else:
           logger.warning("Neither a periodic or a file watcher for otel export was selected.")
           logger.info("Otel export will not be running.")
    try:
        while True:
            if (app_started == False):
                app.logger.info("Starting application")
                if (use_ssl):
                    app.logger.info("Running application in ssl_context")
                    app.run(host='0.0.0.0', port=use_port, ssl_context=context, threaded=True, debug=False)
                else:
                    app.logger.info("Running application without encryption")
                    app.run(host='0.0.0.0', port=use_port, debug=False)
                app_started = True
            time.sleep(sleep_time)
    except (KeyboardInterrupt,SystemExit):
        app.logger.info("Caught info to stop program")
        stop_background_thread = True
        app.logger.info("Stopping thread checking for configurations changes.")
        time.sleep(1)
        app.logger.info("Main thread exits now. Goodbye :)")

if run_with_wsgi:
    if __name__ == "__main__":
        #use_port = load_conf()
        #app.run('0.0.0.0', port=use_port)
        app.run('0.0.0.0', port=80)
    #create_app().run(host="0.0.0.0", port=80)
else:
    if __name__ == '__main__':
        main()
