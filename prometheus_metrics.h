#ifndef PROMETHEUS_METRICS_H
#define PROMETHEUS_METRICS_H

#include <json-c/json.h>

struct prometheus_metric {
    const char *json_key;
    const char *metric_name;
    const char *help;
    const char *type;
    double multiplier;
};

/*
 * Convert the status JSON object to Prometheus text.
 *
 * Returns a malloc-allocated string.
 * The caller must free() the returned pointer.
 * Returns NULL on failure.
 */
char *writePrometheusMetrics(json_object *jobj);

#endif /* PROMETHEUS_METRICS_H */
