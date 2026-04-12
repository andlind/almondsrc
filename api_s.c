#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/resource.h>
#include <malloc.h>
#include <json-c/json.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "uthash.h"
#include "data.h"
#include "configuration.h"
#include "logger.h"
#include "plugins.h"
#include "utils.h"
#include "api.h"
#include "version.h"
#include "main.h"

void apiMonitorItem(int plugin_id, int a_flags) {
        if (a_flags == API_MONITOR_SOFT) {
                apiMonitorSoftItem(plugin_id);
        }
        else if (a_flags == API_MONITOR_SOFT_VALUE) {
                apiMonitorItemSoftValue(plugin_id);
        }
        else if (a_flags == API_MONITOR_HARD) {
                apiMonitorHardItem(plugin_id);
        }
        else if (a_flags == API_MONITOR_HARD_VALUE) {
                apiMonitorItemHardValue(plugin_id);
        }
        else {
                printf("[apiMonitorItem] a_flags do not match any run value\n");
                writeLog("[apiMonitorItem] aflags does not have a corresponding value set.", 1, 0);
        }
}

void apiMonitorSoftItem(int plugin_id) {
        char* message = NULL;
        char rCode[12];

        message = malloc((size_t)apimessage_size * sizeof(char)+1);
        if (message == NULL) {
                writeLog("Failed to allocate memory for api message.", 1, 0);
        }
        else {  
                message[0] = '\0';
                sprintf(rCode, "%d", g_plugins[plugin_id]->output.retCode);
                snprintf(message, apimessage_size, "{\n     \"plugin\":\"%s\",\n     \"output\":\"%s\",\n     \"returncode\":%s\n}\n",
                        g_plugins[plugin_id]->description,
                        trim(g_plugins[plugin_id]->output.retString),
                        trim(rCode));
        }
        socket_message = malloc((size_t)(apimessage_size + 1) * sizeof(char));
        if (socket_message == NULL) {
                fprintf(stderr, "Failed to allocate memory.\n");
                writeLog("Failed to allocate memory in [apiMonitorSoftItem: socket_message]", 2, 0);
                return;
        }
        else
                memset(socket_message, '\0', (size_t)(apimessage_size + 1) * sizeof(char));
        if (message != NULL)
                snprintf(socket_message, apimessage_size, "%s", message);
        else
                snprintf(socket_message, apimessage_size, "Failed to allocate memory for message.");
        free(message);
        message = NULL;
}

void apiMonitorHardItem(int plugin_id) {
        char* message = NULL;
        char retString[pluginoutput_size];
        char ch = '/';
        PluginOutput *output;
        int rc = 0;

        output = malloc(sizeof(PluginOutput));
    	if (output == NULL) {
        	writeLog("Failed to allocate memory for plugin output", 1, 0);
                return;
    	}
    	memset(output, 0, sizeof(PluginOutput));
        output->retString = malloc(pluginoutput_size * sizeof(char));
    	if (output->retString == NULL) {
        	writeLog("Failed to allocate memory for return string", 1, 0);
        	free(output);
        	return;
    	}
    	memset(output->retString, 0, pluginoutput_size);
        message = malloc((size_t)apimessage_size * sizeof(char)+1);
        if (message == NULL) {
                writeLog("Failed to allocate memory for api message.", 1, 0);
                free(output->retString);
                free(output);
                return;
        }
        else
                message[0] = '\0';
        memset(message, 0, apimessage_size);
        // Now run plugin_id and get output and rCode
    	snprintf(pluginCommand, plugincommand_size, "%s%c%s", pluginDir, ch, g_plugins[plugin_id]->command);
    	snprintf(infostr, infostr_size, "Running apiMonitorHard: %s.", g_plugins[plugin_id]->command);
    	writeLog(trim(infostr), 0, 0);
    	TrackedPopen tp = tracked_popen(pluginCommand);
    	if (tp.fp == NULL) {
        	printf("Failed to run command\n");
        	writeLog("Failed to run command via tracked_popen()", 2, 0);
                snprintf(retString, 120, "Failed to run command: %s", pluginCommand);
        	rc = -1;
    	}
    	else {
        	add_plugin_pid(tp.pid);
        	while (fgets(retString, sizeof(retString), tp.fp) != NULL) {
                	// VERBOSE  printf("%s", retString);
                        //printf("DEBUG: Successfully read a line: %s", retString);
            		//fflush(stdout);
        	}
        	rc = tracked_pclose(&tp);
        	if (rc == -1) {
                	snprintf(infostr, infostr_size,"[apiMonitorHardItem] tracked_pclose failed: errno %d (%s)", errno, strerror(errno));
            		writeLog(trim(infostr), 1, 0);
        	}
        	remove_plugin_pid(tp.pid);
    	}
	if (rc > 0) {
        	if (rc == 256)
                	output->retCode = 1;
        	else if (rc == 512)
                	output->retCode = 2;
        	else
                	output->retCode = rc;
    	}
    	else
        	output->retCode = rc;
        memset(output->retString, 0, pluginoutput_size);
        char* checked_output = trim(retString);
        if (checked_output) {
        	snprintf(output->retString, pluginoutput_size, "%s", checked_output);
    	}
        const char* plugin_desc = (g_plugins[plugin_id] && g_plugins[plugin_id]->description)
                                  ? g_plugins[plugin_id]->description
                                  : "Unknown";

    	const char* plugin_out = (output->retString) ? output->retString : "No output";
    	snprintf(message, apimessage_size,
        	"{\n"
        	"     \"plugin\":\"%s\",\n"
        	"     \"output\":\"%s\",\n"
        	"     \"returncode\":%d\n"
        	"}\n",
        	plugin_desc,
        	plugin_out,
        	output->retCode);

    	socket_message = malloc((size_t)(apimessage_size + 1) * sizeof(char));
    	if (socket_message == NULL) {
        	fprintf(stderr, "Failed to allocate memory for socket messages.\n");
        	writeLog("Failed to allocate memory [apiMonitorHardItem:socket message]", 2, 0);
        	return;
    	}
        else
                memset(socket_message, '\0', (size_t)(apimessage_size + 1) * sizeof(char));
    	snprintf(socket_message, apimessage_size, "%s", message);
    	free(message);
        free(output);
    	message = NULL;
        output = NULL;
}

