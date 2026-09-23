#define _POSIX_C_SOURCE 200112L

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alerting.h"
#include "data.h"
#include "logger.h"

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} AlertBuffer;

typedef struct {
    const char *data;
    size_t length;
    size_t offset;
} AlertUpload;

static int buffer_reserve(AlertBuffer *buffer, size_t extra) {
    size_t required = buffer->length + extra + 1;
    size_t capacity;
    char *data;

    if (required <= buffer->capacity) return 0;
    capacity = buffer->capacity == 0 ? 256 : buffer->capacity;
    while (capacity < required) capacity *= 2;
    data = realloc(buffer->data, capacity);
    if (!data) return -1;
    buffer->data = data;
    buffer->capacity = capacity;
    return 0;
}

static int buffer_append(AlertBuffer *buffer, const char *text) {
    size_t length = strlen(text);

    if (buffer_reserve(buffer, length) != 0) return -1;
    memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
    return 0;
}

static char *alert_strdup(const char *text) {
    size_t length;
    char *copy;

    if (!text) return NULL;
    length = strlen(text);
    copy = malloc(length + 1);
    if (!copy) return NULL;
    memcpy(copy, text, length + 1);
    return copy;
}

static char *trim_config_value(char *value) {
    char *end;

    while (*value == ' ' || *value == '\t' || *value == '\r' || *value == '\n') value++;
    end = value + strlen(value);
    while (end > value && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) end--;
    *end = '\0';
    return value;
}

static int set_config_value(char **target, const char *value) {
    char *copy = alert_strdup(value);

    if (!copy) return -1;
    free(*target);
    *target = copy;
    return 0;
}

static int set_route_value(AlertRoute *route, const char *field, const char *value) {
    if (strcmp(field, "slack_webhook_url") == 0)
        return set_config_value(&route->slack_webhook_url, value);
    if (strcmp(field, "smtp_url") == 0)
        return set_config_value(&route->smtp_url, value);
    if (strcmp(field, "smtp_username") == 0)
        return set_config_value(&route->smtp_username, value);
    if (strcmp(field, "smtp_password") == 0)
        return set_config_value(&route->smtp_password, value);
    if (strcmp(field, "email_from") == 0)
        return set_config_value(&route->email_from, value);
    if (strcmp(field, "email_recipient") == 0)
        return set_config_value(&route->email_recipient, value);
    if (strcmp(field, "email_subject") == 0)
        return set_config_value(&route->email_subject, value);
    if (strcmp(field, "ilert_api_key") == 0)
        return set_config_value(&route->ilert_api_key, value);
    if (strcmp(field, "ilert_webhook_url") == 0)
        return set_config_value(&route->ilert_webhook_url, value);
    if (strcmp(field, "prometheus_alertmanager_url") == 0)
        return set_config_value(&route->prometheus_alertmanager_url, value);
    if (strcmp(field, "pagerduty_routing_key") == 0)
        return set_config_value(&route->pagerduty_routing_key, value);
    if (strcmp(field, "opsgenie_api_key") == 0)
        return set_config_value(&route->opsgenie_api_key, value);
    if (strcmp(field, "opsgenie_p2_priority") == 0) {
        route->opsgenie_p2_priority = atoi(value);
        return 0;
    }
    if (strcmp(field, "opsgenie_p5_priority") == 0) {
        route->opsgenie_p5_priority = atoi(value);
        return 0;
    }
    if (strcmp(field, "enabled") == 0) {
        route->enabled = strcmp(value, "false") != 0 && strcmp(value, "0") != 0;
        return 0;
    }
    return 1;
}

void alert_config_init(AlertConfig *config) {
    if (config) memset(config, 0, sizeof(*config));
}

