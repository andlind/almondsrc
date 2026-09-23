#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <json-c/json.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "configuration.h"
#include "heal.h"
#include "logger.h"
#include "main.h"

#define HEAL_DEFAULT_TIMEOUT_SECONDS 30
#define HEAL_MAX_RULES 1024
#define HEAL_LOG_FILE "/var/log/almond/heal_actions.log"

typedef struct HealRule {
    int plugin_id;
    char *plugin_name;
    char *command;
    unsigned int timeout_seconds;
} HealRule;

static HealRule *heal_rules = NULL;
static size_t heal_rule_count = 0;
static int heal_trigger_state = 2;
static bool heal_reload_active = false;
static pthread_mutex_t heal_mutex = PTHREAD_MUTEX_INITIALIZER;

extern char *confDir;
extern bool try_to_heal;
extern volatile sig_atomic_t is_stopping;

static void free_rule(HealRule *rule) {
    if (!rule) return;
    free(rule->plugin_name);
    free(rule->command);
}

static void free_rules(HealRule *rules, size_t count) {
    if (!rules) return;
    for (size_t i = 0; i < count; ++i) free_rule(&rules[i]);
    free(rules);
}

static int state_from_name(const char *name) {
    if (!name) return -1;
    if (strcmp(name, "WARNING") == 0) return 1;
    if (strcmp(name, "CRITICAL") == 0) return 2;
    return -1;
}

static int valid_command(const char *command) {
    struct stat st;
    return command && command[0] == '/' && stat(command, &st) == 0 &&
           S_ISREG(st.st_mode) && access(command, X_OK) == 0;
}

static char *config_path(void) {
    const char *directory = confDir ? confDir : "/etc/almond";
    size_t length = strlen(directory) + sizeof("/heal.conf");
    char *path = malloc(length);
    if (path) snprintf(path, length, "%s/%s", directory, "heal.conf");
    return path;
}

int heal_reload_config(void) {
    char *path = config_path();
    struct json_object *root = NULL;
    struct json_object *state = NULL;
    struct json_object *heals = NULL;
    HealRule *new_rules = NULL;
    size_t new_count = 0;
    int new_trigger = 2;

    if (!path) return -1;
    root = json_object_from_file(path);
    if (!root) {
        free(path);
        pthread_mutex_lock(&heal_mutex);
        free_rules(heal_rules, heal_rule_count);
        heal_rules = NULL;
        heal_rule_count = 0;
        pthread_mutex_unlock(&heal_mutex);
        return 0;
    }

    if (!json_object_object_get_ex(root, "react_on_state", &state) ||
        !json_object_object_get_ex(root, "heals", &heals) ||
        !json_object_is_type(state, json_type_string) ||
        !json_object_is_type(heals, json_type_array)) {
        writeLog("Invalid heal.conf: expected react_on_state and heals.", 1, 0);
        json_object_put(root);
        free(path);
        return -1;
    }
    new_trigger = state_from_name(json_object_get_string(state));
    if (new_trigger < 0) {
        writeLog("Invalid heal.conf: react_on_state must be WARNING or CRITICAL.", 1, 0);
        json_object_put(root);
        free(path);
        return -1;
    }

    size_t array_length = json_object_array_length(heals);
    if (array_length > HEAL_MAX_RULES) array_length = HEAL_MAX_RULES;
    if (array_length > 0) {
        new_rules = calloc(array_length, sizeof(*new_rules));
        if (!new_rules) {
            json_object_put(root);
            free(path);
            return -1;
        }
    }
    for (size_t i = 0; i < array_length; ++i) {
        struct json_object *entry = json_object_array_get_idx(heals, i);
        struct json_object *id = NULL;
        struct json_object *plugin = NULL;
        struct json_object *run = NULL;
        struct json_object *timeout = NULL;
        HealRule *rule = &new_rules[new_count];

        if (!entry || !json_object_is_type(entry, json_type_object) ||
            !json_object_object_get_ex(entry, "run", &run) ||
            !json_object_is_type(run, json_type_string)) {
            writeLog("Ignoring malformed heal rule.", 1, 0);
            continue;
        }
        json_object_object_get_ex(entry, "id", &id);
        json_object_object_get_ex(entry, "plugin", &plugin);
        if ((!id || !json_object_is_type(id, json_type_int)) &&
            (!plugin || !json_object_is_type(plugin, json_type_string))) {
            writeLog("Ignoring heal rule without an integer id or plugin name.", 1, 0);
            continue;
        }
        const char *command = json_object_get_string(run);
        if (!valid_command(command)) {
            writeLog("Ignoring heal rule with a missing or non-executable command.", 1, 0);
            continue;
        }
        rule->plugin_id = id ? json_object_get_int(id) : -1;
        rule->plugin_name = plugin ? strdup(json_object_get_string(plugin)) : NULL;
        rule->command = strdup(command);
        rule->timeout_seconds = HEAL_DEFAULT_TIMEOUT_SECONDS;
        if (json_object_object_get_ex(entry, "timeout_seconds", &timeout) &&
            json_object_is_type(timeout, json_type_int) && json_object_get_int(timeout) > 0) {
            rule->timeout_seconds = (unsigned int)json_object_get_int(timeout);
        }
        if (!rule->command || (plugin && !rule->plugin_name)) {
            free_rule(rule);
            continue;
        }
        new_count++;
    }

    pthread_mutex_lock(&heal_mutex);
    free_rules(heal_rules, heal_rule_count);
    heal_rules = new_rules;
    heal_rule_count = new_count;
    heal_trigger_state = new_trigger;
    pthread_mutex_unlock(&heal_mutex);

    snprintf(infostr, infostr_size, "Loaded %zu user-defined heal rules.", new_count);
    writeLog(infostr, 0, 0);
    json_object_put(root);
    free(path);
    return 0;
}

