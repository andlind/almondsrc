#include <stdio.h>
#include <stdbool.h>
#include <json-c/json.h>
#include "collect.h"

int main() {
    //bool showVerbose = true;
    //struct json_object *json = create_labels_json();
    struct json_object *json = read_labels("/etc/almond/plugins.conf");

    printf("%s\n",
        json_object_to_json_string_ext(json, JSON_C_TO_STRING_PRETTY));

    json_object_put(json); // free memory

    //struct json_object *info = get_system_info(showVerbose);

    //printf("%s\n",
    //    json_object_to_json_string_ext(info, JSON_C_TO_STRING_PRETTY));

    //json_object_put(info);

    struct json_object *met1 = parse_perfdata("CPU STATISTICS OK : user=5.64% system=2.56%, iowait=0.00%, idle=91.79%, nice=0.00%, steal=0.00% | CpuUser=5.64%;95;100;0; CpuSystem=2.56%;95;100;0; CpuIowait=0.00%;60;100;0; CpuIdle=91.79%;0;0;0; CpuNice=0.00%;0;0;0; CpuSteal=0.00%;0;0;0;");

    printf("%s\n",
        json_object_to_json_string_ext(met1, JSON_C_TO_STRING_PRETTY));

    json_object_put(met1);

    struct json_object *met2 = parse_perfdata("OK - 127.0.0.1: rta 0.088ms lost 0%|rta=0.088ms;100.000;500.000;0; pl=0%;20;60;0;100 rtmax=0.415ms;;;; rtmin=0.005ms;;;; ");

    printf("%s\n",
        json_object_to_json_string_ext(met2, JSON_C_TO_STRING_PRETTY));

    json_object_put(met1);

    struct json_object *met3 = parse_perfdata("OK - load average: 0.63, 0.50, 0.53|load1=0.630;70.000;90.000;0; load5=0.500;70.000;90.000;0; load15=0.530;70.000;90.000;0; ");

    printf("%s\n",
        json_object_to_json_string_ext(met3, JSON_C_TO_STRING_PRETTY));

    json_object_put(met1);

    struct json_object *met4 = parse_perfdata("OK (Percent free: 65.6%, Total: 5924.6328125 MB, Used: 2040.17578125 MB, Buffers: 128.05078125 MB, Cached: 2734.98046875 MB) | free=65.6 total=5924.6328125 used=2040.17578125 buffers=128.05078125 cached=2734.98046875 ");

    printf("%s\n",
        json_object_to_json_string_ext(met4, JSON_C_TO_STRING_PRETTY));

    json_object_put(met1);

    struct json_object *met5 = parse_perfdata("OK: howru RUNNING | pid=8 uptime=5:30:45 ");

    printf("%s\n",
        json_object_to_json_string_ext(met5, JSON_C_TO_STRING_PRETTY));

    json_object_put(met1);
    return 0;
}