void apiMonitorItemSoftValue(int id) {
        char* message = NULL;
        bool needHelp = false;

        message = malloc((size_t)apimessage_size * sizeof(char)+1);
        if (message == NULL) {
                writeLog("Failed to allocate memory for api message.", 1, 0);
        }
        else
                message[0] = '\0';
        const char *metrics = strchr(g_plugins[id]->output.retString, '|');
        if (!metrics) {
                char nstr[4];
                sprintf(nstr, "%d", id);
                //strcat(message,"{\n     \"customCheck\":\"");
                //strcat(message,"Item with ID ");
                //strcat(message, nstr);
                //strcat(message, "does not provide metrics.");
                //strcat(message, "\"\n}\n");
                snprintf(message, apimessage_size, "{\n     \"customCheck\":\"Item with ID %s does not provide metrics.\" \n}\n", nstr);
        }
        else {
                metrics++;
                char output[500];
                char return_code[2];
                const char *semicolon = strchr(customMonitorVals, ';');
                semicolon++;
                char metricName[32];
                sscanf(semicolon, "%31s", metricName);
                int crit = 0, warn = 0;
                char direction[16] = "below";
                char *cpos = strstr(customMonitorVals, "--critical=");
                if (cpos) {
                        sscanf(cpos, "--critical=%d", &crit);
                } else if ((cpos = strstr(customMonitorVals, "-c")) != NULL) {
                        sscanf(cpos, "-c%d", &crit);
                }
                else {
                        printf("No values read.\n");
                        crit = 0;
                }
                char *wpos = strstr(customMonitorVals, "--warning=");
                if (wpos) {
                        sscanf(wpos, "--warning=%d", &warn);
                } else if ((wpos = strstr(customMonitorVals, "-w")) != NULL) {
                        sscanf(wpos, "-w%d", &warn);
                }
                else {
                        printf("No values read.");
                        warn = 0;
                }
                sscanf(customMonitorVals, "%*[^;];%31[^:]:%15s", metricName, direction);
                char searchKey[70];
                snprintf(searchKey, sizeof(searchKey), "%s=", metricName);
                char *found = strstr(metrics, searchKey);
                if (!found) {
                        printf("Metric %s not found.\n", metricName);
                        needHelp = true;
                }
                if (!needHelp) {
                        double value = atof(found + strlen(searchKey));
                        snprintf(return_code, sizeof(return_code), "%s", "0");
                        //printf("Value of %s: %.2f\n", metricName, value);
                        if (strcmp(direction, "above") == 0) {
                                if (value > crit) {
                                        snprintf(output, sizeof(output), "CRITICAL: %s=%.2f above %d", metricName, value, crit);
                                        snprintf(return_code, sizeof(return_code), "%s", "2");
                                }
                                else if (value > warn) {
                                        snprintf(output, sizeof(output), "WARNING: %s=%.2f above %d", metricName, value, warn);
                                        snprintf(return_code, sizeof(return_code), "%s", "1");
                                }
                                else {
                                        snprintf(output, sizeof(output), "OK: %s=%.2f", metricName, value);
                                }
                        }
                        else { // default below
                                if (value < crit) {
                                        snprintf(output, sizeof(output), "CRITICAL: %s=%.2f below %d", metricName, value, crit);
                                        snprintf(return_code, sizeof(return_code), "%s", "2");
                                } else if (value < warn) {
                                        snprintf(output, sizeof(output), "WARNING: %s=%.2f below %d", metricName, value, warn);
                                        snprintf(return_code, sizeof(return_code), "%s", "1");
                                } else {
                                        snprintf(output, sizeof(output), "OK: %s=%.2f", metricName, value);
                                }
                        }
                        /*strcat(message,"{\n     \"plugin\":\"");
                        strcat(message, g_plugins[id]->description);
                        strcat(message, " ");
                        strcat(message, metricName);
                        strcat(message, "\",\n");
                        strcat(message, "     \"output\":\"");
                        strcat(message, output);
                        strcat(message, "\",\n     \"returncode\":");
                        strcat(message, return_code);
                        strcat(message, "\n}\n");*/
                        snprintf(message, apimessage_size, "{\n     \"plugin\":\"%s %s \",\n     \"output\":\"%s\",\n     \"returncode\":%s\n}\n",
                                g_plugins[id]->description,
                                metricName,
                                output, return_code);
                }
                else {
                        char temp[500];
                        snprintf(temp, sizeof(temp), "UNKNOWN: Metric '%.100s' not found. Metrics found = %.200s", metricName, output);
                        strncpy(output, temp, sizeof(output) - 1);
                        output[sizeof(output) - 1] = '\0';
                        snprintf(message, apimessage_size, "{\n     \"plugin\":\"%s %s\",\n     \"output\":\"%s\",\n       \"returncode\":3\n}\n",
                                g_plugins[id]->description, metricName, output); 
                }
        }
        //printf("%s\n", message);
        socket_message = malloc((size_t)(apimessage_size + 1) * sizeof(char));
        if (socket_message == NULL) {
                fprintf(stderr, "Failed to allocate memory.\n");
                writeLog("Failed to allocate memory in [apiMonitorSoftItem: socket_message]", 2, 0);
                return;
        }
        else
                memset(socket_message, '\0', (size_t)(apimessage_size + 1) * sizeof(char));

        snprintf(socket_message, apimessage_size, "%s", message);

        free(message);
        free(customMonitorVals);
        customMonitorVals = NULL;
        message = NULL;
        if (api_args) {
                free(api_args);
                api_args = NULL;
        }
}

