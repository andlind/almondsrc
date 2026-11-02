#ifndef ALMOND_ALERTING_H
#define ALMOND_ALERTING_H

#define ALMOND_ALERTING_CONFIG_PATH "/etc/almond/alerting.conf"
#define ALMOND_ALERTING_MAX_CHECKS 256

typedef struct {
    char *slack_webhook_url;
    char *smtp_url;
    char *smtp_username;
    char *smtp_password;
    char *email_from;
    char *email_recipient;
    char *email_subject;
    char *ilert_api_key;
    char *ilert_webhook_url;
    char *prometheus_alertmanager_url;
    char *pagerduty_routing_key;
    char *opsgenie_api_key;
    int opsgenie_p2_priority;
    int opsgenie_p5_priority;
    int enabled;
    int configured;
} AlertRoute;

typedef struct {
    int send_alerts_to_slack;
    int send_alerts_to_email;
    int send_alerts_to_ilert;
    int send_alerts_to_prometheus;
    int send_alerts_to_pagerduty;
    int send_alerts_to_opsgenie;
    char *slack_webhook_url;
    char *smtp_url;
    char *smtp_username;
    char *smtp_password;
    char *email_from;
    char *email_recipient;
    char *email_subject;
    char *ilert_api_key;
    char *ilert_webhook_url;
    char *prometheus_alertmanager_url;
    char *pagerduty_routing_key;
    char *opsgenie_api_key;
    int opsgenie_p2_priority;
    int opsgenie_p5_priority;
    AlertRoute checks[ALMOND_ALERTING_MAX_CHECKS];
} AlertConfig;

struct PluginItem;

typedef struct {
    const char *host;
    const char *check_name;
    const char *state;
    const char *summary;
    const char *details;
    const char *timestamp;
} AlertDetails;

void alert_config_init(AlertConfig *config);
int alert_config_load(const char *path, AlertConfig *config);
void alert_config_free(AlertConfig *config);
int alert_notify_check(AlertConfig *config,
                       struct PluginItem *item,
                       const AlertDetails *alert,
                       int send_slack,
                       int send_email);

/* Return 0 on success and -1 on error. Configuration values are owned by AlertConfig. */
int alert_send_slack(const char *webhook_url, const AlertDetails *alert);
int alert_send_email(const char *smtp_url,
                     const char *username,
                     const char *password,
                     const char *from,
                     const char *recipient,
                     const char *subject,
                     const AlertDetails *alert);
int alert_send_ilert(const char *api_key, const char *webhook_url, const AlertDetails *alert);
int alert_send_prometheus_alertmanager(const char *alertmanager_url, const AlertDetails *alert);
int alert_send_pagerduty(const char *routing_key, const AlertDetails *alert, int retcode);
int alert_send_opsgenie(const char *api_key, const AlertDetails *alert, int retcode, int p2_priority, int p5_priority);

#endif