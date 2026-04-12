#ifndef ALMOND_DATA_STRUCTURES_HEADER
#define ALMOND_DATA_STRUCTURES_HEADER

typedef struct PluginItem {
        char* name;
        char* description;
        char* command;
        char lastRunTimestamp[20];
        char nextRunTimestamp[20];
        char lastChangeTimestamp[20];
        char statusChanged[1];
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

#endif