void apiMonitorItemHardValue(int id) {
        char* message = NULL;
        bool needHelp = false;
        char retString[pluginoutput_size];
        char ch = '/';
        PluginOutput *output;
        int rc = 0;

        output = malloc(sizeof(PluginOutput));
        if (output == NULL) {
                writeLog("Failed to allocate memory for plugin output", 1, 0);
                return;
        }
        memset(output, 0, sizeof(PluginOutput));
        output->retString = malloc(pluginoutput_size * sizeof(char));
        if (output->retString == NULL) {
                writeLog("Failed to allocate memory for return string", 1, 0);
                free(output);
                return;
        }
        memset(output->retString, 0, pluginoutput_size);
        message = malloc((size_t)apimessage_size * sizeof(char)+1);
        if (message == NULL) {
                writeLog("Failed to allocate memory for api message.", 1, 0);
        }
        else
                message[0] = '\0';
        memset(message, 0, apimessage_size);
        snprintf(pluginCommand, plugincommand_size, "%s%c%s", pluginDir, ch, g_plugins[id]->command);
        snprintf(infostr, infostr_size, "Running apiMonitorHardValue: %s.", g_plugins[id]->command);
        writeLog(trim(infostr), 0, 0);
        TrackedPopen tp = tracked_popen(pluginCommand);
        if (tp.fp == NULL) {
                printf("Failed to run command\n");
                writeLog("Failed to run command via tracked_popen()", 2, 0);
                rc = -1;
        }
        else {
                add_plugin_pid(tp.pid);
                while (fgets(retString, sizeof(retString), tp.fp) != NULL) {
                // VERBOSE  printf("%s", retString);
                }
                rc = tracked_pclose(&tp);
                if (rc == -1) {
                        snprintf(infostr, infostr_size,"[apiMonitorHardItemValue] tracked_pclose failed: errno %d (%s)", errno, strerror(errno));
                        writeLog(trim(infostr), 1, 0);
                }
                remove_plugin_pid(tp.pid);
        }
        if (rc > 0)
        {
                if (rc == 256)
                        output->retCode = 1;
                else if (rc == 512)
                        output->retCode = 2;
                else
                        output->retCode = rc;
        }
        else
                output->retCode = rc;
        //strncpy(output.retString, trim(retString), strlen(retString));
        memset(output->retString, 0, pluginoutput_size);
        char* checked_output = trim(retString);
        if (checked_output) {
                snprintf(output->retString, pluginoutput_size, "%s", checked_output);
        }
        const char *metrics = strchr(output->retString, '|');
        if (!metrics) {
                char nstr[4];
                sprintf(nstr, "%d", id);
                snprintf(message, apimessage_size, "{\n     \"customCheck\":\"Item with ID %s does not provide metrics.\" \n}\n", nstr);
        }
        else {
                metrics++;
                char output[200];
                char return_code[2];
                const char *semicolon = strchr(customMonitorVals, ';');
                semicolon++;
                char metricName[32];
                sscanf(semicolon, "%31s", metricName);
                int crit = 0, warn = 0;
                char direction[16] = "below";
		char *cpos = strstr(customMonitorVals, "--critical=");
                if (cpos) {
                        sscanf(cpos, "--critical=%d", &crit);
                } else if ((cpos = strstr(customMonitorVals, "-c")) != NULL) {
                        sscanf(cpos, "-c%d", &crit);
                }
                else {
                        printf("No values read.\n");
                        crit = 0;
                }
                char *wpos = strstr(customMonitorVals, "--warning=");
                if (wpos) {
                        sscanf(wpos, "--warning=%d", &warn);
                } else if ((wpos = strstr(customMonitorVals, "-w")) != NULL) {
                        sscanf(wpos, "-w%d", &warn);
                }
                else {
                        printf("No values read\n.");
                        warn = 0;
                }
                sscanf(customMonitorVals, "%*[^;];%31[^:]:%15s", metricName, direction);
                char searchKey[70];
                snprintf(searchKey, sizeof(searchKey), "%s=", metricName);
                char *found = strstr(metrics, searchKey);
                if (!found) {
                        printf("Metric %s not found.\n", metricName);
                        needHelp = true;
                }
                if (!needHelp) {
                        double value = atof(found + strlen(searchKey));
                        snprintf(return_code, sizeof(return_code), "%s", "0");
                        if (strcmp(direction, "above") == 0) {
                                if (value > crit) {
                                        snprintf(output, sizeof(output), "CRITICAL: %s=%.2f above %d", metricName, value, crit);
                                        snprintf(return_code, sizeof(return_code), "%s", "2");
                                }
                                else if (value > warn) {
                                        snprintf(output, sizeof(output), "WARNING: %s=%.2f above %d", metricName, value, warn);
                                        snprintf(return_code, sizeof(return_code), "%s", "1");
                                }
                                else {
                                        snprintf(output, sizeof(output), "OK: %s=%.2f", metricName, value);
                                }
                        }
                        else { // default below
                                if (value < crit) {
                                        snprintf(output, sizeof(output), "CRITICAL: %s=%.2f below %d", metricName, value, crit);
                                        snprintf(return_code, sizeof(return_code), "%s", "2");
                                } else if (value < warn) {
                                        snprintf(output, sizeof(output), "WARNING: %s=%.2f below %d", metricName, value, warn);
                                        snprintf(return_code, sizeof(return_code), "%s", "1");
                                } else {
                                        snprintf(output, sizeof(output), "OK: %s=%.2f", metricName, value);
                                }
                        }
                        snprintf(message, apimessage_size, "{\n     \"plugin\":\"%s %s \",\n     \"output\":\"%s\",\n     \"returncode\":%s\n}\n",
                                g_plugins[id]->description,
                                metricName,
                                output, return_code);
                }
                else {
                        char temp[400];
                        snprintf(temp, sizeof(temp), "UNKNOWN: Metric '%s' not found. Metrics found = %s", metricName, output);
                        strncpy(output, temp, sizeof(output) - 1);
                        output[sizeof(output) - 1] = '\0';
                        snprintf(message, apimessage_size, "{\n     \"plugin\":\"%s %s\",\n     \"output\":\"%s\",\n       \"returncode\":3\n}\n",
                                g_plugins[id]->description, metricName, output);
                }
        }
        socket_message = malloc((size_t)(apimessage_size + 1) * sizeof(char));
        if (socket_message == NULL) {
                fprintf(stderr, "Failed to allocate memory.\n");
                writeLog("Failed to allocate memory in [apiMonitorHardItem: socket_message]", 2, 0);
                return;
        }
        else
                memset(socket_message, '\0', (size_t)(apimessage_size + 1) * sizeof(char));

        snprintf(socket_message, apimessage_size, "%s", message);
        free(message);
        free(customMonitorVals);
        free(output);
        customMonitorVals = NULL;
        message = NULL;
        output = NULL;
        if (api_args) {
                free(api_args);
                api_args = NULL;
        }
}

void apiReadData(int plugin_id, int flags) {
	char* pluginName = NULL;
	char rCode[12];
	char* message = NULL;
	unsigned short is_error = 0;

	if (plugin_id < 0) {
		printf("Strange things happen...\n");
		return;
	}

	message = malloc((size_t)apimessage_size * sizeof(char)+1);
	if (message == NULL) {
		writeLog("Failed to allocate memory for api message.", 1, 0);
	}
	else
       		message[0] = '\0';
	pluginName = malloc((size_t)pluginitemname_size * sizeof(char)+1);
	if (pluginName == NULL) {
		fprintf(stderr, "Failed to allocate memory in apiReadData.\n");
		writeLog("Failed to allocate memory [apiReadData:pluginName]", 2, 0);
		return;
	}
	if (plugin_id == 0 && flags == 0) {
		printf("This is an invalid check.\n");
		is_error++;
	}
	if (plugin_id > decCount || flags > 100) {
		printf("This is an invalid check.\n");
		is_error++;
	}	
	if (is_error > 0) {
		strcat(message, "{\n     \"almond\":\"Invalid check - no such plugin or flag\"\n}\n");
		socket_message = malloc((size_t)(apimessage_size + 1) * sizeof(char));
                if (socket_message == NULL) {
                        fprintf(stderr, "Failed to allocate memory.\n");
                        writeLog("Failed to allocate memory in [apiReadData:socket_message]", 2, 0);
                        return;
                }
		else
			memset(socket_message, '\0', (size_t)(apimessage_size + 1) * sizeof(char));
                strcpy(socket_message, message);
                free(message);
                free(pluginName);
                message = pluginName = NULL;
		return;
	}
	// In new structure I need to increase id with 1
	//plugin_id += 1;
	pluginName = strdup(g_plugins[plugin_id]->name);
        removeChar(pluginName, '[');
        removeChar(pluginName, ']');
	if (flags == API_FLAGS_VERBOSE) {
		strcat(message,"{\n     \"name\":\"");
        	strcat(message, pluginName);
        	strcat(message, "\",\n");
		strcat(message, "     \"description\":\"");
	        strcat(message, g_plugins[plugin_id]->description);
		strcat(message, "\",\n");
		switch (g_plugins[plugin_id]->output.retCode) {
			case 0:
				strcat(message, "     \"pluginStatus\":\"OK\",\n");
				break;
			case 1:
				strcat(message, "     \"pluginStatus\":\"WARNING\",\n");	
				break;
			case 2: 
				strcat(message, "     \"pluginStatus\":\"CRITICAL\",\n");
				break;
			default:
				strcat(message, "     \"pluginStatus\":\"UNKNOWN\",\n");
				break;
		}
		strcat(message, "     \"pluginStatusCode\":\"");
		sprintf(rCode, "%d", g_plugins[plugin_id]->output.retCode); 
	   	strcat(message, trim(rCode));
		strcat(message,  "\",\n");
		strcat(message, "     \"pluginOutput\":\"");
		strcat(message, trim(g_plugins[plugin_id]->output.retString));
		strcat(message, "\",\n");
		strcat(message, "     \"pluginStatusChanged\":\"");
		strcat(message, g_plugins[plugin_id]->statusChanged);
		strcat(message, "\",\n");
		strcat(message, "     \"lastChange\":\"");
		strcat(message, g_plugins[plugin_id]->lastChangeTimestamp);
		strcat(message, "\",\n");
		strcat(message, "     \"lastRun\":\"");
		strcat(message, g_plugins[plugin_id]->lastRunTimestamp);
		strcat(message, "\",\n");
                strcat(message, "     \"nextScheduledRun\":\"");
		strcat(message, g_plugins[plugin_id]->nextRunTimestamp);
		strcat(message, "\"\n");
	}
        else {
		strcat(message,"{\n     \"");
                strcat(message, pluginName);
                strcat(message, "\":\"");
                strcat(message, trim(g_plugins[plugin_id]->output.retString));
                strcat(message, "\"\n");
	}
	strcat(message, "}\n");
	free(pluginName);
	pluginName = NULL;
	socket_message = malloc((size_t)(apimessage_size + 1) * sizeof(char));
	if (socket_message == NULL) {
		fprintf(stderr, "Failed to allocate memory.\n");
		writeLog("Failed to allocate memory in [apiReadData:socket_message]", 2, 0);
		return;
	}
	else
		memset(socket_message, '\0', (size_t)(apimessage_size + 1) * sizeof(char));
	strncpy(socket_message, message, (size_t)apimessage_size);
	free(message);
	message = NULL;
}