void alert_config_free(AlertConfig *config) {
    int i;

    if (!config) return;
    free(config->slack_webhook_url);
    free(config->smtp_url);
    free(config->smtp_username);
    free(config->smtp_password);
    free(config->email_from);
    free(config->email_recipient);
    free(config->email_subject);
    free(config->ilert_api_key);
    free(config->ilert_webhook_url);
    free(config->prometheus_alertmanager_url);
    free(config->pagerduty_routing_key);
    free(config->opsgenie_api_key);
    for (i = 0; i < ALMOND_ALERTING_MAX_CHECKS; i++) {
        free(config->checks[i].slack_webhook_url);
        free(config->checks[i].smtp_url);
        free(config->checks[i].smtp_username);
        free(config->checks[i].smtp_password);
        free(config->checks[i].email_from);
        free(config->checks[i].email_recipient);
        free(config->checks[i].email_subject);
        free(config->checks[i].ilert_api_key);
        free(config->checks[i].ilert_webhook_url);
        free(config->checks[i].prometheus_alertmanager_url);
        free(config->checks[i].pagerduty_routing_key);
        free(config->checks[i].opsgenie_api_key);
    }
    alert_config_init(config);
}

int alert_config_load(const char *path, AlertConfig *config) {
    FILE *file;
    char line[2048];
    int line_number = 0;
    int result = 0;

    if (!path) path = ALMOND_ALERTING_CONFIG_PATH;
    if (!config) return -1;
    file = fopen(path, "r");
    if (!file) {
        writeLog("Unable to open the alerting configuration file.", LOG_ERROR, 0);
        return -1;
    }

    alert_config_free(config);
    while (fgets(line, sizeof(line), file)) {
        char *key;
        char *value;
        char *separator;
        char route_field[64];
        int check_index;
        int field_result;
        int value_result;

        line_number++;
        key = trim_config_value(line);
        if (*key == '\0' || *key == '#') continue;
        separator = strchr(key, '=');
        if (!separator) {
            snprintf(line, sizeof(line), "Invalid alerting configuration at line %d.", line_number);
            writeLog(line, LOG_ERROR, 0);
            result = -1;
            continue;
        }
        *separator = '\0';
        value = trim_config_value(separator + 1);
        key = trim_config_value(key);

        if (strcmp(key, "send_alerts_to_slack") == 0) {
            config->send_alerts_to_slack = strcmp(value, "false") != 0 && strcmp(value, "0") != 0;
            continue;
        }
        if (strcmp(key, "send_alerts_to_email") == 0) {
            config->send_alerts_to_email = strcmp(value, "false") != 0 && strcmp(value, "0") != 0;
            continue;
        }
        if (strcmp(key, "send_alerts_to_ilert") == 0) {
            config->send_alerts_to_ilert = strcmp(value, "false") != 0 && strcmp(value, "0") != 0;
            continue;
        }
        if (strcmp(key, "send_alerts_to_prometheus") == 0) {
            config->send_alerts_to_prometheus = strcmp(value, "false") != 0 && strcmp(value, "0") != 0;
            continue;
        }
        if (strcmp(key, "send_alerts_to_pagerduty") == 0) {
            config->send_alerts_to_pagerduty = strcmp(value, "false") != 0 && strcmp(value, "0") != 0;
            continue;
        }
        if (strcmp(key, "send_alerts_to_opsgenie") == 0) {
            config->send_alerts_to_opsgenie = strcmp(value, "false") != 0 && strcmp(value, "0") != 0;
            continue;
        }
        if (strcmp(key, "opsgenie_p2_priority") == 0) {
            config->opsgenie_p2_priority = atoi(value);
            continue;
        }
        if (strcmp(key, "opsgenie_p5_priority") == 0) {
            config->opsgenie_p5_priority = atoi(value);
            continue;
        }

        if (sscanf(key, "check.%d.%63s", &check_index, route_field) == 2) {
            if (check_index < 0 || check_index >= ALMOND_ALERTING_MAX_CHECKS) {
                result = -1;
                continue;
            }
            if (!config->checks[check_index].configured)
                config->checks[check_index].enabled = 1;
            field_result = set_route_value(&config->checks[check_index], route_field, value);
            if (field_result == 1) continue;
            config->checks[check_index].configured = 1;
            if (field_result != 0) {
                result = -1;
                break;
            }
            continue;
        }

        if (strcmp(key, "slack_webhook_url") == 0)
            value_result = set_config_value(&config->slack_webhook_url, value);
        else if (strcmp(key, "smtp_url") == 0)
            value_result = set_config_value(&config->smtp_url, value);
        else if (strcmp(key, "smtp_username") == 0)
            value_result = set_config_value(&config->smtp_username, value);
        else if (strcmp(key, "smtp_password") == 0)
            value_result = set_config_value(&config->smtp_password, value);
        else if (strcmp(key, "email_from") == 0)
            value_result = set_config_value(&config->email_from, value);
        else if (strcmp(key, "email_recipient") == 0)
            value_result = set_config_value(&config->email_recipient, value);
        else if (strcmp(key, "email_subject") == 0)
            value_result = set_config_value(&config->email_subject, value);
        else if (strcmp(key, "ilert_api_key") == 0)
            value_result = set_config_value(&config->ilert_api_key, value);
        else if (strcmp(key, "ilert_webhook_url") == 0)
            value_result = set_config_value(&config->ilert_webhook_url, value);
        else if (strcmp(key, "prometheus_alertmanager_url") == 0)
            value_result = set_config_value(&config->prometheus_alertmanager_url, value);
        else if (strcmp(key, "pagerduty_routing_key") == 0)
            value_result = set_config_value(&config->pagerduty_routing_key, value);
        else if (strcmp(key, "opsgenie_api_key") == 0)
            value_result = set_config_value(&config->opsgenie_api_key, value);
        else
            continue;

        if (value_result != 0) {
            writeLog("Unable to allocate memory for alerting configuration.", LOG_ERROR, 0);
            result = -1;
            break;
        }
    }
    fclose(file);
    return result;
}

