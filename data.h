#ifndef ALMOND_DATA_STRUCTURES_HEADER
#define ALMOND_DATA_STRUCTURES_HEADER

#define TIMESTAMP_SIZE 64
#define PLUGIN_HISTORY_SIZE 5
#include <time.h>
#include <stdbool.h>
#include "uthash.h"    // or your favorite C hash library

typedef struct PluginOutput {
        int retCode;
        int prevRetCode;
        char* retString;
} PluginOutput;

typedef struct Scheduler {
        int id;
        time_t timestamp;
} Scheduler;

typedef struct TrackedPopen {
        FILE *fp;
        pid_t pid;
} TrackedPopen;

typedef struct PluginHistoryItem {
        int statusCode;
        char timestamp[TIMESTAMP_SIZE];
} PluginHistoryItem;

typedef struct PluginItem {
    char *name;
    char *description;
    char *command;                
    int active;
    int interval;
    int id;
    PluginOutput output;
    char lastRunTimestamp[TIMESTAMP_SIZE];
    char nextRunTimestamp[TIMESTAMP_SIZE];
    char lastChangeTimestamp[TIMESTAMP_SIZE];
        char statusChanged[2];
        bool statusChangedValue;
        char statusChangedAt[TIMESTAMP_SIZE];
        time_t statusChangedAtEpoch;
        long statusDuration;
        bool statusInitialized;
        PluginHistoryItem history[PLUGIN_HISTORY_SIZE];
        size_t historyCount;
        int alert_last_sent_state;
        bool alert_state_initialized;
        bool alert_slack_sent;
        bool alert_email_sent;
        bool alert_ilert_sent;
        bool alert_prometheus_sent;
        bool alert_pagerduty_sent;
        bool alert_opsgenie_sent;
        bool heal_attempted;
    time_t nextRun;
    bool touched;
    UT_hash_handle hh;
} PluginItem;

#endif // ALMOND_DATA_STRUCTURES_HEADER