void apiDryRun(int plugin_id) {
	char* pluginName = NULL;
	char* message = NULL;
        char retString[2280];
        char ch = '/';
        PluginOutput output;
        int rc = 0;
	
	output.retString = malloc((size_t)pluginoutput_size);
	message = malloc((size_t)(apimessage_size + 1) * sizeof(char));
	if (message == NULL) {
		writeLog("Failed to allocate memory for api message", 1, 0);
		return;
	}
	else
		memset(message, '\0', (size_t)(apimessage_size + 1) * sizeof(char));
	pluginName = malloc((size_t)(pluginitemname_size + 1) * sizeof(char));
	if (pluginName == NULL) {
        	fprintf(stderr, "Failed to allocate memory in apiDryRun.\n");
                writeLog("Failed to allocate memory [apiDryRun: pluginName", 2, 0);
                return;
        }
	else
		memset(pluginName, '\0', (size_t)(pluginitemname_size + 1) * sizeof(char));
	// In new structure increase id with 1
	//plugin_id++;
        strncpy(pluginName, g_plugins[plugin_id]->name, pluginitemname_size+1);
        removeChar(pluginName, '[');
        removeChar(pluginName, ']');
        strcpy(message, "{\n     \"dryExecutePlugin\":\"");
        strcat(message, pluginName);
        strcat(message, "\"");
        strcat(message, ",\n");
        /*strcpy(pluginCommand, pluginDir);
        strncat(pluginCommand, &ch, 1);
        strcat(pluginCommand, g_plugins[plugin_id]->command);*/
	snprintf(pluginCommand, plugincommand_size, "%s%c%s", pluginDir, ch, g_plugins[plugin_id]->command);
        snprintf(infostr, infostr_size, "Running: %s.", g_plugins[plugin_id]->command);
        writeLog(trim(infostr), 0, 0);
        TrackedPopen tp = tracked_popen(pluginCommand);
        if (tp.fp == NULL) {
                printf("Failed to run command\n");
		writeLog("Failed to run command via tracked_popen()", 2, 0);
		rc = -1;
        }
	else {
		add_plugin_pid(tp.pid);
		while (fgets(retString, sizeof(retString), tp.fp) != NULL) {
                // VERBOSE  printf("%s", retString);
        	}
        	rc = tracked_pclose(&tp);
		if (rc == -1) {
			snprintf(infostr, infostr_size,"[apiDryRun] tracked_pclose failed: errno %d (%s)", errno, strerror(errno));
            		writeLog(trim(infostr), 1, 0);
		}
		remove_plugin_pid(tp.pid);
	}
        if (rc > 0)
        {
                if (rc == 256)
                        output.retCode = 1;
                else if (rc == 512)
                        output.retCode = 2;
                else
                        output.retCode = rc;
        }
        else
                output.retCode = rc;
        strncpy(output.retString, trim(retString), strlen(retString));
        strcat(message, "     \"pluginOutput:\":\"");
	strcat(message, trim(output.retString));
        strcat(message, "\"");
	strcat(message, "\n}\n");
	socket_message = malloc((size_t)(apimessage_size + 1) * sizeof(char));
	if (socket_message == NULL) {
        	fprintf(stderr, "Failed to allocate memory for socket messages.\n");
                writeLog("Failed to allocate memory [apiDryRun:socket message]", 2, 0);
                return;
	}
	else
		memset(socket_message, '\0', (size_t)(apimessage_size + 1) * sizeof(char));
        strncpy(socket_message, message, apimessage_size);
	free(pluginName);
	pluginName = NULL;
	free(output.retString);
	output.retString = NULL;
	free(message);
	message = NULL;
}

void apiRunPlugin(int plugin_id, int flags) {
	char* pluginName = NULL;
	char* message = NULL;
	int waitCount = 0;

	message = (char *) malloc(sizeof(char) * (apimessage_size+1));
	if (message == NULL) {
		writeLog("Failed to allocate memory for api message", 1, 0);
		return;
	}
	else
		memset(message, '\0', (size_t)(apimessage_size+1) * sizeof(char));
	pluginName = malloc((size_t)(pluginitemname_size + 1) * sizeof(char));
	if (pluginName == NULL) {
		fprintf(stderr, "Failed to allocate memory in apiRunPlugin.\n");
                writeLog("Failed to allocate memory [apiRunPlugin: pluginName]", 2, 0);
                return;
        }
	else
		memset(pluginName, '\0', (size_t)(pluginitemname_size+1) * sizeof(char));
	// In new structure increase id with one
	//plugin_id++;
	pluginName = strdup(g_plugins[plugin_id]->name);
        removeChar(pluginName, '[');
        removeChar(pluginName, ']');
	// Check if same plugin is running in thread, in which case wait...
	while (threadIds[(short)plugin_id] > 0) {
		writeLog("Waiting for thread to finish...", 0, 0);
		sleep(1);
		waitCount++;
		if (waitCount > 10) {
			writeLog("Reached waitCount threshold. Continue.", 1, 0);
			break;
		}
	}
	char p_id[12];
	snprintf(p_id, sizeof(p_id), "%i",plugin_id);
	setApiCmdFile("execute", p_id);
	strcpy(message, "{\n     \"executePlugin\":\"");
	strcat(message, pluginName);
	strcat(message, "\"");
	if (flags == API_FLAGS_VERBOSE) {
		strcat(message, ",\n");
		sleep(10);
		strcat(message, "     \"pluginOutput:\":\"");
		strcat(message, trim(g_plugins[plugin_id]->output.retString));
		strcat(message, "\"");
        }
	strcat(message, "\n}\n");
	socket_message = malloc((size_t)(apimessage_size + 1) * sizeof(char));
	if (socket_message == NULL) {
		fprintf(stderr, "Failed to allocate memory in apiRunPlugin.\n");
                writeLog("Failed to allocate memory [apiRunPlugin: socket_message]", 2, 0);
                return;
        }
	else
		memset(socket_message, '\0', (size_t)(apimessage_size + 1) * sizeof(char));
	if (strlen(message) > apimessage_size) {
		printf("DEBUG: [apiRunPlugin] Message is larger than size.\n");
		message[apimessage_size-1] = '\0';
	}
	strncpy(socket_message, message, (size_t)apimessage_size);
	free(pluginName);
	pluginName = NULL;
	if (message != NULL) {
		free(message);
		message = NULL;
	}	
}

