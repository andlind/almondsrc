#define _GNU_SOURCE

#include "prometheus_metrics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct prometheus_metric prometheus_metrics[] = {
    { "pid", "process_id",
      "Process identifier.", "gauge", 1.0 },

    { "uptime_seconds", "process_uptime_seconds",
      "Process uptime in seconds.", "gauge", 1.0 },

    { "plugin_count", "plugin_count",
      "Number of loaded plugins.", "gauge", 1.0 },

    { "user_cpu_time", "process_cpu_user_seconds_total",
      "Total user CPU time in seconds.", "counter", 1.0 },

    { "system_cpu_tume", "process_cpu_system_seconds_total",
      "Total system CPU time in seconds.", "counter", 1.0 },

    { "heap_allocated_kb", "process_heap_allocated_bytes",
      "Allocated heap memory in bytes.", "gauge", 1024.0 },

    { "heap_total_kb", "process_heap_total_bytes",
      "Total heap arena memory in bytes.", "gauge", 1024.0 },

    { "max_resident_set_size_kb", "process_max_resident_memory_bytes",
      "Maximum resident memory in bytes.", "gauge", 1024.0 },

    { "rss_kb", "process_resident_memory_bytes",
      "Current resident memory in bytes.", "gauge", 1024.0 },

    { "stack_size_kb", "process_stack_limit_bytes",
      "Process stack limit in bytes.", "gauge", 1024.0 },

    { "minor_page_faults", "process_minor_page_faults_total",
      "Minor page faults.", "counter", 1.0 },

    { "major_page_faults", "process_major_page_faults_total",
      "Major page faults.", "counter", 1.0 },

    { "swaps", "process_swaps_total",
      "Swap operations reported by getrusage.", "counter", 1.0 },

    { "block_input_ops", "process_block_input_operations_total",
      "Block input operations.", "counter", 1.0 },

    { "block_output_ops", "process_block_output_operations_total",
      "Block output operations.", "counter", 1.0 },

    { "ipc_msgs_sent", "process_ipc_messages_sent_total",
      "IPC messages sent.", "counter", 1.0 },

    { "ipc_msgs_received", "process_ipc_messages_received_total",
      "IPC messages received.", "counter", 1.0 },

    { "signals_received", "process_signals_received_total",
      "Signals received.", "counter", 1.0 },

    { "voluntary_context_switches",
      "process_context_switches_voluntary_total",
      "Voluntary context switches.", "counter", 1.0 },

    { "involuntary_context_switches",
      "process_context_switches_involuntary_total",
      "Involuntary context switches.", "counter", 1.0 },

    { "thread_count", "process_threads",
      "Number of currently running threads.", "gauge", 1.0 },

    { "total_threads_since_start", "process_threads_started_total",
      "Total threads started since application start.", "counter", 1.0 },

    { "open_file_descriptors", "process_open_file_descriptors",
      "Number of currently open file descriptors.", "gauge", 1.0 },

    { "rchar", "process_io_read_chars_total",
      "Characters returned by read-like system calls.", "counter", 1.0 },

    { "wchar", "process_io_written_chars_total",
      "Characters passed to write-like system calls.", "counter", 1.0 },

    { "syscr", "process_io_read_syscalls_total",
      "Read-like system calls.", "counter", 1.0 },

    { "syscw", "process_io_write_syscalls_total",
      "Write-like system calls.", "counter", 1.0 },

    { "read_bytes", "process_io_storage_read_bytes_total",
      "Bytes fetched from storage.", "counter", 1.0 },

    { "write_bytes", "process_io_storage_written_bytes_total",
      "Bytes sent to storage.", "counter", 1.0 },

    { "cancelled_write_bytes", "process_io_cancelled_write_bytes_total",
      "Cancelled write bytes.", "counter", 1.0 }
};

static void prometheusWriteLabel(FILE *out, const char *value)
{
    if (value == NULL)
        return;

    for (const char *p = value; *p != '\0'; ++p) {
        switch (*p) {
        case '\\':
            fputs("\\\\", out);
            break;
        case '"':
            fputs("\\\"", out);
            break;
        case '\n':
            fputs("\\n", out);
            break;
        default:
            fputc(*p, out);
            break;
        }
    }
}

char *writePrometheusMetrics(json_object *jobj)
{
    char *result = NULL;
    size_t result_size = 0;

    FILE *out = open_memstream(&result, &result_size);
    if (out == NULL)
        return NULL;

    const char *hostname = "unknown";
    const char *version = "unknown";
    json_object *value_obj = NULL;

    if (json_object_object_get_ex(jobj, "hostname", &value_obj) &&
        json_object_is_type(value_obj, json_type_string)) {
        hostname = json_object_get_string(value_obj);
    }

    if (json_object_object_get_ex(jobj, "almond_version", &value_obj) &&
        json_object_is_type(value_obj, json_type_string)) {
        version = json_object_get_string(value_obj);
    }

    const size_t count =
        sizeof(prometheus_metrics) / sizeof(prometheus_metrics[0]);

    for (size_t i = 0; i < count; ++i) {
        const struct prometheus_metric *metric = &prometheus_metrics[i];
        double value;

        if (!json_object_object_get_ex(jobj, metric->json_key,
                                       &value_obj)) {
            continue;
        }

        enum json_type value_type = json_object_get_type(value_obj);
        if (value_type != json_type_int &&
            value_type != json_type_double) {
            continue;
        }

        value = json_object_get_double(value_obj);

        fprintf(out, "# HELP almond_%s %s\n",
                metric->metric_name, metric->help);

        fprintf(out, "# TYPE almond_%s %s\n",
                metric->metric_name, metric->type);

        fprintf(out, "almond_%s{hostname=\"",
                metric->metric_name);

        prometheusWriteLabel(out, hostname);

        fputs("\",version=\"", out);
        prometheusWriteLabel(out, version);

        fprintf(out, "\"} %.17g\n\n",
                value * metric->multiplier);
    }

    if (fclose(out) != 0) {
        free(result);
        return NULL;
    }

    return result;
}
