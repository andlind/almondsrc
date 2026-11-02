#ifndef ALMOND_DATA_STRUCTURES_HEADER
#define ALMOND_DATA_STRUCTURES_HEADER

#include <stdbool.h>
#include <time.h>
#include <json-c/json.h>

#define PLUGIN_HISTORY_SIZE 5

typedef struct PluginHistoryItem {
        int statusCode;
        char timestamp[64];
} PluginHistoryItem;

typedef struct PluginItem {
        char* name;
        char* description;
        char* command;
        char lastRunTimestamp[20];
        char nextRunTimestamp[20];
        char lastChangeTimestamp[20];
        char statusChanged[2];
        bool statusChangedValue;
        char statusChangedAt[64];
        time_t statusChangedAtEpoch;
        long statusDuration;
        bool statusInitialized;
        PluginHistoryItem history[PLUGIN_HISTORY_SIZE];
        size_t historyCount;
        int alert_last_sent_state;
        unsigned char alert_state_initialized;
        unsigned char alert_slack_sent;
        unsigned char alert_email_sent;
        unsigned char alert_ilert_sent;
        unsigned char alert_prometheus_sent;
        unsigned char alert_pagerduty_sent;
        unsigned char alert_opsgenie_sent;
        int active;
        int interval;
        int id;
        time_t nextRun;
} PluginItem;

typedef struct PluginOutput {
        int retCode;
        int prevRetCode;
        char* retString;
} PluginOutput;

typedef struct Scheduler {
	int id;
	time_t timestamp;
} Scheduler;

typedef struct {
	const char *name;
    	const char *id;
    	const char *tag;
    	const char *lastChange;
        const char *lastRun;
        const char *dataName;
        const char *nextRun;
        const char *pluginName;
        const char *pluginOutput;
        const char *pluginStatus;
        const char *pluginStatusChanged;
        int pluginStatusCode;
	struct json_object *labels;  
    	struct json_object *metrics;
} GKafkaMessage;

#endif