void apiRunAndRead(int plugin_id, int flags) {
	char* pluginName = NULL;
	char rCode[12];
	char strNum[12];
        char* message = NULL;
	unsigned short is_error = 0;
        
	message = malloc((size_t)apimessage_size+1 * sizeof(char));
	if (message == NULL) {
		writeLog("Could not allocate memory for apimessage", 2, 0);
		return;
	}
	else {
		memset(message, '\0', (size_t)apimessage_size+1 * sizeof(char));
	}
	if (plugin_id == 0 && flags == 0) {
                printf("This is an invalid check.\n");
                is_error++;
        }
        if (plugin_id > decCount || flags > 100) {
                printf("This is an invalid check.\n");
                is_error++;
        }
        if (is_error > 0) {
                strcat(message, "{\n     \"almond\":\"Invalid check - no such plugin or flag\"\n}\n");
                socket_message = malloc((size_t)strlen(message)+1);
                if (socket_message == NULL) {
                        fprintf(stderr, "Failed to allocate memory.\n");
                        writeLog("Failed to allocate memory in [apiReadData:socket_message]", 2, 0);
                        return;
                }
                strcpy(socket_message, message);
                free(message);
                free(pluginName);
                message = pluginName = NULL;
                return;
        }
	// In new structure increase id with 1
	//plugin_id += 1;
        snprintf(strNum, sizeof(strNum), "%d", plugin_id);
        setApiCmdFile("update", strNum);
	pluginName = (char *)malloc((size_t)(pluginitemname_size+1) * sizeof(char));
	if (pluginName == NULL) {
		fprintf(stderr, "Memory allocation failed.\n");
		writeLog("Failed to allocate memory [apiRunAndRead:pluginName]", 2, 0);
		return;
	}
	else
		memset(pluginName, '\0', (size_t)(pluginitemname_size+1) * sizeof(char));
        strncpy(pluginName, g_plugins[plugin_id]->name, (size_t)pluginitemname_size+1);
        removeChar(pluginName, '[');
        removeChar(pluginName, ']');
        //runPlugin(plugin_id, 0);
        PluginItem *item = g_plugins[plugin_id];
        if (item) {
            run_plugin(item);
        }
	if (timeScheduler)
		rescheduleChecks();
        createUpdateFile(g_plugins[plugin_id], strNum);
	strcpy(message, "{\n     \"executePlugin\":\"");
        strcat(message, pluginName);
        strcat(message, "\",\n");
        strcat(message, "      \"result\": {\n");
	sleep(10);
	if (flags == API_FLAGS_VERBOSE) {
		strcat(message, "          \"name\":\"");
		strcat(message, pluginName);
		free(pluginName);
		pluginName = NULL;
		strcat(message, "\",\n");
		strcat(message, "          \"description\":\"");
                strcat(message, g_plugins[plugin_id]->description);
                strcat(message, "\",\n");
                switch (g_plugins[plugin_id]->output.retCode) {
                        case 0:
                                strcat(message, "          \"pluginStatus\":\"OK\",\n");
                                break;
                        case 1:
                                strcat(message, "          \"pluginStatus\":\"WARNING\",\n");
                                break;
                        case 2:
                                strcat(message, "          \"pluginStatus\":\"CRITICAL\",\n");
                                break;
                        default:
                                strcat(message, "          \"pluginStatus\":\"UNKNOWN\",\n");
                                break;
                }
                strcat(message, "          \"pluginStatusCode\":\"");
                sprintf(rCode, "%d", g_plugins[plugin_id]->output.retCode);
                strcat(message, trim(rCode));
                strcat(message,  "\",\n");
                strcat(message, "          \"pluginOutput\":\"");
                strcat(message, trim(g_plugins[plugin_id]->output.retString));
                strcat(message, "\",\n");
                strcat(message, "          \"pluginStatusChanged\":\"");
                strcat(message, g_plugins[plugin_id]->statusChanged);
                strcat(message, "\",\n");
                strcat(message, "          \"lastChange\":\"");
                strcat(message, g_plugins[plugin_id]->lastChangeTimestamp);
                strcat(message, "\",\n");
                strcat(message, "          \"lastRun\":\"");
                strcat(message, g_plugins[plugin_id]->lastRunTimestamp);
                strcat(message, "\",\n");
                strcat(message, "          \"nextScheduledRun\":\"");
                strcat(message, g_plugins[plugin_id]->nextRunTimestamp);
                strcat(message, "\"\n     }\n");
	}
	else {
		strcat(message, "          \"returnString\":\"");
		strcat(message, trim(g_plugins[plugin_id]->output.retString));
		strcat(message, "\"\n     }\n");
	}
	strcat(message, "}\n");
	if (socket_message != NULL) {
		free(socket_message);
		socket_message = NULL;
	}
	socket_message = malloc((size_t)(apimessage_size+1) * sizeof(char)); 
	if (socket_message == NULL) {
		fprintf(stderr, "Failed to allocate memory.\n");
		writeLog("Failed to allocate memory [apiRunAndRead:socket_message]", 2, 0);
		return;
	}
	if (strlen(message) > apimessage_size) {
		printf("Message is to big. Try increase apimessage_size.\n");
		message[apimessage_size-1] = '\0';
	}
	strncpy(socket_message, message, (size_t)apimessage_size);
	if (pluginName != NULL) {
		free(pluginName);
		pluginName = NULL;
	}
	if (message != NULL) {
		free(message);
		message = NULL;
	}
}

void apiGetMetrics() {
	char ch = '/';

	snprintf(storeName, storename_size, "%s%c%s", storeDir, ch, metricsFileName);
	apiReadFile(storeName, 2);
}

