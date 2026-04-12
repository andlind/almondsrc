#!/usr/bin/python3

import subprocess
import sys

return_code = 0
returnString = ""
metricsString = ""
collect_metrics = []
p_metrics = subprocess.run("curl localhost:9644/public_metrics | grep almond", shell=True, text=True, capture_output=True)
l_metrics = subprocess.run("curl localhost:9644/metrics | grep almond", shell=True, text=True, capture_output=True)
str_metrics = p_metrics.stdout + l_metrics.stdout
metrics = str_metrics.split('\n')
for x in metrics:
	if x.find("kafka_under_replicated_replicas") > 0:
		collect_metrics.append(x)
	if x.find("kafka_max_offset") > 0:
		collect_metrics.append(x)
	if x.find("kafka_request_bytes_total") > 0:
		collect_metrics.append(x)
	if x.find("heartbeat_requests_errors") > 0:
		collect_metrics.append(x)
	if x.find("log_written_bytes") > 0:
		collect_metrics.append(x)
for c in collect_metrics:
	if c.find("under_replicated") > 0:
		under_replicated = c.split(" ")[1]
		if (isinstance(under_replicated, str)):
			try:
				under_replicated = float(under_replicated)
			except:
				print("Error: under_replicated must be a numeric value")
				sys.exit(3)
		rounded_under_replicated = round(under_replicated)
		metricsString += f" under_replicated_replicas={rounded_under_replicated}"
		if (rounded_under_replicated) > 0:
			return_code = 2
	if c.find("max_offset") > 0:
		max_offset = c.split(" ")[1]
		metricsString += " max_offset=" + max_offset.split(".")[0]
	if c.find("written_bytes") > 0:
		written_bytes = c.split(" ")[1]
		metricsString += " log_written_bytes=" + written_bytes
	if c.find("heartbeat") > 0:
		heartbeat_errors = c.split(" ")[1]
		metricsString += " heartbeat_request_errors=" + heartbeat_errors
		if (isinstance(heartbeat_errors, str)):
			try:
				heartbeat_errors = int(heartbeat_errors)
			except:
				print("Conversion error: heartbeat_errors must be a numeric value")
				sys.exit(3)
		if (heartbeat_errors > 0):
			return_code = 1
	if c.find("request_bytes") > 0:
		if c.find("produce") > 0:
			p_request_bytes = c.split(" ")[1]
			metricsString += " producer_request_bytes=" + p_request_bytes
		else:
			c_request_bytes = c.split(" ")[1]
			metricsString += " consumer_request_bytes=" + c_request_bytes	
if (return_code == 0):
	returnString = "Almond topic is fine"
elif (return_code == 1):
	returnString = "Almond has heartbeat request errors"
elif (return_code == 2):
	returnString = "Almond is underreplicated"
else:
	returnString = "Something is wrong..."
print (returnString + " |" + metricsString)
sys.exit(return_code)
		

