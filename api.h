#ifndef API_H
#define API_H

#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <stdbool.h>
#include "constants.h"

#define ALMOND_API_PORT 9165
#define SOCKET_READY 1
#define NO_SOCKET -1
#define API_READ 10
#define API_MONITOR 11
#define API_RUN 15
#define API_EXECUTE_AND_READ 25
#define API_GET_METRICS 30
#define API_READ_ALL 100
#define API_FLAGS_VERBOSE 1
#define API_DRY_RUN 6
#define API_EXECUTE_GARDENER 17
#define API_NAME_START 50
#define API_ENABLE_TIMETUNER 50
#define API_DISABLE_TIMETUNER 51
#define API_ENABLE_GARDENER 52
#define API_DISABLE_GARDENER 53
#define API_ENABLE_CLEARCACHE 54
#define API_DISABLE_CLEARCACHE 55
#define API_ENABLE_QUICKSTART 56
#define API_DISABLE_QUICKSTART 57
#define API_ENABLE_STANDALONE 58
#define API_DISABLE_STANDALONE 59
#define API_ENABLE_PUSH 60
#define API_DISABLE_PUSH 61
#define API_ENABLE_METRICS_PUSH 62
#define API_DISABLE_METRICS_PUSH 63
#define API_SET_SCHEDULER_TYPE 69
#define API_SET_PLUGINOUTPUT 70
#define API_SET_SAVEONEXIT 71
#define API_SET_SLEEP 73
#define API_SET_HOSTNAME 75
#define API_SET_METRICSPREFIX 76
#define API_SET_JSONFILENAME 77
#define API_SET_METRICSFILENAME 78
#define API_SET_MAINTENANCE_STATUS 80
#define API_SET_PUSH_URL 300
#define API_SET_PUSH_PORT 301
#define API_SET_PUSH_INTERVAL 302
#define API_GET_PLUGINOUTPUT 81
#define API_GET_SLEEP 83
#define API_GET_SAVEONEXIT 84
#define API_GET_HOSTNAME 85
#define API_GET_METRICSPREFIX 86
#define API_GET_JSONFILENAME 87
#define API_GET_METRICSFILENAME 88
#define API_GET_PLUGIN_RELOAD_TS 91
#define API_GET_SCHEDULER 113
#define API_GET_PUSH_URL 303
#define API_GET_PUSH_PORT 304
#define API_GET_PUSH_INTERVAL 305
#define API_CHECK_PLUGIN_CONFIG 92
#define API_RELOAD_ALMOND 93
#define API_RELOAD_CONFIG_HARD 94
#define API_RELOAD_CONFIG_SOFT 95
#define API_ALMOND_VERSION 110
#define API_ALMOND_STATUS 111
#define API_ALMOND_PLUGINSTATUS 112
#define API_MONITOR_SOFT 200
#define API_MONITOR_SOFT_VALUE 201
#define API_MONITOR_HARD 205
#define API_MONITOR_HARD_VALUE 206
#define API_NAME_END 96
#define API_DENIED 66
#define API_ERROR 2

// External declarations for globals used in API
extern SSL_CTX *ctx;
extern SSL *ssl;
extern char *server_message;
extern char *client_message;
extern char *almondCertificate;
extern char *almondKey;
extern char *push_url;
extern bool allowAllHosts;
extern char *hosts_allowed[MAX_HOSTS];
extern int hosts_allowed_count;
extern int push_port;
extern int push_interval;
extern unsigned int total_threads_run;

// Function declarations
void apiMonitorItem(int, int);
void apiMonitorSoftItem(int);
void apiMonitorHardItem(int);
void apiMonitorItemSoftValue(int);
void apiMonitorItemHardValue(int);
void apiReadData(int, int);
void apiDryRun(int);
void apiRunPlugin(int, int);
void runPluginArgs(int, int, int);
void apiRunAndRead(int, int);
void apiGetMetrics();
void apiReadAll();
void apiGetHostName();
void apiGetVars(int);
void apiCheckPluginConf();
void apiReloadConfigHard();
void apiReloadConfigSoft();
void apiReload();
void apiShowVersion();
void apiShowStatus();
void apiShowPluginStatus();
int createSocket(int);

#endif // API_H
