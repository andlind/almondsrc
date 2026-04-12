#!/usr/bin/python3
import yaml
import json
import os

def findPos(entry):
    pos = entry.find('=')
    return pos + 1

def load_conf():
    global data_dir, yaml_file_name

    data_dir = ""
    data_file = ""
    yaml_file_name = ""
    if os.path.isfile('/etc/almond/api.conf'):
        conf = open("/etc/almond/api.conf", "r")
    else:
        conf = open("/etc/almond/almond.conf", "r")
    for line in conf:
        if (line.find('data') == 0):
            if (line.find('jsonFile') > 0):
                json_file = line[findPos(line):].rstrip()
            if (line.find('yamlFile') > 0):
                yaml_file_name = line[findPos(line):].rstrip()
        if (line.find('api') == 0):
            if (line.find('dataDir') > 0):
                data_dir = line[findPos(line):].rstrip()
    data_file = data_dir
    data_file = data_dir + "/" + json_file
    return data_file

def main():
    global data_dir, yaml_file_name

    data_file = load_conf()
    if len(yaml_file_name < 1):
        yaml_file_name = 'export.yaml'
    yaml_file = data_dir + "/" + yaml_file_name
    with open(data_file, 'r') as file:
        data = json.load(file)

    with open(yaml_file, 'w') as yaml_f:
        yaml.dump(data, yaml_f)

    #with open(yaml_file, 'r') as yaml_f:
    #    print(yaml_f.read())

if __name__ == '__main__':
    main()
