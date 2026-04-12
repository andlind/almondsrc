#!/usr/bin/python3

import os
from os import walk
from datetime import datetime

data_dir="/opt/almond/data/metrics"
metrics_file="monitor.metrics"
enabled=0
cleanup_time=86400

def load_conf():
    global store_result, data_dir, enabled, cleanup_time
    conf = open("/etc/almond/almond.conf", "r")
    for line in conf:
        if (line.find('scheduler') == 0):
            if (line.find('storeDir') > 0):
                pos = line.find('=')
                data_dir = line[pos+1:].rstrip()
            if (line.find('enableGardener') > 0):
                pos = line.find('=')
                enabled = line[pos+1].rstrip()
        if (line.find('data') == 0):
            if (line.find('metricsFile') > 0):
                pos = line.find('=')
                metrics_file = line[pos+1:].rstrip()
        if (line.find('gardener') == 0):
            if (line.find('cleanUpTime') > 0):
                pos = line.find('=')
                cleanup_time = line.pos[+1].rstrip()
                if (isinstance(int(cleanup_time), int)):
                    cleanup_time = int(cleanup_time)
                else:
                    cleanup_time = 86400

    conf.close()

def get_data_dir():
    global data_dir
    return data_dir

def get_enabled():
    global enabled
    return enabled

def get_cleanup_time():
    global cleanup_time
    return cleanup_time

def get_metrics_file():
    global metrics_file
    return metrics_file

def truncate_metrics_file(datadir, metricsfile, clean_uptime):
    # Truncate file
    current_dir = os.getcwd()
    os.chdir(datadir)
    now = datetime.now()
    date_time = now.strftime("%a %b %d %H:%M:%S %Y")
    truncid = -1
    line_count = 0
    with open(metricsfile, 'r+') as f:
        lines = f.readlines()
        for line in lines:
            line_count = line_count +1
            pos = line.find(',')
            date_strip = line[:pos]
            try:
                datetime_obj = datetime.strptime(date_strip, "%a %b %d %H:%M:%S %Y")
            except ValueError:
                print ("Value error!")
                print (date_strip)
                datetime_obj = datetime.strptime("Tue Apr  18 19:38:03 1972", "%a %b %d %H:%M:%S %Y")
            timediff = now - datetime_obj
            timediff_in_secs = timediff.total_seconds()
            print(timediff_in_secs)
            if (timediff_in_secs > cleanup_time):
                print (line + " should be deleted")
            else:
                if (truncid == -1):
                    truncid = line_count
                    print (line + " should be saved")
                else:
                    break
        f.seek(0)
        f.truncate()
        f.writelines(lines[line_count:])
        f.close()
    os.chdir(current_dir)
    
if __name__ == '__main__':
    load_conf()
    
    filenames = next(walk(get_data_dir()), (None, None, []))[2]

    for f in filenames:
        if (f == get_metrics_file()):
            print (f + " is current metrics file.")
        else:
            print (f + " will be cut.")
            truncate_metrics_file(get_data_dir(), f, get_cleanup_time())