static const AlertRoute *route_for_check(const AlertConfig *config, int check_index) {
    if (check_index >= 0 && check_index < ALMOND_ALERTING_MAX_CHECKS && config->checks[check_index].configured)
        return &config->checks[check_index];
    return NULL;
}

int alert_notify_check(AlertConfig *config, struct PluginItem *item,
                       const AlertDetails *alert, int send_slack, int send_email) {
    const AlertRoute *route;
    const char *slack_url;
    const char *smtp_url;
    const char *smtp_username;
    const char *smtp_password;
    const char *email_from;
    const char *email_recipient;
    const char *email_subject;
    const char *ilert_api_key;
    const char *ilert_webhook_url;
    const char *prometheus_alertmanager_url;
    const char *pagerduty_routing_key;
    const char *opsgenie_api_key;
    int result = 0;
    int check_index;
    int state_changed;

    if (!config || !item || !alert) return -1;
    check_index = item->id;
    if (check_index < 0 || check_index >= ALMOND_ALERTING_MAX_CHECKS) return -1;
    route = route_for_check(config, check_index);
    if (route && !route->enabled) return 0;

    state_changed = !item->alert_state_initialized || item->alert_last_sent_state != item->output.retCode;
    if (state_changed) {
        item->alert_last_sent_state = item->output.retCode;
        item->alert_state_initialized = 1;
        item->alert_slack_sent = 0;
        item->alert_email_sent = 0;
        item->alert_ilert_sent = 0;
        item->alert_prometheus_sent = 0;
        item->alert_pagerduty_sent = 0;
        item->alert_opsgenie_sent = 0;
    }

    slack_url = route && route->slack_webhook_url ? route->slack_webhook_url : config->slack_webhook_url;
    smtp_url = route && route->smtp_url ? route->smtp_url : config->smtp_url;
    smtp_username = route && route->smtp_username ? route->smtp_username : config->smtp_username;
    smtp_password = route && route->smtp_password ? route->smtp_password : config->smtp_password;
    email_from = route && route->email_from ? route->email_from : config->email_from;
    email_recipient = route && route->email_recipient ? route->email_recipient : config->email_recipient;
    email_subject = route && route->email_subject ? route->email_subject : config->email_subject;
    ilert_api_key = route && route->ilert_api_key ? route->ilert_api_key : config->ilert_api_key;
    ilert_webhook_url = route && route->ilert_webhook_url ? route->ilert_webhook_url : config->ilert_webhook_url;
    prometheus_alertmanager_url = route && route->prometheus_alertmanager_url ? route->prometheus_alertmanager_url : config->prometheus_alertmanager_url;
    pagerduty_routing_key = route && route->pagerduty_routing_key ? route->pagerduty_routing_key : config->pagerduty_routing_key;
    opsgenie_api_key = route && route->opsgenie_api_key ? route->opsgenie_api_key : config->opsgenie_api_key;

    if (send_slack && slack_url && *slack_url && !item->alert_slack_sent) {
        if (alert_send_slack(slack_url, alert) == 0) item->alert_slack_sent = 1;
        else result = -1;
    }
    if (send_email && smtp_url && *smtp_url && email_from && *email_from && email_recipient && *email_recipient &&
        !item->alert_email_sent) {
        if (alert_send_email(smtp_url, smtp_username, smtp_password, email_from, email_recipient,
                             email_subject && *email_subject ? email_subject : "Almond monitoring alert", alert) == 0)
            item->alert_email_sent = 1;
        else
            result = -1;
    }
    if (config->send_alerts_to_ilert && ilert_webhook_url && *ilert_webhook_url && !item->alert_ilert_sent) {
        if (alert_send_ilert(ilert_api_key, ilert_webhook_url, alert) == 0) item->alert_ilert_sent = 1;
        else result = -1;
    }
    if (config->send_alerts_to_prometheus && prometheus_alertmanager_url && *prometheus_alertmanager_url && !item->alert_prometheus_sent) {
        if (alert_send_prometheus_alertmanager(prometheus_alertmanager_url, alert) == 0) item->alert_prometheus_sent = 1;
        else result = -1;
    }
    if (config->send_alerts_to_pagerduty && pagerduty_routing_key && *pagerduty_routing_key && !item->alert_pagerduty_sent) {
        if (alert_send_pagerduty(pagerduty_routing_key, alert, item->output.retCode) == 0) item->alert_pagerduty_sent = 1;
        else result = -1;
    }
    if (config->send_alerts_to_opsgenie && opsgenie_api_key && *opsgenie_api_key && !item->alert_opsgenie_sent) {
        int p2_priority = route && route->opsgenie_p2_priority ? route->opsgenie_p2_priority : config->opsgenie_p2_priority;
        int p5_priority = route && route->opsgenie_p5_priority ? route->opsgenie_p5_priority : config->opsgenie_p5_priority;
        if (alert_send_opsgenie(opsgenie_api_key, alert, item->output.retCode, p2_priority, p5_priority) == 0) item->alert_opsgenie_sent = 1;
        else result = -1;
    }
    return result;
}