void runPluginArgs(int id, int aflags, int api_action) {
	//const char space[1] = " ";
	char* command = NULL;
	char* newcmd = NULL;
	char* pluginName = NULL;
	char* runArgsStr = NULL;
        char ch = '/';
        PluginOutput output;
        //char currTime[22];
	char currTime[TIME_BUF_LEN];
	char rCode[12];
        int rc = 0;
	char* message = NULL;

	id++;
	//printf("DEBUG: ID = %d\n", id);
	// TODO Validate args
	message = (char *) malloc(sizeof(char) * (apimessage_size+1));
	if (message == NULL) {
		fprintf(stderr, "Failed to allocate memory in [runPluginArgs:message].\n");
		writeLog("Failed to allocate memory in [runPluginArgs:message].", 2, 0);
		return;
	}
	else
		memset(message, '\0', (size_t)(apimessage_size+1) * sizeof(char));
	newcmd = malloc(200);
	if (newcmd == NULL) {
		fprintf(stderr, "Failed to allocate memory in runPluginArgs.\n");
		writeLog("Failed to allocate memory [runPluginArgs: newcmd]", 2, 0);
		return;
	}
	else
		memset(newcmd, '\0', 200);
	command = malloc((size_t)(pluginitemcmd_size + 1) * sizeof(char));
	if (command == NULL) {
                fprintf(stderr, "Failed to allocate memory in runPluginArgs.\n");
                writeLog("Failed to allocate memory [runPluginArgs: command]", 2, 0);
                return;
        }
	else
		memset(command, '\0', (size_t)(pluginitemcmd_size + 1) * sizeof(char));
	pluginName = malloc((size_t)(pluginitemname_size + 1) * sizeof(char));
	if (pluginName == NULL) {
                fprintf(stderr, "Failed to allocate memory in runPluginArgs.\n");
                writeLog("Failed to allocate memory [runPluginArgs: pluginName]", 2, 0);
                return;
        }
	else
		memset(pluginName, '\0', (size_t)(pluginitemname_size + 1) * sizeof(char));
	output.retString = malloc((size_t)(pluginoutput_size + 1) * sizeof(char));
	if (output.retString == NULL) {
		fprintf(stderr, "Failed to allocate memory for plugin output string.\n");
		writeLog("Failed to allocate memory [runPluginArgs:output.retString).", 2, 0);
		return;
	}
	else
		memset(output.retString, '\0', (size_t)(pluginoutput_size + 1) * sizeof(char));
        strncpy(pluginName, g_plugins[id]->name, pluginitemname_size+1);
        removeChar(pluginName, '[');
        removeChar(pluginName, ']');
	strcpy(command, g_plugins[id]->command);
	char * token = strtok(command, " ");
	if (pluginDir && token && api_args) {
		snprintf(newcmd, 200, "%s%c%s %s", pluginDir, ch, token, api_args);
	}
	else {
		writeLog("Failed to create new command.", 2, 0);
		return;
	}
	//printf("DEBUG: newcmd = %s\n", newcmd);
	runArgsStr = createRunArgsStr(id, newcmd);
	if (runArgsStr != NULL) {
		setApiCmdFile("executeargs", runArgsStr);
		free(runArgsStr);
	}
	else {
		fprintf(stderr, "Failed to allocate memory for execute arguments command file.\n");
		writeLog("Failed to allocate memory [setAPiCmdsFile: executeargs].", 2, 0);
	}
	TrackedPopen tp = tracked_popen(newcmd);
        if (tp.fp == NULL) {
                printf("Failed to run command\n");
                writeLog("Failed to run command.", 2, 0);
		strcpy(message, "\n{ \"failedToRun\":\"");
	 	strcat(message, newcmd);
		strcat(message, "\"}");
		socket_message = malloc((size_t)(apimessage_size + 1) * sizeof(char));
		if (socket_message == NULL) {
                	fprintf(stderr, "Failed to allocate memory in runPluginArgs.\n");
                	writeLog("Failed to allocate memory [runPluginArgs: socketmessage]", 2, 0);
                	return;
        	}
		else
			memset(socket_message, '\0', (size_t)(apimessage_size + 1) * sizeof(char));
        	strncpy(socket_message, message, apimessage_size);
		free(api_args);
		free(command);
		memset(&newcmd[0], 0, sizeof(*newcmd));
		free(newcmd);
		free(pluginName);
		api_args = NULL;
		command = NULL;
		pluginName = NULL;
		return;
        }
	add_plugin_pid(tp.pid);
	while (fgets(pluginReturnString, pluginmessage_size, tp.fp) != NULL) {
                // VERBOSE  printf("%s", pluginReturnString);
        }
        rc = tracked_pclose(&tp);
        if (rc > 0)
        {
                if (rc == 256)
                        output.retCode = 1;
                else if (rc == 512)
                        output.retCode = 2;
                else
                        output.retCode = rc;
        }
        else
                output.retCode = rc;
	remove_plugin_pid(tp.pid);
        strcpy(output.retString, trim(pluginReturnString));
	size_t dest_size = 20;
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        int len = snprintf(currTime, dest_size, "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon +1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	if (len >= dest_size) {
		writeLog("Possible truncation of timestamp in function 'runPluginArgs'.", 1, 0);
	}
        if (api_action == API_DRY_RUN)
		strcpy(message, "{\n     \"dryExecutePlugin\":\"");
	else {
                if (output.retCode != g_plugins[id]->output.retCode){
                	strcpy(g_plugins[id]->statusChanged, "1");
                	strcpy(g_plugins[id]->lastChangeTimestamp, currTime);
		}
                else {
                	strcpy(g_plugins[id]->statusChanged, "0");
                }
		strcpy(message, "{\n     \"executePlugin\":\"");
		strcpy(g_plugins[id]->lastRunTimestamp, currTime);
                time_t nextTime = t + (g_plugins[id]->interval * 60);
                struct tm tNextTime;
                memset(&tNextTime, '\0', sizeof(struct tm));
                localtime_r(&nextTime, &tNextTime);
                len = snprintf(g_plugins[id]->nextRunTimestamp, dest_size, "%04d-%02d-%02d %02d:%02d:%02d", tNextTime.tm_year + 1900, tNextTime.tm_mon +1, tNextTime.tm_mday, tNextTime.tm_hour, tNextTime.tm_min, tNextTime.tm_sec);
		if (len >= dest_size) {
			writeLog("Possible truncation of timestamp in function 'runPluginArgs'.", 1, 0);
		}
                g_plugins[id]->nextRun = nextTime;
		if (timeScheduler) {
			scheduler[g_plugins[id]->id].timestamp = nextTime;
			rescheduleChecks();
		}
                output.prevRetCode = output.retCode;
                g_plugins[id]->output = output;
	}
        strcat(message, pluginName);
        strcat(message, "\",\n");
        strcat(message, "      \"result\": {\n");
        if (aflags == API_FLAGS_VERBOSE || aflags == API_DRY_RUN) {
                strcat(message, "          \"name\":\"");
                strcat(message, pluginName);
                free(pluginName);
		pluginName = NULL;
                strcat(message, "\",\n");
                strcat(message, "          \"description\":\"");
                strcat(message, g_plugins[id]->description);
                strcat(message, "\",\n");
                switch (output.retCode) {
                        case 0:
                                strcat(message, "          \"pluginStatus\":\"OK\",\n");
                                break;
                        case 1:
                                strcat(message, "          \"pluginStatus\":\"WARNING\",\n");
                                break;
                        case 2:
                                strcat(message, "          \"pluginStatus\":\"CRITICAL\",\n");
                                break;
                        default:
                                strcat(message, "          \"pluginStatus\":\"UNKNOWN\",\n");
                                break;
                }
                strcat(message, "          \"pluginStatusCode\":\"");
                sprintf(rCode, "%d", output.retCode);
                strcat(message, trim(rCode));
                strcat(message,  "\",\n");
                strcat(message, "          \"pluginOutput\":\"");
                strcat(message, trim(output.retString));
                strcat(message, "\",\n");
		if (aflags == API_FLAGS_VERBOSE) {
                	strcat(message, "          \"pluginStatusChanged\":\"");
                	strcat(message, g_plugins[id]->statusChanged);
                	strcat(message, "\",\n");
                	strcat(message, "          \"lastChange\":\"");
                	strcat(message, g_plugins[id]->lastChangeTimestamp);
                	strcat(message, "\",\n");
		}
                strcat(message, "          \"lastRun\":\"");
                strcat(message, currTime);
                strcat(message, "\",\n");
		if (aflags == API_FLAGS_VERBOSE) {
                	strcat(message, "          \"nextScheduledRun\":\"");
                	strcat(message, g_plugins[id]->nextRunTimestamp);
                	strcat(message, "\"\n     }\n");
		}
		else {
			strcat(message, "     }\n");
		}
        }
        else {
                strcat(message, "          \"returnString\":\"");
                strcat(message, trim(g_plugins[id]->output.retString));
                strcat(message, "\"\n     }\n");
        }
        strcat(message, "}\n");
	socket_message = malloc((size_t)(apimessage_size + 1) * sizeof(char));
	if (socket_message == NULL) {
		fprintf(stderr, "Failed to allocate memory.\n");
		writeLog("Failed to allocate memory [runPluginArgs:socket_message]", 2, 0);
		return;
	}
	else
		memset(socket_message, '\0', (size_t)(apimessage_size + 1) * sizeof(char));
        strncpy(socket_message, message, (size_t)apimessage_size);
	free(api_args);
	api_args = NULL;
	free(command);
	command = NULL;
	memset(&newcmd[0], 0, sizeof(*newcmd));
	free(newcmd);
	newcmd = NULL;
	free(output.retString);
	output.retString = NULL;
	free(message);
	message = NULL;
}

