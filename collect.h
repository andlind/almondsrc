#ifndef ALMOND_COLLECTOR_STRUCTURES_HEADER
#define ALMOND_COLLECTOR_STRUCTURES_HEADER

#include <stdbool.h>
#include <json-c/json.h>

struct json_object* create_labels_json();
struct json_object* read_labels(const char *);
struct json_object* get_system_info(bool);
struct json_object* parse_perfdata(const char *);

#endif
