#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <json-c/json.h>
struct json_object *parse_perfdata(const char *plugin_output);
int main(void) {
    assert(parse_perfdata(NULL) == NULL);
    assert(parse_perfdata("OK no metrics") == NULL);
    for (int i=0; i<2500; ++i) {
        struct json_object *obj = parse_perfdata("OK | cpu=95%;80;90;0;100 memory=5.5;10;20;0;50");
        assert(obj != NULL);
        struct json_object *cpu = NULL, *value = NULL, *unit = NULL;
        assert(json_object_object_get_ex(obj, "cpu", &cpu));
        assert(json_object_object_get_ex(cpu, "value", &value));
        assert(json_object_object_get_ex(cpu, "unit", &unit));
        assert(json_object_get_string(value)[0] == '9');
        assert(json_object_get_string(unit)[0] == 'p');
        json_object_put(obj);
    }
    puts("parse_perfdata basic ownership test passed");
    return 0;
}
