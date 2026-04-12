#
# MODXML.PY
# This mod requires you to have python3-dicttoxml installed
# You can install it with your package manager or using pip3
# Make sure you have a recent package of dicttoxml. You can update
# version with pip3 install --upgrade dicttoxml
#
# In almond.conf you can add the line
# data.xmlFile=<filename>.xml to specify the name of the file.
#
# Author Andreas Lindell <andreas.lindell@almondmonitor.com>
#

#!/usr/bin/python3

import os
import json
import dicttoxml
from xml.dom.minidom import parseString

def findPos(entry):
    pos = entry.find('=')
    return pos + 1

def load_conf():
    global data_dir, xml_file_name

    data_dir = ""
    data_file = ""
    xml_file_name = ""
    if os.path.isfile('/etc/almond/api.conf'):
        conf = open("/etc/almond/api.conf", "r")
    else:
        conf = open("/etc/almond/almond.conf", "r")
    for line in conf:
        if (line.find('data') == 0):
            if (line.find('jsonFile') > 0):
                json_file = line[findPos(line):].rstrip()
            if (line.find('xmlFile') > 0):
                xml_file_name = line[findPos(line):].rstrip()
        if (line.find('api') == 0):
            if (line.find('dataDir') > 0):
                data_dir = line[findPos(line):].rstrip()
    data_file = data_dir
    data_file = data_dir + "/" + json_file
    return data_file

def save_xml_to_file(string, file_name):
    with open(file_name, 'w') as file:
        file.write(string)

def main():
    global xml_file_name, data_dir
    
    data_file = load_conf()
    if len(xml_file_name) < 1:
        xml_file_name = 'export.xml'
    xml_file = data_dir + "/" + xml_file_name
    with open(data_file, 'r') as jsonfile:
        data = json.load(jsonfile)
    xml = dicttoxml.dicttoxml(data, attr_type=False)
    dom = parseString(xml)
    xml_result = dom.toprettyxml()
    save_xml_to_file(xml_result, xml_file)

if __name__ == '__main__':
    main()
