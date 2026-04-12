#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <assert.h>
#ifdef __linux__
#include <malloc.h>
#endif
#include <sys/types.h>
#include "data.h"
#include "plugins.h"

#define STATUS_SIZE 2
#define PLUGIN_LINE_MAX 1024

/* Helper utilities */
static char *safe_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (!d) {
        perror("safe_strdup");
        return NULL;
    }
    memcpy(d, s, n);
    return d;
}

static void free_plugin_item(PluginItem *it, int free_retstring) {
    if (!it) return;
    if (it->name) free(it->name);
    if (it->description) free(it->description);
    if (it->command) free(it->command);
    if (free_retstring && it->output.retString) free(it->output.retString);
    free(it);
}

/* Extract the base command (existing) */
static char *extract_base_command(const char *cmd) {
    if (!cmd) return NULL;

    char *space = strchr(cmd, ' ');
    size_t len = space ? (size_t)(space - cmd) : strlen(cmd);
   
    char *base = malloc(len + 1);
    if (!base) return NULL;

    strncpy(base, cmd, len);
    base[len] = '\0';
    return base;
}

static PluginItem *parse_declaration_line(const char *line) {
    if (!line) return NULL;
    char buf[PLUGIN_LINE_MAX];
    strncpy(buf, line, sizeof(buf));
    buf[sizeof(buf)-1] = '\0';

    // Skip comments and short lines
    if (buf[0] == '#' || strlen(buf) < 5) return NULL;

    // Strip newline
    buf[strcspn(buf, "\n")] = '\0';

    // Find [name]…;…;…;…
    char *start = strchr(buf, '[');
    char *end   = strchr(buf, ']');
    if (!start || !end || end <= start) return NULL;
    *end = '\0';
    char *name = start + 1;

    // Split remainder by ';'
    char *rest = end + 1;
    while (*rest == ' ') rest++;
    char *description = strtok(rest, ";");
    char *command     = strtok(NULL, ";");
    char *active_str  = strtok(NULL, ";");
    char *interval_str= strtok(NULL, ";");
    if (!description || !command || !active_str || !interval_str) return NULL;

    PluginItem *item = calloc(1, sizeof *item);
    if (!item) {
        perror("calloc PluginItem");
        return NULL;
    }

    item->name        = safe_strdup(name);
    item->description = safe_strdup(description);
    item->command     = safe_strdup(command);
    item->active      = atoi(active_str);
    item->interval    = atoi(interval_str);
    item->touched     = false;

    return item;
}

static PluginItem **load_declarations(const char *path, int *out_count) {
    FILE *f = fopen(path, "r");
    if (!f) {
        perror("Failed to open plugin declaration file");
        return NULL;
    }

    PluginItem **list = NULL;
    size_t cap = 0, n = 0;
    char line[PLUGIN_LINE_MAX];

    while (fgets(line, sizeof line, f)) {
        PluginItem *item = parse_declaration_line(line);
        if (!item) continue;

        if (n + 1 > cap) {
            cap = cap ? cap * 2 : 16;
            PluginItem **tmp = realloc(list, cap * sizeof *list);
            if (!tmp) {
                perror("realloc in load_declarations");
                // cleanup partial list
                for (size_t i = 0; i < n; ++i) free_plugin_item(list[i], 1);
                free(list);
                fclose(f);
                return NULL;
            }
            list = tmp;
        }
        list[n++] = item;
    }

    fclose(f);
    *out_count = (int)n;
    return list;
}

void load_plugins() {
    FILE *fp = fopen(pluginDeclarationFile, "r");
    if (!fp) {
        perror("Failed to open config file");
        return;
    }

    PluginItem **new_list = calloc(decCount ? decCount : 16, sizeof *new_list);
    if (!new_list) {
        perror("Memory allocation failed for plugin list");
        fclose(fp);
        return;
    }

    char line[PLUGIN_LINE_MAX];
    int index = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (index >= decCount) break;
        PluginItem *item = parse_declaration_line(line);
        if (!item) continue;

        item->id = index;

        item->lastRunTimestamp[0]    = '\0';
        item->nextRunTimestamp[0]    = '\0';
        item->lastChangeTimestamp[0] = '\0';
        strcpy(item->statusChanged, "0");
        item->nextRun = 0;

        item->output.retCode     = 0;
        item->output.prevRetCode = 0;
        item->output.retString   = safe_strdup("");

        item->touched = true;
        HASH_ADD_KEYPTR(hh, g_plugin_map, item->name, strlen(item->name), item);

        new_list[index++] = item;
    }

    fclose(fp);

    free(g_plugins);
    g_plugins      = new_list;
    g_plugin_count = index;
}