static int buffer_append_json(AlertBuffer *buffer, const char *text) {
    const unsigned char *current = (const unsigned char *)(text ? text : "");
    char escaped[7];

    while (*current) {
        switch (*current) {
            case '"': if (buffer_append(buffer, "\\\"") != 0) return -1; break;
            case '\\': if (buffer_append(buffer, "\\\\") != 0) return -1; break;
            case '\n': if (buffer_append(buffer, "\\n") != 0) return -1; break;
            case '\r': if (buffer_append(buffer, "\\r") != 0) return -1; break;
            case '\t': if (buffer_append(buffer, "\\t") != 0) return -1; break;
            default:
                if (*current < 0x20) {
                    snprintf(escaped, sizeof(escaped), "\\u%04x", *current);
                    if (buffer_append(buffer, escaped) != 0) return -1;
                } else {
                    if (buffer_reserve(buffer, 1) != 0) return -1;
                    buffer->data[buffer->length++] = (char)*current;
                    buffer->data[buffer->length] = '\0';
                }
        }
        current++;
    }
    return 0;
}

static int buffer_append_html(AlertBuffer *buffer, const char *text) {
    const char *current = text ? text : "";

    while (*current) {
        const char *replacement = NULL;
        switch (*current) {
            case '&': replacement = "&amp;"; break;
            case '<': replacement = "&lt;"; break;
            case '>': replacement = "&gt;"; break;
            case '"': replacement = "&quot;"; break;
            case '\'': replacement = "&#39;"; break;
            default: break;
        }
        if (replacement) {
            if (buffer_append(buffer, replacement) != 0) return -1;
        } else {
            if (buffer_reserve(buffer, 1) != 0) return -1;
            buffer->data[buffer->length++] = *current;
            buffer->data[buffer->length] = '\0';
        }
        current++;
    }
    return 0;
}