void apiGetVars(int v) {
	switch (v) {
		case 1:
			/*if (kafka_tag == NULL)
                        	constructSocketMessage("kafkatag", "NULL");
                	else
                        	constructSocketMessage("kafkatag", kafka_tag);*/
			break;
		case 2:
			constructSocketMessage("metricsprefix", metricsOutputPrefix);
			break;
		case 3:
			constructSocketMessage("jsonfilename", jsonFileName);
			break;
		case 4:
			constructSocketMessage("metricsfilename", metricsFileName);
			break;
		case 5:
			/*if (useKafkaConfigFile) {
				char* currentTopic = getKafkaTopic();
				if (currentTopic != NULL) {
					constructSocketMessage("kafkatopic", currentTopic);
				}
				else {
					constructSocketMessage("kafkatopic", "NULL");
				}
			}
			else if (kafka_topic == NULL)
                        	constructSocketMessage("kafkatopic", "NULL");
			else
                        	constructSocketMessage("kafkatopic", kafka_topic);*/
			break;
		case 6:
			int length = snprintf(NULL, 0, "%d", schedulerSleep);
			char* sleep_num = malloc(length + 1);
			snprintf(sleep_num, length + 1,  "%d", schedulerSleep);
			constructSocketMessage("schedulersleep", sleep_num);
			free(sleep_num);
			break;
		case 7:
			char soe_val[6];
			sprintf(soe_val, "%s", saveOnExit ? "true" : "false");
			constructSocketMessage("saveonexit", soe_val);
			break;
		case 8:
			char plo_val[6];
			sprintf(plo_val, "%s", logPluginOutput ? "true" : "false");
			constructSocketMessage("pluginoutput", plo_val);
			break;
		case 9:
			/*char s_kStartId[2];
			sprintf(s_kStartId, "%d", kafka_start_id);
			constructSocketMessage("kafkastartid", s_kStartId);*/
			break;
		case 10:
			char plts[14];
			sprintf(plts, "%ld", tPluginFile);
			constructSocketMessage("pluginslastchangets", plts);
			break;
		case 11:
			if (!external_scheduler) {
				constructSocketMessage("scheduler", "internal");
			}
			else {
				constructSocketMessage("scheduler", "external");
			}
			break;
		default:
			constructSocketMessage("getvar", "No matching object found");
	}
}

void apiShowStatus() {
	FILE *fp;
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);

	double user_time = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6;
	double system_time = usage.ru_stime.tv_sec + usage.ru_stime.tv_usec /1e6; 
	pid_t pid = getppid();

        json_object *jobj = json_object_new_object();
	json_object_object_add(jobj, "hostname", json_object_new_string(hostName));
        json_object_object_add(jobj, "almond_version", json_object_new_string(VERSION));
	json_object_object_add(jobj, "pid", json_object_new_int(pid));
	fp = fopen("/proc/uptime", "r");
    	if (fp) { 
		double uptime = 0.0;
    		if (fscanf(fp, "%lf", &uptime) == 1) {
        		json_object_object_add(jobj, "uptime_seconds", json_object_new_double(uptime));
    		}
    		fclose(fp);
	}
	json_object_object_add(jobj, "plugin_count", json_object_new_int(decCount));
	json_object_object_add(jobj, "user_cpu_time", json_object_new_double(user_time));
	json_object_object_add(jobj, "system_cpu_tume", json_object_new_double(system_time));
	struct mallinfo2 mi = mallinfo2();  // glibc >= 2.33
        json_object_object_add(jobj, "heap_allocated_kb", json_object_new_int64(mi.uordblks / 1024));
        json_object_object_add(jobj, "heap_total_kb",     json_object_new_int64(mi.arena / 1024));
	json_object_object_add(jobj, "max_resident_set_size_kb", json_object_new_int64(usage.ru_maxrss));
	struct rlimit rl;
    	if (getrlimit(RLIMIT_STACK, &rl) == 0) {
        	json_object_object_add(jobj, "stack_size_kb", json_object_new_int64(rl.rlim_cur / 1024));
    	}
	fp = fopen("/proc/self/statm", "r");
    	if (fp) {
		long rss_pages = 0;
    		if (fscanf(fp, "%*s %ld", &rss_pages) == 1) {
        		long page_size_kb = sysconf(_SC_PAGESIZE) / 1024;
        		json_object_object_add(jobj, "rss_kb", json_object_new_int64(rss_pages * page_size_kb));
    		}
    		fclose(fp);
	}
	json_object_object_add(jobj, "minor_page_faults", json_object_new_int64(usage.ru_minflt));
	json_object_object_add(jobj, "major_page_faults", json_object_new_int64(usage.ru_majflt));
	json_object_object_add(jobj, "swaps", json_object_new_int64(usage.ru_nswap));
	json_object_object_add(jobj, "block_input_ops", json_object_new_int64(usage.ru_inblock));
	json_object_object_add(jobj, "block_output_ops", json_object_new_int64(usage.ru_oublock));
	json_object_object_add(jobj, "ipc_msgs_sent", json_object_new_int64(usage.ru_msgsnd));
	json_object_object_add(jobj, "ipc_msgs_received", json_object_new_int64(usage.ru_msgrcv));
	json_object_object_add(jobj, "signals_received", json_object_new_int64(usage.ru_nsignals));
	json_object_object_add(jobj, "voluntary_context_switches", json_object_new_int64(usage.ru_nvcsw));
	json_object_object_add(jobj, "involuntary_context_switches", json_object_new_int64(usage.ru_nivcsw));
	json_object_object_add(jobj, "thread_count", json_object_new_int(get_thread_count()));
	json_object_object_add(jobj, "total_threads_since_start", json_object_new_int(total_threads_run));
	json_object_object_add(jobj, "open_file_descriptors", json_object_new_int64(get_fd_count()));
	fp = fopen("/proc/self/io", "r");
	if (fp) {
		char line[256];
    		while (fgets(line, sizeof(line), fp)) {
        		char key[64];
        		unsigned long long value;
			if (sscanf(line, "%63[^:]: %llu", key, &value) == 2) {
            			json_object_object_add(jobj, key, json_object_new_int64(value));
        		}
    		}
		fclose(fp);
	}
        const char *json_str = json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY);
        int size = strlen(json_str) + 2;
        socket_message = malloc((size_t)size);
        if (socket_message == NULL) {
                printf("Memory allocation failed.\n");
                writeLog("Memory allocation failed [constructSocketMessage:socket_message]", 2, 0);
                return;
        }
        else
                memset(socket_message, '\0', (size_t)size * sizeof(char));
        snprintf(socket_message, (size_t)size, "%s\n", json_str);
        json_object_put(jobj);
}

