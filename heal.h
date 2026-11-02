#ifndef ALMOND_HEAL_H
#define ALMOND_HEAL_H

#include <stdbool.h>

#include "data.h"

int heal_reload_config(void);
int heal_maybe_run(PluginItem *item, int previous_state);
void heal_record_state(PluginItem *item);
void heal_set_reload_in_progress(bool in_progress);
void heal_free(void);

extern bool log_heal_command;

#endif
