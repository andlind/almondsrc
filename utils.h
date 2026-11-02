#ifndef UTILS_H
#define UTILS_H

#include <sys/types.h>
#include "data.h"

// Utility functions
void safe_free_str(char **ptr);
void removeChar(char *str, char garbage);
int load_allowed_hosts(const char *filename);
int is_host_allowed(const char *client_ip);
void add_plugin_pid(pid_t pid);
void remove_plugin_pid(pid_t pid);
int is_plugin_pid(pid_t pid);
TrackedPopen tracked_popen(const char *cmd);
int tracked_pclose(TrackedPopen *tp);
int getNextMessage();
void checkCtMemoryAlloc();
void updateHostName(char * str);
void writePluginResultToFile(int, int);
void writeToKafkaTopic(int, int);

#endif // UTILS_H