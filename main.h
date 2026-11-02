#ifndef MAIN_H
#define MAIN_H

#include <pthread.h>
#include <signal.h>
#include "data.h"

#define TIME_BUF_LEN 80

extern FILE *fptr;
extern char *logmessage;
extern char* logfile;
extern char* logDir;
extern int is_file_open;
extern int shutdown_phase;
extern pthread_cond_t file_opened;
extern size_t logmessage_size;
extern size_t logfile_size;
extern volatile sig_atomic_t is_stopping;

extern char* pluginDir;
extern char* storeDir;
extern char* socket_message;
extern char* customMonitorVals;
extern char* api_args;
extern char* pluginCommand;
extern char* storeName;
extern PluginItem **g_plugins;
extern unsigned short *threadIds;
extern bool timeScheduler;

extern size_t pluginitemname_size;
extern size_t plugincommand_size;
extern size_t storename_size;
extern size_t apimessage_size;
extern size_t pluginitemcmd_size;
extern size_t pluginoutput_size;
extern size_t pluginmessage_size;
extern size_t filename_size;

extern char* pluginReturnString;
extern char* fileName;
extern char* jsonFileName;
extern char* metricsFileName;
extern char* metricsOutputPrefix;
extern char* kafka_tag;
extern char* kafka_topic;
extern char* hostName;
extern unsigned int kafka_start_id;
extern int schedulerSleep;
extern bool saveOnExit;
extern bool logPluginOutput;
extern bool useKafkaConfigFile;
extern bool external_scheduler;
extern time_t tPluginFile;
extern Scheduler *scheduler;

void free_constants(void);
void free_plugin_declarations(void);
void destroy_mutexes(void);

char *trim(char *s);
void setApiCmdFile(char * name, char * value);
void rescheduleChecks();
void createUpdateFile(PluginItem *item, char name[3]);
void apiReadFile(char *fileName, int type);
char* createRunArgsStr(int num, const char* str);
void constructSocketMessage(const char* key, const char* value);
char* getKafkaTopic();
int get_thread_count();
int get_fd_count();
int check_plugin_conf_file(char *declarationFile);
void add_plugin_pid(pid_t pid);
void remove_plugin_pid(pid_t pid);

#endif // MAIN_H
