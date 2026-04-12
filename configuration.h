#ifndef ALMOND_CONFIG_STRUCTURES_HEADER
#define ALMOND_CONFIG_STRUCTURES_HEADER

#include <stdbool.h>

typedef struct {
	int intval;
	char* strval;
} ConfVal;

typedef struct ConfigEntry {
	const char* name;
	void (*process)(ConfVal value);
} ConfigEntry;

// Configuration processing functions
void process_allow_all_hosts(ConfVal);
void process_almond_api(ConfVal);
void process_almond_port(ConfVal);
void process_almond_standalone(ConfVal);
void procrss_iam_aud(ConfVal);
void process_iam_issuer(ConfVal);
void process_iam_public_key_file(ConfVal);
void process_iam_roles_accepted(ConfVal);
void process_json_file(ConfVal);
void process_metrics_file(ConfVal);
void process_metrics_output_prefix(ConfVal);
void process_save_on_exit(ConfVal);
void process_plugin_declaration(ConfVal);
void process_plugin_directory(ConfVal);
void process_almond_certificate(ConfVal);
void process_clear_data_cache_interval(ConfVal);
void process_conf_dir(ConfVal);
void process_data_cache_time_frame(ConfVal);
void process_enable_clear_data_cache(ConfVal);
void process_enable_gardener(ConfVal);
void process_enable_iam_aud(ConfVal);
void process_enable_iam_roles(ConfVal);
void process_enable_kafka_export(ConfVal);
void process_enable_kafka_id(ConfVal);
void process_enable_kafka_ssl(ConfVal);
void process_enable_kafka_tags(ConfVal);
void process_almond_format(ConfVal);
void process_gardener_run_interval(ConfVal);
void process_gardener_script(ConfVal);
void process_host_name(ConfVal);
void process_init_sleep(ConfVal);
void process_kafka_ca_certificate(ConfVal);
void process_kafka_config_file(ConfVal);
void process_kafka_producer_certificate(ConfVal);
void process_kafka_start_id(ConfVal);
void process_kafka_tag(ConfVal);
void process_almond_key(ConfVal);
void process_data_dir(ConfVal);
void process_log_dir(ConfVal);
void process_log_plugin_output(ConfVal);
void process_log_to_stdout(ConfVal);
void process_almond_quickstart(ConfVal);
void process_run_gardener_at_start(ConfVal);
void process_store_results(ConfVal);
void process_almond_sleep(ConfVal);
void process_store_dir(ConfVal);
void process_truncate_log(ConfVal);
void process_truncate_log_interval(ConfVal);
void process_tune_master(ConfVal);
void process_tune_cycle(ConfVal);
void process_tune_timer(ConfVal);
void process_almond_scheduler_type(ConfVal);
void process_almond_api_tls(ConfVal);
void process_external_scheduler(ConfVal);
void process_schema_registry_url(ConfVal);
void process_schema_name(ConfVal);
void process_use_kafka_config(ConfVal);
void process_almond_push(ConfVal);
void process_metrics_push(ConfVal);
void process_push_url(ConfVal);
void process_push_port(ConfVal);
void process_push_interval(ConfVal);

int parse__conf_line(char *buf);

// External declarations for globals used in config processing
extern bool enableIamAud;
extern bool enableIamRoles;
extern bool local_api;
extern bool use_push;
extern bool use_metrics_push;
extern int config_memalloc_fails;
extern int local_port;
extern int push_port;
extern int push_interval;
extern int iam_roles_count;
extern bool standalone;
extern bool use_ssl;
extern bool kafkaAvro;
extern char allowed_hosts_file[26];
extern char* jsonFileName;
extern char* metricsFileName;
extern char* infostr;
extern char* push_url;
extern char* iam_aud;
extern char* iam_issuer;
extern char* iam_public_key_file;
extern char **iam_roles_accepted;
extern size_t jsonfilename_size;
extern size_t metricsfilename_size;
extern size_t infostr_size;

#endif