static HealRule *find_rule(PluginItem *item) {
    for (size_t i = 0; i < heal_rule_count; ++i) {
        if ((heal_rules[i].plugin_id >= 0 && heal_rules[i].plugin_id == item->id) ||
            (heal_rules[i].plugin_name && strcmp(heal_rules[i].plugin_name, item->name) == 0)) {
            return &heal_rules[i];
        }
    }
    return NULL;
}

static void log_heal_event(const char *item_name, const char *command, int result, const char *output) {
	FILE *f = fopen(HEAL_LOG_FILE, "a");
    	if (!f) return;

    	time_t now = time(NULL);
    	char timestamp[32];
    	strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    	fprintf(f, "========================================\n");
    	fprintf(f, "[%s] HEAL ATTEMPT\n", timestamp);
    	fprintf(f, "Target Item : %s\n", item_name ? item_name : "Unknown");
    	fprintf(f, "Command     : %s\n", command);
    	fprintf(f, "Result      : %s (exit code %d)\n", (result == 0) ? "SUCCESS" : "FAILED/TIMEOUT", result);
    	if (output && *output) {
        	fprintf(f, "--- Output ---\n%s\n", output);
    	}
    	fprintf(f, "========================================\n\n");

    	fclose(f);
}

static int execute_command(const HealRule *rule) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execl(rule->command, rule->command, (char *)NULL);
        _exit(127);
    }

    time_t deadline = time(NULL) + rule->timeout_seconds;
    int status = 0;
    while (!is_stopping && time(NULL) < deadline) {
        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == pid) return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
        if (result < 0 && errno != EINTR) return -1;
        usleep(100000);
    }
    kill(pid, SIGTERM);
    waitpid(pid, &status, 0);
    return -1;
}

