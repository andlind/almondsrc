#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include "configuration.h"
#include "logger.h"
#include "api.h"
#include "utils.h"
#include "main.h"

void process_allow_all_hosts(ConfVal value) {
        if ((strcmp(value.strval, "false") == 0) || (value.intval > 0)) {
                writeLog("Almond API will neeed /etc/almond/allowed_hosts file.", 0, 1);
                if (load_allowed_hosts(allowed_hosts_file) < 0) {
                        writeLog("File '/etc/almond/allowed_hosts' not found.", 2, 1);
                        writeLog("Almond API will connect to any host.", 1, 1);
                        return;
                }
                allowAllHosts = false;
        }
        else {
                writeLog("Almond API will connect to any host.", 0, 1);
        }
}

void process_almond_api(ConfVal value) {
	if ((strcmp(value.strval, "true") == 0) || (value.intval >= 1)) {
		local_api = true;
	}
}

void process_almond_certificate(ConfVal value) {
	almondCertificate = malloc((size_t)strlen(value.strval)+1);
	if (almondCertificate == NULL) {
		fprintf(stderr, "Failed to allocate memory [almondCertificate].\n");
		writeLog("Failed to allocate memory [almondCertificate]", 2, 1);
		config_memalloc_fails++;
		return;
	}
	strncpy(almondCertificate, value.strval, strlen(value.strval));
	almondCertificate[strlen(value.strval)] = '\0';
	writeLog("Almond certificate provided if TLS for API is enabled.", 0, 1);
}

void process_almond_key(ConfVal value) {
	almondKey = malloc((size_t)strlen(value.strval)+1);
	if (almondKey == NULL) {
		fprintf(stderr, "Failed to allocate memory [almondSSLKey].\n");
		writeLog("Failed to allocate memory [almondSSLKey]", 2, 1);
		config_memalloc_fails++;
		return;
	}
	strncpy(almondKey, value.strval, strlen(value.strval));
	almondKey[strlen(value.strval)] = '\0';
	writeLog("Almond certificate key provided to be used by API to run with  SSL encryption.", 0, 1);
}

void process_almond_port(ConfVal value) {
	if (value.intval >= 1) {
        	local_port = value.intval;
	}
	else local_port = ALMOND_API_PORT;
	if (local_api) {
        	writeLog("Almond will enable local api.", 0, 1);
        }
}

void process_almond_standalone(ConfVal value) {
	if ((strcmp(value.strval, "true") == 0) || (value.intval >= 1)) {
		writeLog("Almond will run standalone. No monitor data will be sent to HowRU.", 0, 1);
		standalone = true;
	}
}

void process_almond_api_tls(ConfVal value) {
	if ((strcmp(value.strval, "true") == 0) || (value.intval >= 1)) {
		writeLog("Almond scheduler use TLS encryption.", 0, 1);
		use_ssl = true;
	}
}

void process_json_file(ConfVal value) {
	//strncpy(jsonFileName, value.strval, strlen(value.strval));
	//jsonFileName[strlen(value.strval)] = '\0';
	snprintf(jsonFileName, jsonfilename_size, "%s", value.strval);
	snprintf(infostr, infostr_size, "Json data will be collected in file: %s.", jsonFileName);
	writeLog(trim(infostr), 0, 1);
}

void process_metrics_file(ConfVal val) {
	/*strncpy(metricsFileName, val.strval, strlen(val.strval));
        metricsFileName[strlen(val.strval)] = '\0';*/
	snprintf(metricsFileName, metricsfilename_size, "%s", val.strval);
	snprintf(infostr, infostr_size, "Metrics will be collected in file: %s", metricsFileName);
	writeLog(trim(infostr), 0, 1);
}

void process_almond_push(ConfVal value) {
        if ((strcmp(value.strval, "true") == 0) || (value.intval >= 1)) {
                writeLog("Almond data push enabled.", 0, 1);
                use_push = true;
        }
}

void process_metrics_push(ConfVal value) {
        if ((strcmp(value.strval, "true") == 0) || (value.intval >= 1)) {
                writeLog("Almond metrics push enabled.", 0, 1);
                use_metrics_push = true;
        }
}

void process_push_url(ConfVal value) {
        size_t this_len = strlen(value.strval) + 1;
        push_url = malloc(this_len);
        if (push_url == NULL) {
                fprintf(stderr, "Failed to allocate memory for push url.\n");
                writeLog("Failed to allocate memory [push_url]", 2, 1);
                if (use_push)
                        use_push = false;
                return;
        }
        else
                memset(push_url, '\0', (size_t)(strlen(value.strval)+1) * sizeof(char));
        snprintf(push_url, this_len, "%s", value.strval);
        snprintf(infostr, infostr_size, "Almond push url is set to '%s'", push_url);
        writeLog(trim(infostr), 0, 1);
}

void process_push_port(ConfVal value) {
        if (value.intval >= 1) {
                push_port = value.intval;
        }
        else {
                if (use_push) {
                        writeLog("Almond use push requires variable 'push_port' which is not set properly.", 1, 1);
                        // Set push port from api.conf value
                }
        }
}

void process_push_interval(ConfVal value) {
        if (value.intval >= 1) {
                // Sanity check
                push_interval = value.intval;
                snprintf(infostr, infostr_size, "Almond push interval is set to %d.", push_interval);
                writeLog(trim(infostr), 0, 1);
        }
        else {
                if (use_push) {
                        writeLog("Almond push interval not set properly in configution.", 1, 1);
                        writeLog("Almond push interval is set to default value of 120.", 0, 1);
                }
        }
}

