#ifndef ALMOND_PLUGINS_H
#define ALMOND_PLUGINS_H

#include "data.h"

// global config
extern char* pluginDeclarationFile;
extern size_t pluginoutput_size;
extern int g_plugin_count;
extern PluginItem **g_plugins;
extern PluginItem *g_plugin_map;
//extern PluginOutput *g_outputs;
extern int decCount;

int countDeclarations(char *file_name);
void run_plugin(PluginItem *item);
// main entry point
//void init_plugins(const char *, size_t *);
int init_plugins();
//void updatePluginDeclarations();
void update_plugins(void);
size_t getPluginCount();
PluginItem *getPluginItem(size_t index);

#endif // ALMOND_PLUGINS_H
