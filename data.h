#ifndef ALMOND_DATA_STRUCTURES_HEADER
#define ALMOND_DATA_STRUCTURES_HEADER

#define TIMESTAMP_SIZE 64
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
    time_t nextRun;
    bool touched;
    UT_hash_handle hh;
} PluginItem;

#endif // ALMOND_DATA_STRUCTURES_HEADER