static const char *alert_value(const char *value) {
    return value ? value : "";
}

static int build_html(AlertBuffer *buffer, const AlertDetails *alert) {
    if (buffer_append(buffer,
        "<!doctype html><html><body style=\"margin:0;background:#f4f6f8;"
        "font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;color:#17202a;\">"
        "<table role=\"presentation\" width=\"100%\" cellpadding=\"0\" cellspacing=\"0\"><tr><td "
        "style=\"padding:32px 16px\"><table role=\"presentation\" width=\"600\" "
        "cellpadding=\"0\" cellspacing=\"0\" style=\"max-width:600px;margin:auto;background:#fff;"
        "border:1px solid #dfe4ea;border-radius:8px;overflow:hidden\"><tr><td "
        "style=\"padding:24px 28px;background:#153243;color:#fff\"><div style=\"font-size:12px;"
        "letter-spacing:1px;text-transform:uppercase;opacity:.75\">Almond monitoring</div><h1 "
        "style=\"margin:8px 0 0;font-size:24px\">Alert notification</h1></td></tr><tr><td "
        "style=\"padding:28px\"><div style=\"display:inline-block;padding:6px 10px;background:#fff1f0;"
        "color:#b42318;border-radius:4px;font-weight:600;font-size:13px\">") != 0 ||
        buffer_append_html(buffer, alert_value(alert->state)) != 0 ||
        buffer_append(buffer, "</div><h2 style=\"margin:18px 0 8px;font-size:20px\">") != 0 ||
        buffer_append_html(buffer, alert_value(alert->check_name)) != 0 ||
        buffer_append(buffer, "</h2><p style=\"font-size:16px;line-height:1.5\">") != 0 ||
        buffer_append_html(buffer, alert_value(alert->summary)) != 0 ||
        buffer_append(buffer, "</p><table role=\"presentation\" style=\"margin-top:24px;font-size:14px;"
        "line-height:1.8\"><tr><td style=\"padding-right:24px;color:#667085\">Host</td><td>") != 0 ||
        buffer_append_html(buffer, alert_value(alert->host)) != 0 ||
        buffer_append(buffer, "</td></tr><tr><td style=\"padding-right:24px;color:#667085\">Time</td><td>") != 0 ||
        buffer_append_html(buffer, alert_value(alert->timestamp)) != 0 ||
        buffer_append(buffer, "</td></tr></table><pre style=\"margin-top:24px;padding:16px;background:#f8fafc;"
        "border:1px solid #eaecf0;border-radius:4px;white-space:pre-wrap;font:13px monospace\">") != 0 ||
        buffer_append_html(buffer, alert_value(alert->details)) != 0 ||
        buffer_append(buffer, "</pre></td></tr></table></td></tr></table></body></html>") != 0) return -1;
    return 0;
}