static int execute_command_with_output(const HealRule *rule, char *out_buf, size_t out_size) {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        // Child process
        close(pipefd[0]); // Close unused read end

        // Redirect standard output and standard error to the pipe
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        execl(rule->command, rule->command, (char *)NULL);
        _exit(127);
    }

    // Parent process
    close(pipefd[1]); // Close unused write end in parent

    // Set read end of pipe to non-blocking mode
    int flags = fcntl(pipefd[0], F_GETFL, 0);
    if (flags != -1) {
        fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);
    }

    time_t deadline = time(NULL) + rule->timeout_seconds;
    int status = 0;
    size_t total_bytes_read = 0;

    if (out_buf && out_size > 0) {
        out_buf[0] = '\0';
    }

    while (!is_stopping && time(NULL) < deadline) {
        // Read available data from the pipe
        if (out_buf && total_bytes_read < out_size - 1) {
            ssize_t bytes_read = read(pipefd[0], out_buf + total_bytes_read, out_size - 1 - total_bytes_read);
            if (bytes_read > 0) {
                total_bytes_read += bytes_read;
                out_buf[total_bytes_read] = '\0';
            }
        }

        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == pid) {
            // Process finished; read any remaining output
            if (out_buf && total_bytes_read < out_size - 1) {
                ssize_t bytes_read;
                while ((bytes_read = read(pipefd[0], out_buf + total_bytes_read, out_size - 1 - total_bytes_read)) > 0) {
                    total_bytes_read += bytes_read;
                }
                out_buf[total_bytes_read] = '\0';
            }
            close(pipefd[0]);
            return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
        }

        if (result < 0 && errno != EINTR) {
            close(pipefd[0]);
            return -1;
        }

        usleep(100000); // 100ms
    }

    // Timeout or stopping signal reached: terminate child process
    kill(pid, SIGTERM);
    waitpid(pid, &status, 0);

    // Read remaining data after termination
    if (out_buf && total_bytes_read < out_size - 1) {
        ssize_t bytes_read;
        while ((bytes_read = read(pipefd[0], out_buf + total_bytes_read, out_size - 1 - total_bytes_read)) > 0) {
            total_bytes_read += bytes_read;
        }
        out_buf[total_bytes_read] = '\0';
    }

    close(pipefd[0]);
    return -1;
}

int heal_maybe_run(PluginItem *item, int previous_state) {
    HealRule *rule;
    int trigger_state;
    int result;

    if (!item || !try_to_heal || is_stopping || heal_reload_active ||
        !item->statusInitialized || previous_state >= item->output.retCode) {
        return 0;
    }
    pthread_mutex_lock(&heal_mutex);
    trigger_state = heal_trigger_state;
    rule = find_rule(item);
    if (item->output.retCode < trigger_state || previous_state >= trigger_state ||
        item->heal_attempted) {
        pthread_mutex_unlock(&heal_mutex);
        return 0;
    }
    if (!rule) {
        pthread_mutex_unlock(&heal_mutex);
        writeLog("No heal script available.", 1, 0);
        return 0;
    }
    item->heal_attempted = true;
    char *command = strdup(rule->command);
    unsigned int timeout = rule->timeout_seconds;
    pthread_mutex_unlock(&heal_mutex);

    if (!command) return 0;
    HealRule local_rule = {.command = command, .timeout_seconds = timeout};
    writeLog("Running configured heal action.", 1, 0);
    if (log_heal_command) {
    	char output_buffer[4096] = {0};
    	result = execute_command(&local_rule);
    	result = execute_command_with_output(&local_rule, output_buffer, sizeof(output_buffer));
    	log_heal_event(item->name, command, result, output_buffer);
    }
    else {
	result = execute_command(&local_rule);
    }
    free(command);
    if (result == 0) writeLog("Heal action completed; rerunning plugin check.", 0, 0);
    else writeLog("Heal action failed or timed out.", 2, 0);
    return 1;
}

void heal_record_state(PluginItem *item) {
    if (!item) return;
    pthread_mutex_lock(&heal_mutex);
    if (item->output.retCode < heal_trigger_state) item->heal_attempted = false;
    pthread_mutex_unlock(&heal_mutex);
}

void heal_set_reload_in_progress(bool in_progress) {
    pthread_mutex_lock(&heal_mutex);
    heal_reload_active = in_progress;
    pthread_mutex_unlock(&heal_mutex);
}

void heal_free(void) {
    pthread_mutex_lock(&heal_mutex);
    free_rules(heal_rules, heal_rule_count);
    heal_rules = NULL;
    heal_rule_count = 0;
    pthread_mutex_unlock(&heal_mutex);
}
