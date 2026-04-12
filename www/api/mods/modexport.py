#!/usr/bin/python3
# MODEXPORT.PY
# Requirements pycurl (install with pip3 install pycurl)
# This mod exports the current howru metrics to a file.
# The default value of the export file is '/opt/almond/data/howru_export.prom'.
#
# In almond.conf you can add the line
# data.howru_export_file=<filename> to specify the name of the file.
# the following line in the configuration file
# api.howru_export_interval=30
# will give the export interval in seconds
#
# Author Andreas Lindell <andreas.lindell@almondmonitor.com>
#
import os
import time
import pycurl
from io import BytesIO

def findPos(entry):
    pos = entry.find('=')
    return pos + 1

def load_conf():
    global howru_port, file_name, export_interval

    howru_port = "85"
    file_name = "/opt/almond/data/howru_export.prom"
    export_interval = 30
    if os.path.isfile('/etc/almond/api.conf'):
        conf = open("/etc/almond/api.conf", "r")
    else:
        conf = open("/etc/almond/almond.conf", "r")
    for line in conf:
        if (line.find('api.bindPort') == 0):
            howru_port = line[findPos(line):].rstrip()
        if (line.find('data.howru_export_file') == 0):
            file_name = line[findPos(line):].rstrip()
        if (line.find('api.howru_export_interval') == 0):
            export_interval = line[findPos(line):].rstrip()

def export_howru_metrics_file():
    global file_name, howru_port

    b_obj = BytesIO()
    crl = pycurl.Curl()
    url = "http://localhost:" + howru_port + "/metrics"
    crl.setopt(crl.URL, url)
    crl.setopt(crl.WRITEDATA, b_obj)
    crl.perform()
    crl.close()
    get_body = b_obj.getvalue()
    #print ('Output of GET request:\n%s' % get_body.decode('utf-8'))
    with open(file_name, "wb") as f:
        f.write(get_body)

def main():
    global howru_port, file_name, export_interval
    load_conf()
    if (len(file_name) < 10):
        file_name = '/opt/almond/data/howru_export.prom'
    if os.path.isfile(file_name):
        ti_m = os.path.getmtime(file_name)
        print (ti_m)
        if (int(time.time()) > int(ti_m) + export_interval):
            export_howru_metrics_file()
    else:
        export_howru_metrics_file()

if __name__ == '__main__':
    main()