static int curl_post(const char *url, const char *payload) {
    CURL *curl = curl_easy_init();
    struct curl_slist *headers = NULL;
    CURLcode result;
    long response_code = 0;

    if (!curl) return -1;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    result = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result == CURLE_OK && response_code >= 200 && response_code < 300 ? 0 : -1;
}

static size_t upload_read(char *buffer, size_t size, size_t count, void *userdata) {
    AlertUpload *upload = userdata;
    size_t available = upload->length - upload->offset;
    size_t requested = size * count;
    size_t amount = available < requested ? available : requested;

    if (amount > 0) {
        memcpy(buffer, upload->data + upload->offset, amount);
        upload->offset += amount;
    }
    return amount;
}

int alert_send_slack(const char *webhook_url, const AlertDetails *alert) {
    AlertBuffer payload = {0};
    int result;

    if (!webhook_url || !alert || buffer_append(&payload, "{\"text\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->summary)) != 0 ||
        buffer_append(&payload, " | ") != 0 ||
        buffer_append_json(&payload, alert_value(alert->check_name)) != 0 ||
        buffer_append(&payload, " (") != 0 ||
        buffer_append_json(&payload, alert_value(alert->state)) != 0 ||
        buffer_append(&payload, ")\"}") != 0) {
        free(payload.data);
        return -1;
    }
    result = curl_post(webhook_url, payload.data);
    free(payload.data);
    if (result != 0) writeLog("Unable to send alert to Slack.", LOG_ERROR, 0);
    return result;
}

int alert_send_email(const char *smtp_url, const char *username, const char *password,
                     const char *from, const char *recipient, const char *subject,
                     const AlertDetails *alert) {
    AlertBuffer html = {0};
    AlertBuffer message = {0};
    AlertUpload upload;
    CURL *curl = NULL;
    struct curl_slist *recipients = NULL;
    CURLcode result;
    int return_code = -1;

    if (!smtp_url || !from || !recipient || !subject || !alert || build_html(&html, alert) != 0) goto cleanup;
    if (buffer_append(&message, "To: ") != 0 || buffer_append(&message, recipient) != 0 ||
        buffer_append(&message, "\r\nFrom: ") != 0 || buffer_append(&message, from) != 0 ||
        buffer_append(&message, "\r\nSubject: ") != 0 || buffer_append(&message, subject) != 0 ||
        buffer_append(&message, "\r\nMIME-Version: 1.0\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n") != 0 ||
        buffer_append(&message, html.data) != 0) goto cleanup;

    curl = curl_easy_init();
    if (!curl) goto cleanup;
    upload.data = message.data;
    upload.length = message.length;
    upload.offset = 0;
    recipients = curl_slist_append(NULL, recipient);
    curl_easy_setopt(curl, CURLOPT_URL, smtp_url);
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from);
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, upload_read);
    curl_easy_setopt(curl, CURLOPT_READDATA, &upload);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)message.length);
    curl_easy_setopt(curl, CURLOPT_USERNAME, username ? username : "");
    curl_easy_setopt(curl, CURLOPT_PASSWORD, password ? password : "");
    curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    result = curl_easy_perform(curl);
    return_code = result == CURLE_OK ? 0 : -1;

cleanup:
    if (return_code != 0) writeLog("Unable to send alert email.", LOG_ERROR, 0);
    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);
    free(message.data);
    free(html.data);
    return return_code;
}