int init_plugins() {
	printf("Initiate plugins\n");
	load_plugins();
	if (!g_plugins) {
		return 1;
	}
	printf("Loaded %d plugins\n", g_plugin_count);
        for (int i = 0; i < g_plugin_count; i++) {
            /* optionally: debug print
            printf("  [%d] name=%s cmd=\"%s\"\n",
                    i,
                    g_plugins[i]->name,
                    g_plugins[i]->command);
            */
            //runPlugin(i);
        }
	return 0;
}

void update_plugins(void) {
    // 1) Load new config declarations
    int newCount = 0;
    PluginItem **decls = load_declarations(pluginDeclarationFile, &newCount);
    if (!decls) {
        return;
    }
    if (newCount <= 0) {
        free(decls);
        return;
    }

    // 2) Steal the old array and count
    PluginItem **old_plugs = g_plugins;
    size_t       oldCount  = decCount;

    // 3) Allocate and immediately swap in the new g_plugins[]
    PluginItem **new_array = calloc((size_t)newCount, sizeof *new_array);
    if (!new_array) {
        perror("calloc new g_plugins");
        // roll back; g_plugins was untouched
        free(decls);
        return;
    }
    g_plugins   = new_array;
    decCount    = newCount;

    // 4) Mark all old items untouched
    for (size_t j = 0; j < oldCount; ++j) {
        if (old_plugs && old_plugs[j]) {
            old_plugs[j]->touched = false;
        }
    }

    // 5) Diff/merge in file order
    for (int i = 0; i < newCount; ++i) {
        PluginItem *cfg    = decls[i];
        cfg->id            = i;  // enforce file-order id
        bool reused        = false;
        char *new_bc       = extract_base_command(cfg->command);

        // Try to match an old plugin by name + base_command
        for (size_t j = 0; j < oldCount; ++j) {
            PluginItem *old = old_plugs ? old_plugs[j] : NULL;
            if (!old)
                continue;

            if (strcmp(old->name, cfg->name) == 0) {
                char *old_bc = extract_base_command(old->command);
                if (old_bc && new_bc && strcmp(old_bc, new_bc) == 0) {
                    // Reuse runtime fields + output
                    cfg->nextRun = old->nextRun;

                    // Copy timestamps (ensure termination)
                    strncpy(cfg->lastRunTimestamp,    old->lastRunTimestamp,    TIMESTAMP_SIZE);
                    cfg->lastRunTimestamp[TIMESTAMP_SIZE - 1] = '\0';
                    strncpy(cfg->nextRunTimestamp,    old->nextRunTimestamp,    TIMESTAMP_SIZE);
                    cfg->nextRunTimestamp[TIMESTAMP_SIZE - 1] = '\0';
                    strncpy(cfg->lastChangeTimestamp, old->lastChangeTimestamp, TIMESTAMP_SIZE);
                    cfg->lastChangeTimestamp[TIMESTAMP_SIZE - 1] = '\0';

                    // Copy statusChanged
                    memcpy(cfg->statusChanged, old->statusChanged, STATUS_SIZE);

                    // Transfer PluginOutput ownership
                    cfg->output = old->output;
                    old->output.retString = NULL; // disown to avoid double-free

                    cfg->touched = true;
                    reused       = true;

                    // Replace in hash: remove old before adding cfg
                    HASH_DEL(g_plugin_map, old);

                    // Free old struct (fields we did NOT transfer)
                    free_plugin_item(old, 0); // retString was transferred (disowned above)

                    // Mark slot as consumed (Step 6 won't see it)
                    old_plugs[j] = NULL;
                }
                free(old_bc);
                break;
            }
        }

	// Add the cfg (both new and reused) to the hash
        HASH_ADD_KEYPTR(hh, g_plugin_map, cfg->name, strlen(cfg->name), cfg);

        // Install in the new array
        g_plugins[i] = cfg;
	
	// Brand-new plugin ?~G~R initialize and run once
        if (!reused) {
            cfg->nextRun                 = 0;
            cfg->lastRunTimestamp[0]     = '\0';
            cfg->nextRunTimestamp[0]     = '\0';
            cfg->lastChangeTimestamp[0]  = '\0';
            strcpy(cfg->statusChanged, "0");

            run_plugin(cfg);
        }
        free(new_bc);
    }

    // 6) Teardown truly removed or renamed plugins
    if (old_plugs) {
        for (size_t j = 0; j < oldCount; ++j) {
            PluginItem *old = old_plugs[j];
            if (!old)
                continue;

            // Remove from hash first
            HASH_DEL(g_plugin_map, old);

            // Free old plugin entirely
            free_plugin_item(old, 1);
        }
        free(old_plugs);
    }

    // 7) Cleanup
    g_plugin_count = decCount;
    free(decls);       // decls holds pointers now owned by g_plugins (or freed above)
#ifdef __linux__
    malloc_trim(0);
#endif
}

PluginItem *getPluginItem(size_t index) {
    if (index < g_plugin_count)
         return g_plugins[index];
    return NULL;
}