void apiShowPluginStatus() {
	int num_of_oks = 0, num_of_warnings = 0, num_of_criticals = 0, num_of_unknowns = 0;
	for (int i = 0; i < decCount; i++) {
		switch(g_plugins[i]->output.retCode) {
			case 0:
				num_of_oks++;
				break;
			case 1:
				num_of_warnings++;
				break;
			case 2:
				num_of_criticals++;
				break;
			default:
				num_of_unknowns++;
				break;
		}
	}
	json_object *jobj = json_object_new_object();
	json_object_object_add(jobj, "number_of_checks", json_object_new_int(decCount));
	json_object_object_add(jobj, "ok", json_object_new_int(num_of_oks));
	json_object_object_add(jobj, "warning", json_object_new_int(num_of_warnings));
	json_object_object_add(jobj, "critical", json_object_new_int(num_of_criticals));
	json_object_object_add(jobj, "unknown", json_object_new_int(num_of_unknowns));
	const char *json_str = json_object_to_json_string(jobj);
	int size = strlen(json_str) + 2;
        socket_message = malloc((size_t)size);
        if (socket_message == NULL) {
                printf("Memory allocation failed.\n");
                writeLog("Memory allocation failed [constructSocketMessage:socket_message]", 2, 0);
                return;
        }
        else
                memset(socket_message, '\0', (size_t)size * sizeof(char));
        snprintf(socket_message, (size_t)size, "%s\n", json_str);
	json_object_put(jobj);
}

void apiShowVersion() {
	char version[8];
	strcpy(version, "version");
	constructSocketMessage(version, VERSION);
}

void apiGetHostName() {
	char nm[9];
	strcpy(nm, "hostname");
	constructSocketMessage(nm, hostName);
}

void apiCheckPluginConf() {
	int res = check_plugin_conf_file(pluginDeclarationFile);
	if (res == 0) {
		constructSocketMessage("pluginconfiguration", "true");
	}
	else
		constructSocketMessage("pluginconfiguration", "false");
}

void apiReadFile(char *fileName, int type) {
	FILE *f = NULL;
	char info[70];
	char * message = NULL;
        long length;
        int err = 0;

	f = fopen(fileName, "r");
        if (f) {
                fseek(f, 0, SEEK_END);
                length = ftell(f);
                fseek(f, 0, SEEK_SET);
                message = malloc((size_t)length +1);
		if (message == NULL) {
			writeLog("Failed to allocate memory [apiReadFile:message]", 2, 0);
			return;
		}
                if (message) {
			size_t bytes_read = fread(message, 1, length, f);
			if (bytes_read != length) {
				writeLog("[apiReadFile] fread: Partial read of EOF", 1, 0);
			}
			message[length] = '\0';
                }
                fclose(f);
		f = NULL;
        }
        else err++;

        if (message) {
		socket_message = malloc((size_t)length +1);
		if (socket_message == NULL) {
			fprintf(stderr,"Memory allocation failed.\n");
			writeLog("Failed to allocate memory [apiReadFile:socket_message]", 2, 0);
			return;
		}
		//else
		//	memset(socket_message, '\0', (size_t)length);
                strncpy(socket_message, message, (size_t)length);
		socket_message[length] = '\0';
        }
        else err++;
        if (err > 0) {
		if (type == 2)
                	snprintf(info, 70, "{ \"return_info\":\"Could not read metrics file. No results found.\"}\n");
		else
			snprintf(info, 70, "{ \"return_info\":\"Could not read almond file. No results found.\"}\n");
                socket_message = malloc(71);
		if (socket_message == NULL) {
			writeLog("Failed to allocate memory in apiReadFile:err:socket_message", 2, 0);
			return;
		}
		memset(socket_message, '\0', 71);
                strcpy(socket_message, info);
        }
        free(message);
	message = NULL;
}

void createUpdateFile(PluginItem *item, char name[3]) {
    FILE *fp;
    char filename[30];
	/* build the full filename and verify it fits into the buffer */
	int written = snprintf(filename, sizeof(filename), "/opt/almond/api_cmd/%s.udf", name);
	if (written < 0 || (size_t)written >= sizeof(filename)) {
		writeLog("Could not create update file path.", 1, 0);
		return;
	}

	fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Failed to open %s: %s\n", filename, strerror(errno));
        return;
    }

    /* write the fields */
    fprintf(fp, "item_id\t%d\n", item->id);
    fprintf(fp, "item_name\t%s\n", item->name);
    fprintf(fp, "item_lastruntimestamp\t%s\n", item->lastRunTimestamp);
    fprintf(fp, "item_nextruntimestamp\t%s\n", item->nextRunTimestamp);
    fprintf(fp, "item_lastchangetimestamp\t%s\n", item->lastChangeTimestamp);
    fprintf(fp, "item_statuschanged\t%s\n", item->statusChanged);
    fprintf(fp, "item_nextrun\t%ld\n", (long)item->nextRun);
    /* now use the embedded output */
    fprintf(fp, "output_retcode\t%d\n",  item->output.retCode);
    fprintf(fp, "output_retstring\t%s\n", item->output.retString);

    fclose(fp);
}

char* createRunArgsStr(int num, const char* str) {
	char int_str[12]; // Enough for 32-bit int
    	snprintf(int_str, sizeof(int_str), "%d", num);

    	size_t tot_len = strlen(int_str) + 1 + strlen(str) + 1;
    	char* result = malloc(tot_len);
    	if (result == NULL) {
        	return NULL;
    	}

    	snprintf(result, tot_len, "%s;%s", int_str, str);
    	return result;
}