int alert_send_ilert(const char *api_key, const char *webhook_url, const AlertDetails *alert) {
    AlertBuffer payload = {0};
    CURL *curl = NULL;
    struct curl_slist *headers = NULL;
    CURLcode result;
    long response_code = 0;
    int return_code = -1;

    if (!webhook_url || !alert) return -1;

    /* Build ilert JSON payload */
    if (buffer_append(&payload, "{\"eventType\":\"ALERT\",\"summary\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->summary)) != 0 ||
        buffer_append(&payload, "\",\"details\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->details)) != 0 ||
        buffer_append(&payload, "\",\"check_name\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->check_name)) != 0 ||
        buffer_append(&payload, "\",\"host\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->host)) != 0 ||
        buffer_append(&payload, "\",\"state\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->state)) != 0 ||
        buffer_append(&payload, "\",\"timestamp\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->timestamp)) != 0 ||
        buffer_append(&payload, "\"}") != 0) {
        goto cleanup;
    }

    curl = curl_easy_init();
    if (!curl) goto cleanup;

    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (api_key && *api_key) {
        AlertBuffer auth_header = {0};
        if (buffer_append(&auth_header, "Authorization: Bearer ") == 0 &&
            buffer_append(&auth_header, api_key) == 0) {
            headers = curl_slist_append(headers, auth_header.data);
            free(auth_header.data);
        }
    }

    curl_easy_setopt(curl, CURLOPT_URL, webhook_url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    result = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    if (result == CURLE_OK && response_code >= 200 && response_code < 300)
        return_code = 0;

cleanup:
    if (return_code != 0) writeLog("Unable to send alert to ilert.", LOG_ERROR, 0);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(payload.data);
    return return_code;
}

int alert_send_prometheus_alertmanager(const char *alertmanager_url, const AlertDetails *alert) {
    AlertBuffer payload = {0};
    CURL *curl = NULL;
    struct curl_slist *headers = NULL;
    CURLcode result;
    long response_code = 0;
    int return_code = -1;

    if (!alertmanager_url || !alert) return -1;

    /* Build Prometheus Alertmanager JSON payload */
    if (buffer_append(&payload, "[{\"labels\":{\"alertname\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->check_name)) != 0 ||
        buffer_append(&payload, "\",\"host\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->host)) != 0 ||
        buffer_append(&payload, "\",\"state\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->state)) != 0 ||
        buffer_append(&payload, "\"},\"annotations\":{\"summary\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->summary)) != 0 ||
        buffer_append(&payload, "\",\"details\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->details)) != 0 ||
        buffer_append(&payload, "\"},\"startsAt\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->timestamp)) != 0 ||
        buffer_append(&payload, "\",\"endsAt\":\"\"}]") != 0) {
        goto cleanup;
    }

    curl = curl_easy_init();
    if (!curl) goto cleanup;

    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, alertmanager_url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    result = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    if (result == CURLE_OK && response_code >= 200 && response_code < 300)
        return_code = 0;

cleanup:
    if (return_code != 0) writeLog("Unable to send alert to Prometheus Alertmanager.", LOG_ERROR, 0);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(payload.data);
    return return_code;
}

int alert_send_pagerduty(const char *routing_key, const AlertDetails *alert, int retcode) {
    AlertBuffer payload = {0};
    CURL *curl = NULL;
    struct curl_slist *headers = NULL;
    CURLcode result;
    long response_code = 0;
    int return_code = -1;
    const char *severity;
    char dedup_key[256];

    if (!routing_key || !alert) return -1;

    /* Map status code to PagerDuty severity */
    if (retcode == 0)
        severity = "info";
    else if (retcode == 1)
        severity = "warning";
    else
        severity = "critical";

    /* Create dedup key from check name and host */
    snprintf(dedup_key, sizeof(dedup_key), "almond-%s-%s", alert_value(alert->host), alert_value(alert->check_name));

    /* Build PagerDuty Events API v2 JSON payload */
    if (buffer_append(&payload, "{\"routing_key\":\"") != 0 ||
        buffer_append_json(&payload, routing_key) != 0 ||
        buffer_append(&payload, "\",\"event_action\":\"") != 0 ||
        buffer_append(&payload, retcode == 0 ? "resolve" : "trigger") != 0 ||
        buffer_append(&payload, "\",\"dedup_key\":\"") != 0 ||
        buffer_append_json(&payload, dedup_key) != 0 ||
        buffer_append(&payload, "\",\"payload\":{\"summary\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->summary)) != 0 ||
        buffer_append(&payload, "\",\"severity\":\"") != 0 ||
        buffer_append(&payload, severity) != 0 ||
        buffer_append(&payload, "\",\"source\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->host)) != 0 ||
        buffer_append(&payload, "\",\"custom_details\":{\"check_name\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->check_name)) != 0 ||
        buffer_append(&payload, "\",\"state\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->state)) != 0 ||
        buffer_append(&payload, "\",\"details\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->details)) != 0 ||
        buffer_append(&payload, "\",\"timestamp\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->timestamp)) != 0 ||
        buffer_append(&payload, "\"}}}") != 0) {
        goto cleanup;
    }

    curl = curl_easy_init();
    if (!curl) goto cleanup;

    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, "https://events.pagerduty.com/v2/enqueue");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    result = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    if (result == CURLE_OK && response_code >= 200 && response_code < 300)
        return_code = 0;

cleanup:
    if (return_code != 0) writeLog("Unable to send alert to PagerDuty.", LOG_ERROR, 0);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(payload.data);
    return return_code;
}

int alert_send_opsgenie(const char *api_key, const AlertDetails *alert, int retcode, int p2_priority, int p5_priority) {
    AlertBuffer payload = {0};
    CURL *curl = NULL;
    struct curl_slist *headers = NULL;
    AlertBuffer auth_header = {0};
    CURLcode result;
    long response_code = 0;
    int return_code = -1;
    const char *priority_str;
    int priority_num;

    if (!api_key || !alert) return -1;

    /* Map status code to Opsgenie priority */
    if (retcode == 0) {
        priority_num = 5;
        priority_str = "P5";
    } else if (retcode == 1) {
        priority_num = p2_priority ? p2_priority : 2;
        priority_str = priority_num == 1 ? "P1" : (priority_num == 2 ? "P2" : "P3");
    } else if (retcode == 2) {
        priority_num = p5_priority ? p5_priority : 5;
        priority_str = priority_num == 4 ? "P4" : "P5";
    } else {
        /* retcode == 0 when closing */
        priority_num = 4;
        priority_str = "P4";
    }

    /* Build Opsgenie REST API JSON payload */
    if (buffer_append(&payload, "{\"message\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->check_name)) != 0 ||
        buffer_append(&payload, "\",\"description\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->summary)) != 0 ||
        buffer_append(&payload, "\",\"details\":{\"state\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->state)) != 0 ||
        buffer_append(&payload, "\",\"output\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->details)) != 0 ||
        buffer_append(&payload, "\",\"timestamp\":\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->timestamp)) != 0 ||
        buffer_append(&payload, "\"},\"priority\":\"") != 0 ||
        buffer_append(&payload, priority_str) != 0 ||
        buffer_append(&payload, "\",\"tags\":[\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->host)) != 0 ||
        buffer_append(&payload, "\",\"") != 0 ||
        buffer_append_json(&payload, alert_value(alert->check_name)) != 0 ||
        buffer_append(&payload, "\"],\"alias\":\"almond-") != 0 ||
        buffer_append_json(&payload, alert_value(alert->host)) != 0 ||
        buffer_append(&payload, "-") != 0 ||
        buffer_append_json(&payload, alert_value(alert->check_name)) != 0 ||
        buffer_append(&payload, "\"}") != 0) {
        goto cleanup;
    }

    curl = curl_easy_init();
    if (!curl) goto cleanup;

    /* Build Authorization header */
    if (buffer_append(&auth_header, "Authorization: GenieKey ") == 0 &&
        buffer_append(&auth_header, api_key) == 0) {
        headers = curl_slist_append(headers, auth_header.data);
    }
    free(auth_header.data);
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.opsgenie.com/v2/alerts");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    result = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    if (result == CURLE_OK && response_code >= 200 && response_code < 300)
        return_code = 0;

cleanup:
    if (return_code != 0) writeLog("Unable to send alert to Opsgenie.", LOG_ERROR, 0);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(payload.data);
    return return_code;
}
