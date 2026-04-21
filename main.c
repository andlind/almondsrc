#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE
#ifndef VERSION
#define VERSION "0.9.28"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <unistd.h>
#include <netdb.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <math.h>
#include <malloc.h>
#include <zlib.h>
#include <stddef.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <json-c/json.h>
#include <curl/curl.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#include "uthash.h"
#include "data.h"
#include "constants.h"
#include "configuration.h"
#include "logger.h"
#include "plugins.h"
#ifdef USE_AVRO 
#include "mod_avro.h"
#else
#include "mod_kafka.h"
#endif
#include "api.h"
#include "kafkaapi.h"
#include "main.h"
#include "jwt_validate.h"

#define MAX_COLUMNS 2
#define MAX_STRING_SIZE 50
#define MAX_CONSTANTS 50
#define JSON_OUTPUT 0
#define METRICS_OUTPUT 1
#define JSON_AND_METRICS_OUTPUT 2
#define PROMETHEUS_OUTPUT 3
#define JSON_AND_PROMETHEUS_OUTPUT 4
#define HOWRU_API 10 
#define KAFKA_EXPORT_TAG 10
#define KAFKA_EXPORT_ID 20
#define KAFKA_EXPORT_IDTAG 30
#define MAX_PLUGINS 256
#define TIME_BUF_LEN 80
#define MAX_THREAD_COUNT 4294967290
/*#define CMD_BUF_SIZE      1024
#define LINE_BUF_SIZE     1024
#define TIMESTAMP_SIZE     64*/
/*#if defined(_BSD_SOURCE) || defined(_SVID_SOURCE) || defined(_XOPEN_SOURCE)
#define HAS_BIRTHTIME 1
#else
#define HAS_BIRTHTIME 0
#endif*/

enum shutdown_reason {
    SR_NORMAL,
    SR_SIGINT,
    SR_SIGKILL,
    SR_SIGTERM,
    SR_SIGSTOP,
    SR_ERROR
};

void safe_free_str(char **ptr);
char constantsFile[26] = "/opt/almond/memalloc.alm";
char allowed_hosts_file[26] = "/etc/almond/allowed_hosts";
char* confDir = NULL;
char* dataDir = NULL;
char* storeDir = NULL;
char* pluginDir = NULL;
char* logDir = NULL;
char* pluginDeclarationFile = NULL;
char* hostName = NULL;
char* fileName = NULL;
char* jsonFileName = NULL;
char* metricsFileName = NULL;
char* gardenerScript = NULL;
char* metricsOutputPrefix = NULL;
char* infostr = NULL;
char* socket_message = NULL;
char* kafka_brokers = NULL;
char* kafka_topic = NULL;
char* kafka_tag = NULL;
char* kafkaCACertificate = NULL;
char* kafkaSSLKey = NULL;
char* kafkaProducerCertificate = NULL;
char* logmessage = NULL;
char* logfile = NULL;
char* dataFileName = NULL;
char* backupDirectory = NULL;
char* newFileName = NULL;
char* gardenerRetString = NULL;
char* pluginCommand = NULL;
char* pluginReturnString = NULL;
char* push_url = NULL;
char* storeName = NULL;
char* server_message = NULL;
//char* client_message = NULL;
char* almondCertificate = NULL;
char* almondKey = NULL;
char* schemaRegistryUrl = NULL;
char* kafkaConfigFile = NULL;
char* customMonitorVals = NULL;
char *iam_public_key = NULL;
char *iam_public_key_file = NULL;
char *iam_issuer = NULL;
char *iam_aud = NULL;
char **iam_roles_accepted = NULL;
char *hosts_allowed[MAX_HOSTS];
char schemaName[100] = "almond-monitor-topic-value"; 
PluginItem **g_plugins   = NULL;
PluginItem *g_plugin_map   = NULL;
PluginItem *update_g_plugins = NULL;
Scheduler *scheduler = NULL;
// `address` declared earlier; avoid duplicate definition here
// struct sockaddr_in address;
SSL_CTX *ctx;
SSL *ssl;
int initSleep;
int updateInterval;
int push_interval = 120;
int push_interval_cnt = 0;
int hosts_allowed_count = 0;
int iam_roles_count = 0;
bool allowAllHosts = true;
bool confDirSet = false;
bool dataDirSet = false;
bool storeDirSet = false;
bool logDirSet = false;
bool pluginDirSet = false;
bool logPluginOutput = false;
bool pluginResultToFile = false;
bool saveOnExit = false;
bool dockerLog = false;
bool enableGardener = false;
bool runGardenerAtStart = false;
bool enableClearDataCache = false;
bool enableIamAud = false;
bool enableIamRoles = false;
bool enableKafkaExport = false;
bool enableKafkaSSL = false;
bool enableKafkaTag = false;
bool enableKafkaId = false;
bool kafkaAvro = false;
bool enableTimeTuner = false;
bool standalone = false;
bool quick_start = false;
bool local_api = false;
bool use_push = false;
bool use_metrics_push = false;
bool external_scheduler = false;
bool useKafkaConfigFile = false;
bool use_ssl = false;
bool truncateLog = false;
bool timeScheduler = false;
int decCount = 0;
int kafkaexportreqs = 0;
int schedulerSleep = 5000;
int timeTunerMaster = 1;
int timeTunerCycle = 15;
int timeTunerCounter = 0;
int local_port = 9909;
int tspr = 0;
int config_memalloc_fails = 0;
int trunc_time = 0;
int max_try = 60;
int push_port = 0;
int g_plugin_count = 0;
static int g_current_scheduler_cnt = 0;
size_t infostr_size = 400;
size_t gardenermessage_size = 1035;
size_t pluginmessage_size = 2300;
size_t storename_size = 100;
size_t apimessage_size = 2000;
size_t socketservermessage_size = 2000;
size_t socketclientmessage_size = 8192;
size_t logmessage_size = 1545;
size_t confdir_size = 50;
size_t datadir_size = 50;
size_t plugindeclarationfile_size = 75;
size_t metricsoutputprefix_size = 30;
size_t datafilename_size = 100;
size_t jsonfilename_size = 50;
size_t metricsfilename_size = 50;
size_t gardenerscript_size = 75;
size_t logdir_size = 50;
size_t hostname_size = 255;
size_t plugindir_size = 50;
size_t pluginitemname_size = 50;
size_t pluginitemdesc_size = 100;
size_t pluginitemcmd_size = 255;
size_t pluginoutput_size = 1500;
size_t plugincommand_size = 100;
size_t newfilename_size = 250;
size_t storedir_size = 50;
size_t backupdirectory_size = 100;
size_t filename_size = 100;
size_t logfile_size = 100;
size_t max_timestamp_size = 64;
int is_file_open = 0;
size_t declaration_size = 0;
size_t output_size = 0;
size_t update_output_size = 0;
size_t update_declaration_size = 0;
signed int truncateLogInterval = 604800;
unsigned int socket_is_ready = 0;
unsigned int gardenerInterval = 43200;
unsigned int clearDataCacheInterval = 300;
unsigned int dataCacheTimeFrame = 330;
unsigned int kafka_start_id = 0;
unsigned int total_threads_run = 1;
unsigned int volatile thread_counter = 0;
unsigned char output_type = 0;
time_t tLastUpdate, tnextUpdate;
time_t tnextGardener;
time_t tnextClearDataCache;
time_t tPluginFile;
struct sockaddr_in address;
int server_fd = -1;
int api_action = 0;
char* api_args = NULL;
int args_set = 0;
FILE *fptr = NULL;
char constants[MAX_CONSTANTS][50];
int values[50];
unsigned short *threadIds = NULL;
int logmessage_id[5];
int logrecord = 0;
int shutdown_phase = 0;
volatile sig_atomic_t is_stopping = 0;
volatile sig_atomic_t shutdown_reason = SR_NORMAL;
static volatile sig_atomic_t already_exiting = 0;
//pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
static pid_t plugin_pid_set[MAX_PLUGINS];
static pthread_mutex_t plugin_set_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t file_opened = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t update_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t hostname_mutex = PTHREAD_MUTEX_INITIALIZER;

void flushLog();
int isConstantsEnabled();
int getConstants();
void initNewPlugin(int index);
void initScheduler(int, int, bool);
void runPluginCommand(int, char*);
//void runPlugin(int, int);
void runPluginArgs(int, int, int);
void executeGardener();
int initTimeScheduler(bool);
void sig_handler(int);
void process_almond_api(ConfVal);
void process_almond_port(ConfVal);
void process_almond_standalone(ConfVal);
void process_iam_issuer(ConfVal);
void process_iam_public_key_file(ConfVal);
void process_iam_aud(ConfVal);
void process_json_file(ConfVal);
void process_metrics_file(ConfVal);
void process_metrics_output_prefix(ConfVal);
void process_save_on_exit(ConfVal);
void process_plugin_declaration(ConfVal);
void process_plugin_directory(ConfVal);
void process_almond_certificate( ConfVal);
void process_clear_data_cache_interval(ConfVal);
void process_conf_dir(ConfVal);
void process_data_cache_time_frame(ConfVal);
void process_enable_clear_data_cache( ConfVal);
void process_enable_gardener(ConfVal);
void process_enable_kafka_export(ConfVal);
void process_enable_kafka_id(ConfVal);
void process_enable_kafka_ssl(ConfVal);
void process_enable_kafka_tags(ConfVal);
void process_almond_format(ConfVal);
void process_gardener_run_interval(ConfVal);
void process_gardener_script(ConfVal);
void process_host_name(ConfVal);
void process_init_sleep(ConfVal);
void process_kafka_brokers(ConfVal);
void process_kafka_ca_certificate(ConfVal);
void process_kafka_config_file(ConfVal);
void process_kafka_producer_certificate(ConfVal);
void process_kafka_start_id(ConfVal);
void process_kafka_tag(ConfVal);
void process_kafka_topic(ConfVal);
void process_almond_key(ConfVal);
void process_data_dir(ConfVal);
void process_log_dir(ConfVal);
void process_log_plugin_output(ConfVal);
void process_log_to_stdout(ConfVal);
void process_almond_quickstart(ConfVal);
void process_run_gardener_at_start(ConfVal);
void process_store_results(ConfVal);
void process_almond_sleep( ConfVal);
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
void writePluginResultToFile(int, int);
void writeToKafkaTopic(int, int);
void run_plugin(PluginItem *item);

ConfigEntry config_entries[] = {
    {"almond.api", process_almond_api},
    {"almond.certificate", process_almond_certificate},
    {"almond.enableIamAud", process_enable_iam_aud},
    {"almond.enforceIAMRoles", process_enable_iam_roles},
    {"almond.iamAud", process_iam_aud},
    {"almond.iamIssuer", process_iam_issuer},
    {"almond.iamRolesAccepted", process_iam_roles_accepted},
    {"almond.iamPublicKeyFile", process_iam_public_key_file},
    {"almond.key", process_almond_key},
    {"almond.port", process_almond_port},
    {"almond.pushInterval", process_push_interval},
    {"almond.pushPort", process_push_port},
    {"almond.pushUrl", process_push_url},
    {"almond.standalone", process_almond_standalone},
    {"almond.useMetricsPush", process_metrics_push},
    {"almond.usePush", process_almond_push},
    {"almond.useSSL", process_almond_api_tls},
    {"data.jsonFile", process_json_file},
    {"data.metricsFile", process_metrics_file},
    {"data.metricsOutputPrefix", process_metrics_output_prefix},
    {"data.saveOnExit", process_save_on_exit},
    {"plugins.declaration", process_plugin_declaration},
    {"plugins.directory", process_plugin_directory},
    {"scheduler.allowAllHosts", process_allow_all_hosts},
    {"scheduler.certificate", process_almond_certificate},
    {"scheduler.clearDataCacheInterval", process_clear_data_cache_interval},
    {"scheduler.confDir", process_conf_dir},
    {"scheduler.dataCacheTimeFrame", process_data_cache_time_frame},
    {"scheduler.dataDir", process_data_dir},
    {"scheduler.enableClearDataCache", process_enable_clear_data_cache},
    {"scheduler.enableGardener", process_enable_gardener},
    {"scheduler.enableKafkaExport", process_enable_kafka_export},
    {"scheduler.enableKafkaId", process_enable_kafka_id},
    {"scheduler.enableKafkaSSL", process_enable_kafka_ssl},
    {"scheduler.enableKafkaTag", process_enable_kafka_tags},
    {"scheduler.format", process_almond_format},
    {"scheduler.gardenerRunInterval", process_gardener_run_interval},
    {"scheduler.gardenerScript", process_gardener_script},
    {"scheduler.hostName", process_host_name},
    {"scheduler.initSleepMs", process_init_sleep},
    #ifdef USE_AVRO 
    {"scheduler.kafkaAvro", process_kafka_avro},
    #endif
    {"scheduler.kafkaBrokers", process_kafka_brokers},
    {"scheduler.kafkaCACertificate", process_kafka_ca_certificate},
    {"scheduler.kafkaConfigFile", process_kafka_config_file},
    {"scheduler.kafkaProducerCertificate", process_kafka_producer_certificate},
    {"scheduler.kafkaStartId", process_kafka_start_id},
    {"scheduler.kafkaTag", process_kafka_tag},
    {"scheduler.key", process_almond_key},
    {"scheduler.logDir", process_log_dir},
    {"scheduler.logPluginOutput", process_log_plugin_output},
    {"scheduler.logToStdout", process_log_to_stdout},
    {"scheduler.quickStart", process_almond_quickstart},
    {"scheduler.runGardenerAtStart", process_run_gardener_at_start},
    {"scheduler.schemaName", process_schema_name},
    {"scheduler.schemaRegistryUrl", process_schema_registry_url},
    {"scheduler.storeResults", process_store_results},
    {"scheduler.sleepMs", process_almond_sleep},
    {"scheduler.storeDir", process_store_dir},
    {"scheduler.truncateLog", process_truncate_log},
    {"scheduler.truncateLogInterval", process_truncate_log_interval},
    {"scheduler.tuneMaster", process_tune_master},
    {"scheduler.tuneCycle", process_tune_cycle},
    {"scheduler.tuneTimer", process_tune_timer},
    {"scheduler.type", process_almond_scheduler_type},
    {"scheduler.useExternal", process_external_scheduler},
    {"scheduler.useKafkaConfigFile", process_use_kafka_config},
    {"scheduler.useTLS", process_almond_api_tls}
};

struct resp_buf { char *data; size_t len; };

static size_t file_read_cb(void *ptr, size_t size, size_t nmemb, void *stream) {
    return fread(ptr, size, nmemb, (FILE*)stream);
}

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t real = size * nmemb;
    struct resp_buf *rb = userdata;
    char *tmp = realloc(rb->data, rb->len + real + 1);
    if (!tmp) return 0;
    rb->data = tmp;
    memcpy(rb->data + rb->len, ptr, real);
    rb->len += real;
    rb->data[rb->len] = '\0';
    return real;
}

char *trim(char *s) {
    char *ptr;
    if (!s)
        return NULL;   // NULL string
    if (!*s)
        return s;      // empty string
    for (ptr = s + strlen(s) - 1; (ptr >= s) && isspace(*ptr); --ptr);
    ptr[1] = '\0';
    return s;
}

void removeChar(char *str, char garbage) {
        char *src, *dest;
        for (src = dest = str; *src != '\0'; src++){
                *dest = *src;
                if (*dest != garbage) dest++;
        }
        *dest ='\0';
}

char *replaceWord(char *sentence, char *find, char *replace) {
	char *dest = malloc((size_t)strlen(sentence)-strlen(find)+strlen(replace)+1);
	if (dest != NULL)
		dest[0] = '\0';
	strcpy(dest,sentence);
	char buffer[1024] = { 0 };
	char *insert_point = &buffer[0];
	const char *tmp = dest;
	size_t needle_len = strlen(find);
    	size_t repl_len = strlen(replace);
	while (1) {
        	const char *p = strstr(tmp, find);

        	if (p == NULL) {
            		strcpy(insert_point, tmp);
            		break;
        	}
        	memcpy(insert_point, tmp, (size_t)(p - tmp));
        	insert_point += p - tmp;
		memcpy(insert_point, replace, repl_len);
        	insert_point += repl_len;
        	tmp = p + needle_len;
	}
    	strcpy(dest, buffer);
    	return dest;
}

char *load_file_to_string(const char *path) {
	FILE *f = fopen(path, "r");
    	if (!f) return NULL;

    	fseek(f, 0, SEEK_END);
    	long size = ftell(f);
    	rewind(f);

    	char *buf = malloc(size + 1);
    	if (!buf) { fclose(f); return NULL; }

    	fread(buf, 1, size, f);
    	buf[size] = '\0';

    	fclose(f);
    	return buf;
}

/*static int cmp_plugin_by_id(const void *a, const void *b) {
    const PluginItem *pa = *(const PluginItem * const *)a;
    const PluginItem *pb = *(const PluginItem * const *)b;
    return pa->id - pb->id;
}*/

static int contains_scheme(const char *s) {
        return (strstr(s, "http://") == s) || (strstr(s, "https://") == s);
}

static int has_port_in_host(const char *s) {
        // crude check: if there's a ':' after the host part but before any '/' then assume port present
         const char *p = strstr(s, "://");
        if (p) s = p + 3;
        const char *slash = strchr(s, '/');
        const char *colon = strchr(s, ':');
        return (colon && (!slash || colon < slash));
}

static int is_ipv6_literal_no_brackets(const char *s) {
        // IPv6 literal contains ':' and does not start with '[' and does not contain scheme
        return (strchr(s, ':') != NULL) && (s[0] != '[') && !contains_scheme(s);
}

char *extract_authorization_header(const char *request) {
	const char *p = strstr(request, "Authorization:");
    	if (!p) return NULL;

    	p += strlen("Authorization:");
    	while (*p == ' ') p++; // skip spaces

    	const char *end = strstr(p, "\r\n");
    	if (!end) return NULL;

    	size_t len = end - p;
    	char *header = malloc(len + 1);
    	if (!header) return NULL;

    	strncpy(header, p, len);
    	header[len] = '\0';
    	return header;
}

char *extract_bearer_token(const char *auth_header) {
    	if (!auth_header) return NULL;

    	const char *prefix = "Bearer ";
    	if (strncmp(auth_header, prefix, strlen(prefix)) != 0)
        	return NULL;

    	return strdup(auth_header + strlen(prefix));
}

/*int validate_jwt(const char *token,
                 const char *pubkey_pem,
                 char *out_username,
                 size_t username_len,
                 char *out_fullname,
                 size_t fullname_len)
{
    jwt_t *jwt = NULL;

    if (jwt_decode(&jwt, token,
                   (unsigned char*)pubkey_pem,
                   strlen(pubkey_pem)) != 0) {
        writeLog("JWT decode failed", 1, 0);
        return 0;
    }

    time_t exp = jwt_get_grant_int(jwt, "exp");
    if (exp < time(NULL)) {
        writeLog("JWT expired", 1, 0);
        jwt_free(jwt);
        return 0;
    }

    const char *iss = jwt_get_grant(jwt, "iss");
    if (iam_issuer != NULL) {
        if (!iss || strcmp(iss, iam_issuer) != 0) {
            writeLog("Invalid JWT issuer", 1, 0);
            jwt_free(jwt);
            return 0;
        }
    }

    time_t nbf = jwt_get_grant_int(jwt, "nbf");
    if (nbf && time(NULL) < nbf) {
    	writeLog("JWT not valid yet", 1, 0);
    	jwt_free(jwt);
    	return 0;
    }

    time_t iat = jwt_get_grant_int(jwt, "iat");
    if (iat && iat > time(NULL) + 600) {  // allow 10 min skew
    	writeLog("JWT iat is in the future", 1, 0);
    	jwt_free(jwt);
   	return 0;
    }

    const char *aud = jwt_get_grant(jwt, "aud");
    if (expected_audience != NULL) {
    	if (!aud || strcmp(aud, expected_audience) != 0) {
        	writeLog("Invalid JWT audience", 1, 0);
        	jwt_free(jwt);
        	return 0;
    	}
    }

    // Extract identity fields
    const char *username = jwt_get_grant(jwt, "preferred_username");
    const char *fullname = jwt_get_grant(jwt, "name");

    if (username)
        snprintf(out_username, username_len, "%s", username);
    else
        snprintf(out_username, username_len, "unknown");

    if (fullname)
        snprintf(out_fullname, fullname_len, "%s", fullname);
    else
        snprintf(out_fullname, fullname_len, "%s", out_username);

    jwt_free(jwt);
    return 1;
}*/

/*int validate_jwt(const char *token,
                 const char *pubkey_pem,
                 char *out_username,
                 size_t username_len,
                 char *out_fullname,
                 size_t fullname_len)
{
    jwt_t *jwt = NULL;
    const time_t now = time(NULL);
    const int skew = 300; // 5 minutes clock skew

    if (jwt_decode(&jwt, token,
                   (unsigned char*)pubkey_pem,
                   strlen(pubkey_pem)) != 0) {
        writeLog("JWT decode failed", 1, 0);
        return 0;
    }

    //
    // --- exp (required) ---
    //
    time_t exp = jwt_get_grant_int(jwt, "exp");
    if (!exp) {
        writeLog("JWT missing exp", 1, 0);
        jwt_free(jwt);
        return 0;
    }
    if (exp < now - skew) {
        writeLog("JWT expired", 1, 0);
        jwt_free(jwt);
        return 0;
    }

    //
    // --- nbf (optional) ---
    //
    time_t nbf = jwt_get_grant_int(jwt, "nbf");
    if (nbf && now + skew < nbf) {
        writeLog("JWT not valid yet", 1, 0);
        jwt_free(jwt);
        return 0;
    }

    //
    // --- iat (optional sanity check) ---
    //
    time_t iat = jwt_get_grant_int(jwt, "iat");
    if (iat && iat > now + skew) {
        writeLog("JWT iat is in the future", 1, 0);
        jwt_free(jwt);
        return 0;
    }

    //
    // --- iss (optional) ---
    //
    const char *iss = jwt_get_grant(jwt, "iss");
    if (iam_issuer != NULL) {
        if (!iss || strcmp(iss, iam_issuer) != 0) {
            writeLog("Invalid JWT issuer", 1, 0);
            jwt_free(jwt);
            return 0;
        }
    }

    //
    // --- aud (string or array) ---
    //
    // if (enableIamAud && iam_aud != NULL) {
    if (enableIamAud && iam_aud != NULL && strcmp(iam_aud, "") != 0 && strcasecmp(iam_aud, "none") != 0) {
     	const char *aud = jwt_get_grant(jwt, "aud");

        int aud_ok = 0;

        if (aud) {
            // Simple string case
            if (strcmp(aud, iam_aud) == 0)
                aud_ok = 1;
        } else {
            // Try array case
            json_t *aud_json = jwt_get_grant_json(jwt, "aud");
	    if (!aud_json) {
		aud_json = jwt_get_grants_json(jwt, "aud")
            }
            if (aud_json && json_is_array(aud_json)) {
                size_t i;
                json_t *elem;
                json_array_foreach(aud_json, i, elem) {
                    if (json_is_string(elem) &&
                        strcmp(json_string_value(elem), iam_aud) == 0) {
                        aud_ok = 1;
                        break;
                    }
                }
            }
        }

        if (!aud_ok) {
            writeLog("Invalid JWT audience", 1, 0);
            jwt_free(jwt);
            return 0;
        }
    }

    //
    // --- Extract identity fields ---
    //
    const char *username = jwt_get_grant(jwt, "preferred_username");
    const char *fullname = jwt_get_grant(jwt, "name");

    if (username)
        snprintf(out_username, username_len, "%s", username);
    else
        snprintf(out_username, username_len, "unknown");

    if (fullname)
        snprintf(out_fullname, fullname_len, "%s", fullname);
    else
        snprintf(out_fullname, fullname_len, "%s", out_username);

    jwt_free(jwt);
    return 1;
}*/

void build_push_url(char *out, size_t outlen, const char *push_url, int port, const char *path) {
        const char *p = push_url ? push_url : "";
        const char *final_path = path ? path : "";

        if (contains_scheme(p)) {
                // push_url already has scheme
                if (has_port_in_host(p)) {
                        // already has port, just append path if provided
                        snprintf(out, outlen, "%s%s", p, final_path);
                } else {
                        // add port after host portion
                        // find end of host (first '/' after scheme)
                        const char *host_end = strchr(p + (strstr(p, "://") - p) + 3, '/');
                        if (!host_end) {
                                snprintf(out, outlen, "%s:%d%s", p, port, final_path);
                        } else {
                                // insert :port before host_end
                                size_t prefix_len = host_end - p;
                                if (prefix_len + 32 + strlen(final_path) + 1 > outlen) {
                                        out[0] = '\0';
                                        return;
                                }
                                strncpy(out, p, prefix_len);
                                out[prefix_len] = '\0';
                                snprintf(out + prefix_len, outlen - prefix_len, ":%d%s", port, final_path);
                        }
                }
        }
        else {
                // no scheme: decide if IPv6 literal
                if (is_ipv6_literal_no_brackets(p)) {
                        // wrap in brackets
                        snprintf(out, outlen, "http://[%s]:%d%s", p, port, final_path);
                } else {
                        // hostname or IPv4
                        snprintf(out, outlen, "http://%s:%d%s", p, port, final_path);
                }
        }
}

int load_allowed_hosts(const char *filename) {
        FILE *fp = fopen(filename, "r");
        if (!fp) {
                perror("Failed to open allow_hosts file");
                return -1;
        }

        char line[256];
        while (fgets(line, sizeof(line), fp)) {
                // Trim newline
                line[strcspn(line, "\r\n")] = 0;
                if (strlen(line) == 0) continue; // skip empty lines

                if (hosts_allowed_count < MAX_HOSTS) {
                        hosts_allowed[hosts_allowed_count] = strdup(line);
                        hosts_allowed_count++;
                }
        }
        fclose(fp);
        return 0;
}

int is_host_allowed(const char *client_ip) {
        for (int i = 0; i < hosts_allowed_count; i++) {
                if (strcmp(client_ip, hosts_allowed[i]) == 0) {
                        return 1; // exact match
                }
                if (strstr(hosts_allowed[i], "/24")) {
                        char prefix[INET_ADDRSTRLEN];
                        size_t len = strlen(hosts_allowed[i]);
                        if (len > 3) {
                                memcpy(prefix, hosts_allowed[i], len -3);
                                prefix[len -3] = '\0';
                        }
                        if (strncmp(client_ip, prefix, strlen(prefix)) == 0) {
                                return 1;
                        }
                }
        }
        return 0;
}

void add_plugin_pid(pid_t pid) {
    	pthread_mutex_lock(&plugin_set_mtx);
    	for (int i = 0; i < MAX_PLUGINS; i++) {
        	if (plugin_pid_set[i] == 0) { plugin_pid_set[i] = pid; break; }
    	}
    	pthread_mutex_unlock(&plugin_set_mtx);
}

void remove_plugin_pid(pid_t pid) {
    	pthread_mutex_lock(&plugin_set_mtx);
    	for (int i = 0; i < MAX_PLUGINS; i++) {
        	if (plugin_pid_set[i] == pid) { plugin_pid_set[i] = 0; break; }
    	}
    	pthread_mutex_unlock(&plugin_set_mtx);
}

int is_plugin_pid(pid_t pid) {
    	int found = 0;
    	pthread_mutex_lock(&plugin_set_mtx);
    	for (int i = 0; i < MAX_PLUGINS; i++) {
        	if (plugin_pid_set[i] == pid) { found = 1; break; }
    	}
    	pthread_mutex_unlock(&plugin_set_mtx);
    	return found;
}

TrackedPopen tracked_popen(const char *cmd) {
	int pfd[2];
    	if (pipe(pfd) == -1) { perror("pipe"); return (TrackedPopen){NULL, -1}; }

    	pid_t pid = fork();
    	if (pid < 0) {
        	perror("fork");
        	close(pfd[0]); close(pfd[1]);
        	return (TrackedPopen){NULL, -1};
    	}
    	if (pid == 0) {
        	// child
        	close(pfd[0]);
        	dup2(pfd[1], STDOUT_FILENO);
        	close(pfd[1]);
        	execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
        	_exit(127);
    	}

    	close(pfd[1]);
    	FILE *fp = fdopen(pfd[0], "r");
    	return (TrackedPopen){fp, pid};
}

int tracked_pclose(TrackedPopen *tp) {
    	/*int status = -1, rc = -1;
    	if (tp->fp) {
        	fclose(tp->fp);
        	rc = waitpid(tp->pid, &status, 0);
        	if (rc == tp->pid) {
            		if (WIFEXITED(status)) return WEXITSTATUS(status);
            		else return -1;
        	}
    	}
    	return -1;*/
	if (!tp || !tp->fp) return -1;
	fclose(tp->fp);
	int status, rc;
	do {
        	rc = waitpid(tp->pid, &status, 0);
    	} while (rc == -1 && errno == EINTR);
	if (rc == -1) {
		return -1;
	}
	if (WIFEXITED(status))       
		return WEXITSTATUS(status);
    	else if (WIFSIGNALED(status)) 
		return 128 + WTERMSIG(status);
    	else         
                return -1;
}

int getNextMessage() {
	int count = 0;
	for (int i = 0; i < 5; ++i) {
		if (logmessage_id[i] == 0) {
			logmessage_id[i] = 1;
			//printf("DEBUG count is %d\n", count);
			return count;
		}
		count++;
	}
	// Buffer full clear it
	/*for (int j = 0; j < 5; j++) {
		logmessage_id[j] = 0;
		//memset(logmessages[j], 0, logmessage_size * sizeof(char));
		if (logmessages[j] != NULL && logmessage_size > 0) {
			 printf("DEBUG: logmessage[%d] = %s\n", j, logmessages[j]);
			 printf("Now clear with memset...\n"); 
   			 memset(logmessages[j], 0, logmessage_size * sizeof(char));
		} else {
    			// Handle the error, e.g., log an error message or exit the program
    			fprintf(stderr, "Error: logmessages[j] is NULL or logmessage_size is invalid.\n");
    			exit(EXIT_FAILURE);
		}
	}*/
	return 0;
}

void logError(const char* message, int severity, int mode) {
        writeLog(message, severity, mode);
        fprintf(stderr, "%s\n", message);
}

void logInfo(const char*message, int severity,int mode) {
        writeLog(message, severity, mode);
        printf("%s\n", message);
}

void checkCtMemoryAlloc() {
	if (confDir == NULL) {
                fprintf(stderr, "Failed to allocate memory.\n");
        }
	if (dataDir == NULL) {
                fprintf(stderr, "Failed to allocate memory [dataDir].\n");
        }
	if (pluginDir == NULL) {
                fprintf(stderr, "Failed to allocate memory [pluginDir].\n");
        }
	if (pluginDeclarationFile == NULL) {
                fprintf(stderr, "Failed to allocate memory [pluginDeclarationFile].\n");
        }
	if (storeDir == NULL) {
                fprintf(stderr, "Failed to allocate memory [storeDir].\n");
        }
        if (infostr == NULL) {
                fprintf(stderr, "Failed to allocate memory [infostr].\n");
        }
        if (logDir == NULL) {
                fprintf(stderr, "Failed to allocate memory [logDir].\n");
        }
        if (fileName == NULL) {
                fprintf(stderr, "Failed to allocate memory [fileName].\n");
        }
	if (logfile == NULL ) {
		fprintf(stderr, "Failed to allocate memory [logfile].\n");
	}
        if (dataFileName == NULL ) {
                fprintf(stderr, "Failed to allocate memory [dataFileName].\n");
        }
        if (backupDirectory == NULL ) {
                fprintf(stderr, "Failed to allocate memory [backupDirectory].\n");
        }
        if (newFileName == NULL ) {
                fprintf(stderr, "Failed to allocate memory [newFileName].\n");
        }
	if (gardenerRetString == NULL) {
		fprintf(stderr, "Failed to allocate memory [gardenerRetString].\n");
	}
	if (pluginCommand == NULL) {
		fprintf(stderr, "Failed to allocate memory [pluginCommand].\n");
	}
	if (pluginReturnString == NULL) {
		fprintf(stderr, "Failed to allocate memory [pluginReturnString].\n");
	}
	if (storeName == NULL) {
		fprintf(stderr, "Failed to allocare memory [storeName].\n");
	}
	/*if (apiMessage == NULL) {
		fprintf(stderr, "Failed to allocate memory [apiMessage].\n");
	}*/
}

void updateHostName(char * str) {
	for (int i = 0; i < 255; i++) {
                hostName[i] = str[i];
                if (str[i] == '\0')
                        break;
        }
}

int parse__conf_line(char *buf) {
        int i;
        int x;
        int y;
        int s_count = 0;
        int p_count = 0;
        for (i = 0; i < 1000; i++) {
                if (buf[i] == '\n')
                        break;
                if (buf[i] == ';')
                        s_count++;
                if (buf[i] == '[' || buf[i] == ']')
                        p_count++;
        }
        i = 0;
		    char *saveptr = NULL;
		    char *p = strtok_r(buf, ";", &saveptr);
		    char *array[4];
		    i = 0;
		    while (p != NULL && i < 4) {
			    array[i++] = p;
			    p = strtok_r(NULL, ";", &saveptr);
		    }
	if (i < 4) {
		printf("Not enough tokens...\n");
		writeLog("[parse__conf_line] Not enough tokens. Faulty config file.", 1, 0);
		return 2;
	}
        sscanf(array[2], "%d", &x);
        if (x == 0) {
                if (strcmp(array[2], "0") != 0)
                        x = -1;
        }
        if (x == 0 || x == 1) {
                 y = atoi(array[3]);
                 if (!(y > 0)) {
                         return 2;
                 }
        }
        else
                return 2;
        if (s_count == 3 && p_count == 2)
                return 0;
        else
                return 2;
}

static char* getCurrentTimestamp() {
	static char timestamp[20];
	time_t now = time(NULL);
	/* Use proper format specifier for year */
	strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", localtime(&now));
	return timestamp;
}

static int compress_log(const char* src_filename, const char* dest_filename) {
	gzFile dest = NULL;
	FILE* source = NULL;
	char buffer[8192];
	size_t bytes_read;

	source = fopen(src_filename, "rb");
	if (!source) {
		fprintf(stderr, "Error opening %s: %s\n", src_filename, strerror(errno));
		writeLog("Failed to open the log source file for compression.", 1, 1);
		return -1;
	}
	dest = gzopen(dest_filename, "wb");
	if (!dest) {
		fprintf(stderr, "Error opening %s: %s\n", dest_filename, strerror(errno));
		writeLog("Failed to create the compressed log file.", 1, 1);
		if (source) fclose(source);
		return -1;
	}
	while ((bytes_read = fread(buffer, 1, sizeof(buffer), source)) > 0) {
		if (ferror(source)) {
            		perror("fread");
            		fclose(source);
            		gzclose(dest);
            		return -1;
        	}
		if (gzwrite(dest, buffer, bytes_read) != bytes_read) {
			fprintf(stderr, "Compression failed: %s\n", strerror(errno));
			writeLog("Compression of log file failed.", 1, 1);
			fclose(source);
			gzclose(dest);
			return -1;
		}
	}
	fclose(source);
	gzclose(dest);
	return 0;
}

void run_plugin(PluginItem *item) {
	if (!item) return;

    	/* 1) Save old return code and start timers */
    	int    prevRet = item->output.retCode;
    	clock_t start  = clock();
    	time_t  now    = time(NULL);

    	/* 2) Build full plugin command */
    	char cmd[plugincommand_size];
   	snprintf(cmd, plugincommand_size, "%s/%s", pluginDir, item->command);
    	//printf("Running: %s\n", cmd);
	snprintf(infostr, infostr_size, "Running command '%s'.", cmd);
	writeLog(trim(infostr), 0, 0);

    	/* 3) Spawn process and capture last non-empty line */
    	TrackedPopen tp = tracked_popen(cmd);
    	if (!tp.fp) {
        	perror("tracked_popen");
        	item->output.retCode = -1;
    	}
    	else {
        	add_plugin_pid(tp.pid);

        	char *last_line = NULL;
        	char  buf[pluginoutput_size];

        	while (fgets(buf, sizeof buf, tp.fp)) {
            		char *t = trim(buf);
            		if (*t) {
                		free(last_line);
                		last_line = strdup(t);
            		}
        	}
        	int rc = tracked_pclose(&tp);
        	remove_plugin_pid(tp.pid);

        	/* 4) Map shell exit codes to our retCode */
        	if (rc == 126)           
			item->output.retCode = 0;
        	else if (rc == 256)           
			item->output.retCode = 1;
        	else if (rc == 512)           
			item->output.retCode = 2;
        	else                          
			item->output.retCode = rc;

        	/* 5) Safely replace retString, capped at pluginoutput_size */
        	free(item->output.retString);
        	item->output.retString = NULL;

        	if (last_line) {
            		size_t len = strlen(last_line);
            		if (len >= (size_t)pluginoutput_size) {
                		len = pluginoutput_size - 1;
            		}
            		item->output.retString = malloc(len + 1);
            		if (item->output.retString) {
                		memcpy(item->output.retString, last_line, len);
                		item->output.retString[len] = '\0';
            		}
            		free(last_line);
        	}
    	}
    	/* 6) Format current timestamp */
    	char ts_now[TIMESTAMP_SIZE];
    	struct tm tm_now;
    	localtime_r(&now, &tm_now);
    	strftime(ts_now, sizeof ts_now, "%Y-%m-%d %H:%M:%S", &tm_now);

    	/* 7) Update statusChanged and lastChangeTimestamp */
    	if (prevRet != item->output.retCode) {
        	/* statusChanged is a char[2] array */
        	memcpy(item->statusChanged, "1", 2);
        	strncpy(item->lastChangeTimestamp,
                ts_now,
                sizeof item->lastChangeTimestamp - 1);
        	item->lastChangeTimestamp[sizeof item->lastChangeTimestamp - 1] = '\0';
    	}
	else {
        	memcpy(item->statusChanged, "0", 2);
    	}

    	/* 8) Update lastRunTimestamp */
    	strncpy(item->lastRunTimestamp,
            ts_now,
            sizeof item->lastRunTimestamp - 1);
    	item->lastRunTimestamp[sizeof item->lastRunTimestamp - 1] = '\0';

    	/* 9) Compute and store nextRunTimestamp */
    	time_t next = now + (item->interval * 60);
    	struct tm tm_next;
    	localtime_r(&next, &tm_next);
    	strftime(item->nextRunTimestamp,
             sizeof item->nextRunTimestamp,
             "%Y-%m-%d %H:%M:%S",
             &tm_next);
    	item->nextRun = next;

    	/* 10) Save prevRetCode for next iteration */
    	item->output.prevRetCode = prevRet;

    	/* 11) Print elapsed time */
    	double ms = (double)(clock() - start) * 1000.0 / CLOCKS_PER_SEC;
    	/*printf("%s executed in %.0f ms (ret=%d)\n\n",
           item->name,
           ms,
           item->output.retCode);*/
	snprintf(infostr, infostr_size, "%s executed in %.0f ms (ret=%d)", item->name, ms, item->output.retCode);
	writeLog(trim(infostr), 0, 0);
     	//ct = clock() -ct;
        //snprintf(infostr, infostr_size, "%s executed. Execution took %.0f milliseconds.\n", g_plugins[storeIndex]->name, (double)ct);
        //writeLog(trim(infostr), 0, 0);
        if (logPluginOutput) {
                char* o_info;
                int o_info_size = pluginmessage_size + 195;
                o_info = malloc((size_t)o_info_size * sizeof(char));
                if (o_info == NULL) {
                        writeLog("Could not allocate memory for variable 'o_info'.", 2, 0);
                }
		else {
                	snprintf(o_info, (size_t)o_info_size, "%s : %s", item->name, item->output.retString);
                	writeLog(trim(o_info), 0, 0);
                	free(o_info);
               	 	o_info = NULL;
		}
        }
        if (pluginResultToFile) {
                writePluginResultToFile(item->id, 0);
        }
        if (enableKafkaExport) {
                writeToKafkaTopic(item->id, 0);
        }
}


void execute_all_plugins(void) {
    for (int i = 0; i < g_plugin_count; ++i) {
        PluginItem *item = g_plugins[i];
        if (item && item->active) {
            run_plugin(item);
        }
    }
}

int post_json_file_stream(const char *url, const char *filepath) {
	CURL *c = NULL;
   	struct curl_slist *hdrs = NULL;
	FILE *f = NULL;
    	struct stat st;
    	struct resp_buf rb = { .data = NULL, .len = 0 };
    	int rc = -1;

    	f = fopen(filepath, "rb");
    	if (!f) {
		snprintf(infostr, infostr_size, "[push_json] Can not open file '%s'.", filepath);
		writeLog(infostr, 1, 0); 
		fprintf(stderr, "ERROR: cannot open file '%s'\n", filepath); 
		return -1; 
	}
    	if (fstat(fileno(f), &st) != 0) { 
		snprintf(infostr, infostr_size, "[push_json] fstat failed for '%s'.", filepath);
                writeLog(infostr, 1, 0);     
		fprintf(stderr, "ERROR: fstat failed for '%s'\n", filepath); 
		fclose(f); 
		return -1; 
	}

    	curl_global_init(CURL_GLOBAL_DEFAULT);
    	c = curl_easy_init();
    	if (!c) { 
		fclose(f); 
		curl_global_cleanup(); 
		return -1; 
	}

    	hdrs = curl_slist_append(NULL, "Content-Type: application/json");
    	curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
    	curl_easy_setopt(c, CURLOPT_URL, url);
    	curl_easy_setopt(c, CURLOPT_POST, 1L);
    	curl_easy_setopt(c, CURLOPT_READFUNCTION, file_read_cb);
    	curl_easy_setopt(c, CURLOPT_READDATA, f);
    	curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)st.st_size);
    	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
    	curl_easy_setopt(c, CURLOPT_WRITEDATA, &rb);

    	/* DEBUG: verbose output to stderr */
    	curl_easy_setopt(c, CURLOPT_VERBOSE, 1L);
    	curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);

    	CURLcode res = curl_easy_perform(c);
    	if (res != CURLE_OK) {
		snprintf(infostr, infostr_size, "[push_json] curl_easy_perform failed: %s.", curl_easy_strerror(res));
                writeLog(infostr, 1, 0);     
        	fprintf(stderr, "curl_easy_perform failed: %s\n", curl_easy_strerror(res));
    	} else {
        	long http_code = 0;
        	curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http_code);
		snprintf(infostr, infostr_size, "[push_json] HTTP status: '%ld'.", http_code);
                writeLog(infostr, 0, 0);     
        	fprintf(stderr, "HTTP status: %ld\n", http_code);
        	if (rb.len) 
			fprintf(stderr, "Response body: %s\n", rb.data);
        	rc = (http_code >= 200 && http_code < 300) ? 0 : -1;
    	}

    	free(rb.data);
    	curl_slist_free_all(hdrs);
    	curl_easy_cleanup(c);
    	curl_global_cleanup();
    	fclose(f);
    	return rc;
}

int post_metrics_file_stream(const char *url, const char *filepath) {
        CURL *c = NULL;
        struct curl_slist *hdrs = NULL;
        FILE *f = NULL;
        struct stat st;
        struct resp_buf rb = { .data = NULL, .len = 0 };
        int rc = -1;

        f = fopen(filepath, "rb");
        if (!f) {
                snprintf(infostr, infostr_size, "[push_metrics] Can not open file '%s'.", filepath);
                writeLog(infostr, 1, 0);
                fprintf(stderr, "ERROR: cannot open file '%s'\n", filepath);
                return -1;
        }
        if (fstat(fileno(f), &st) != 0) {
                snprintf(infostr, infostr_size, "[push_metrics] fstat failed for '%s'.", filepath);
                writeLog(infostr, 1, 0);
                fprintf(stderr, "ERROR: fstat failed for '%s'\n", filepath);
                fclose(f);
                return -1;
        }

        curl_global_init(CURL_GLOBAL_DEFAULT);
        c = curl_easy_init();
        if (!c) {
                fclose(f);
                curl_global_cleanup();
                return -1;
        }
	hdrs = curl_slist_append(NULL, "Content-Type: text/plain; version=0.0.4; charset=utf-8");
	// If you want to use the newer OpenMetrics format
	// hdrs = curl_slist_append(NULL, "Content-Type: application/openmetrics-text; version=1.0.0; charset=utf-8");
        curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
        curl_easy_setopt(c, CURLOPT_URL, url);
        curl_easy_setopt(c, CURLOPT_POST, 1L);
        curl_easy_setopt(c, CURLOPT_READFUNCTION, file_read_cb);
        curl_easy_setopt(c, CURLOPT_READDATA, f);
        curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)st.st_size);
        curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(c, CURLOPT_WRITEDATA, &rb);

        /* DEBUG: verbose output to stderr */
        curl_easy_setopt(c, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);

        CURLcode res = curl_easy_perform(c);
        if (res != CURLE_OK) {
                snprintf(infostr, infostr_size, "[push_metrics] curl_easy_perform failed: %s.", curl_easy_strerror(res));
                writeLog(infostr, 1, 0);
                fprintf(stderr, "curl_easy_perform failed: %s\n", curl_easy_strerror(res));
        } else {
                long http_code = 0;
                curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http_code);
                snprintf(infostr, infostr_size, "[push_metrics] HTTP status: '%ld'.", http_code);
                writeLog(infostr, 1, 0);
                fprintf(stderr, "HTTP status: %ld\n", http_code);
                if (rb.len)
                        fprintf(stderr, "Response body: %s\n", rb.data);
                rc = (http_code >= 200 && http_code < 300) ? 0 : -1;
        }

        free(rb.data);
        curl_slist_free_all(hdrs);
        curl_easy_cleanup(c);
        curl_global_cleanup();
        fclose(f);
        return rc;
}

/*int post_json_file_stream(const char *url, const char *filepath) {
	CURL *c = NULL;
    	struct curl_slist *hdrs = NULL;
    	FILE *f = NULL;
    	struct stat st;
	int rc = -1;

	f = fopen(filepath, "rb");
    	if (!f) {
        	fprintf(stderr, "Failed to open file: %s\n", filepath);
        	return -1;
    	}
    	if (fstat(fileno(f), &st) != 0) {
        	fprintf(stderr, "Failed to stat file: %s\n", filepath);
        	fclose(f);
        	return -1;
   	}

    	curl_global_init(CURL_GLOBAL_DEFAULT);
    	c = curl_easy_init();
    	if (!c) {
        	fclose(f);
        	curl_global_cleanup();
        	return -1;
    	}

    	hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
    	curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
    	curl_easy_setopt(c, CURLOPT_URL, url);
    	curl_easy_setopt(c, CURLOPT_UPLOAD, 1L);
    	curl_easy_setopt(c, CURLOPT_READDATA, f);
    	curl_easy_setopt(c, CURLOPT_INFILESIZE_LARGE, (curl_off_t)st.st_size);
    	curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    	curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);

	CURLcode res = curl_easy_perform(c);
    	if (res != CURLE_OK) {
        	fprintf(stderr, "curl error: %s\n", curl_easy_strerror(res));
        	rc = -1;
    	} 
	else {
        	long http_code = 0;
        	curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http_code);
        	if (http_code >= 200 && http_code < 300) rc = 0;
        	else {
            		fprintf(stderr, "Server returned HTTP status %ld\n", http_code);
            		rc = -1;
        	}
    	}

    	curl_slist_free_all(hdrs);
    	curl_easy_cleanup(c);
    	curl_global_cleanup();
    	fclose(f);
    	return rc;
}*/

int toggleHostName(char *name) {
        FILE * fPtr = NULL;
        FILE * fTemp = NULL;
        char * filename = NULL;
        char * tempfile = NULL;

        char buffer[1000];
        char fhost[20] = "scheduler.hostName=";
        char newline[300];
        filename = "/etc/almond/almond.conf";
        tempfile = "/etc/almond/almond.temp";

        int i = 0, j = 0;
        while(fhost[i] != '\0') {
                newline[j] = fhost[i];
                i++;
                j++;
        }
        i = 0;
        while (name[i] != '\0') {
                newline[j] = name[i];
                i++;
                j++;
        }
        newline[j] = '\0';

        fPtr = fopen(filename, "r");
        fTemp = fopen(tempfile, "w");

        if (fPtr == NULL || fTemp == NULL) {
                writeLog("Could not update hostname value in configuration file. Read error.", 1, 0);
                exit(EXIT_SUCCESS);
        }

        int changed = 0;
        while ((fgets(buffer, 1000, fPtr)) != NULL){
                char *pch = strstr(buffer, fhost);
                if (pch) {
                        fputs(newline, fTemp);
                        fputs("\n", fTemp);
                        changed = 1;
                }
                else
                        fputs(buffer, fTemp);
        }
        if (changed == 0) {
                // append to file
                fclose(fTemp);
                fTemp = NULL;
                fTemp = fopen("/etc/almond/almond.temp", "a");
                fprintf(fTemp, "%s\n", newline);
        }
        fclose(fPtr);
        fclose(fTemp);
        fPtr = fTemp = NULL;
        remove(filename);
        rename(tempfile, filename);
        writeLog("Updated almond.conf file", 0, 0);
        return 0;
}

int toggleExportFileName(char *name, int mode) {
        FILE * fPtr = NULL;
        FILE * fTemp = NULL;
        char * filename = NULL;
        char * tempfile = NULL;

        char buffer[1000];
        char dFile[15] = "data.jsonFile=";
	char mFile[18] = "data.metricsFile=";
        char newline[300];
        filename = "/etc/almond/almond.conf";
        tempfile = "/etc/almond/almond.temp";

        int i = 0, j = 0;
	if (mode == 0) {
        	while(dFile[i] != '\0') {
                	newline[j] = dFile[i];
               		i++;
                	j++;
        	}
	}
	if (mode == 1) {
                while(mFile[i] != '\0') {
                        newline[j] = mFile[i];
                        i++;
                        j++;
                }
        }
        i = 0;
        while (name[i] != '\0') {
                newline[j] = name[i];
                i++;
                j++;
        }
        newline[j] = '\0';

        fPtr = fopen(filename, "r");
        fTemp = fopen(tempfile, "w");

        if (fPtr == NULL || fTemp == NULL) {
                writeLog("Could not update filename value in configuration file. Read error.", 1, 0);
                exit(EXIT_SUCCESS);
        }

        int changed = 0;
        while ((fgets(buffer, 1000, fPtr)) != NULL){
                char *pch = NULL;
	        if (mode == 0)
			pch = strstr(buffer, dFile);
		else if (mode == 1)
			pch = strstr(buffer, mFile);
                if (pch) {
                        fputs(newline, fTemp);
                        fputs("\n", fTemp);
                        changed = 1;
                }
                else
                        fputs(buffer, fTemp);
        }
        if (changed == 0) {
                // append to file
                fclose(fTemp);
                fTemp = NULL;
                fTemp = fopen("/etc/almond/almond.temp", "a");
                fprintf(fTemp, "%s\n", newline);
        }
        fclose(fPtr);
        fclose(fTemp);
        fPtr = fTemp = NULL;
        remove(filename);
        rename(tempfile, filename);
        writeLog("Updated almond.conf file", 0, 0);
        return 0;
}

void updateFileName(char value[100], int mode) {
	int count = 1;
        char oldName[50];

	if (mode == 0) {
        	for (int c = 0; c < sizeof(oldName); c++) {
        		oldName[c] = jsonFileName[c];
        		jsonFileName[c] = '\0';
        	}
	}
	else if (mode == 1) {
		for (int c = 0; c < sizeof(oldName); c++) {
                        oldName[c] = metricsFileName[c];
                        metricsFileName[c] = '\0';
                }
	}
        for (int i = 0; i < strlen(value); i++) {
        	if (value[i] == '\n')
                	break;
        	else {
			if (mode == 0)
               			jsonFileName[i] = value[i];
			if (mode == 1)
				metricsFileName[i] = value[i];
                }
                if (value[i] == '\0')
                	break;
               	count++;
                if (count == 45) {
			if (mode == 0)
                		writeLog("New jsonfile name possibly writing over buffer size and will be truncated.", 1, 0);
			if (mode == 1)
				writeLog("New metrics filename possible writing over buffer size and will be truncated.", 1, 0);
                       	break;
                }
        }
        if (count == 45) {
		if (mode == 0) {
        		strcat(jsonFileName, ".json");
               		jsonFileName[50] = '\0';
		}
		else if (mode == 1) {
			strcat(metricsFileName, ".metrics");
			metricsFileName[50] = '\0';
		}
        }
        else {
		if (mode == 0) {
        		char *ext = strrchr(jsonFileName, '.');
                	if (ext) {
                		if (*(ext+1) == '\0') {
                        		writeLog("New jsonfile name is ending with a dot.", 1, 0);
                        	}
                        	jsonFileName[strlen(value)+1] = '\0';
                	}
                	else {
                		strcat(jsonFileName, ".json");
                        	jsonFileName[strlen(value)+6] = '\0';
                	}
			snprintf(infostr, infostr_size, "Json export file name changed to '%s'.", jsonFileName);
		}
		else if (mode == 1) {
			char *ext = strrchr(metricsFileName, '.');
                        if (ext) {
                                if (*(ext+1) == '\0') {
                                        writeLog("New metrics filname is ending with a dot.", 1, 0);
                                }
                                metricsFileName[strlen(value)+1] = '\0';
                        }
                        else {
                                strcat(metricsFileName, ".metrics");
                                metricsFileName[strlen(value)+9] = '\0';
                        }
			snprintf(infostr, infostr_size, "Metrics filename changed to '%s'.", metricsFileName);
		}

	}
       	writeLog(infostr, 1, 0);
       	writeLog("If using howru api together with Almond, you need to restart Howru service.", 2, 0);
	if (mode == 0)
       		toggleExportFileName(jsonFileName, 0);
	else if (mode == 1)
		toggleExportFileName(metricsFileName, 1);
       	char * removeFileName = NULL;
        if (mode == 0)
		removeFileName = malloc(datafilename_size);
	else if (mode == 1)
		removeFileName = malloc(100);
	if (removeFileName == NULL) {
       		writeLog("Could not allocate memory for removing old file.", 1, 0);
        }
        else {
		if (mode == 0) {
        		memset(removeFileName, '\0', datafilename_size);
                	snprintf(removeFileName, datafilename_size, "%s%c%s", dataDir, '/', oldName);
			if (remove(removeFileName) == 0) {
                        	snprintf(infostr, infostr_size, "Json export file '%s' is removed.", oldName);
                        	writeLog(infostr, 1, 0);
                	}
                	else {
                        	snprintf(infostr, infostr_size, "Could not remove old export file '%s'.", oldName);
                        	writeLog(infostr, 1, 0);
                	}
		}
		else if (mode == 1) {
			memset(removeFileName, '\0',100);
                        snprintf(removeFileName, 100, "%s%c%s", storeDir, '/', oldName);
			if (remove(removeFileName) == 0) {
                                snprintf(infostr, infostr_size, "Metrics export file '%s' is removed.", oldName);
                                writeLog(infostr, 1, 0);
                        }
                        else {
                                snprintf(infostr, infostr_size, "Could not remove old metrics file '%s'.", oldName);
                                writeLog(infostr, 1, 0);
                        }
		}
        }
        if (removeFileName != NULL) {
        	free(removeFileName);
                removeFileName = NULL;
        }
}

int compare_timestamps(const void* a, const void* b) {
    const struct Scheduler* sa = (const struct Scheduler*)a;
    const struct Scheduler* sb = (const struct Scheduler*)b;

    if (sa->timestamp < sb->timestamp) return -1;
    if (sa->timestamp > sb->timestamp) return 1;

    // Tie-breaker: sort by ID ascending
    if (sa->id < sb->id) return -1;
    if (sa->id > sb->id) return 1;

    return 0;
}


int get_thread_count() {
    int count = 0;
    DIR *dir = opendir("/proc/self/task");
    if (dir) {
        while (readdir(dir)) count++;
        closedir(dir);
    }
    return count - 2; // subtract '.' and '..'
}

void print_io_stats() {
    FILE *fp = fopen("/proc/self/io", "r");
    if (!fp) return;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        printf("%s", line);  // e.g., "read_bytes: 1024"
    }
    fclose(fp);
}

int get_fd_count() {
    int count = 0;
    DIR *dir = opendir("/proc/self/fd");
    if (dir) {
        while (readdir(dir)) count++;
        closedir(dir);
    }
    return count - 2; // subtract '.' and '..'
}

int check_plugin_conf_file(char *declarationFile) {
        FILE * fPtr = NULL;
        int i;
        char buffer[1000];
        int retval = 0;

        fPtr = fopen(declarationFile, "r");
        if (fPtr == NULL)
        {
                writeLog("Error opening the plugin g_plugins file.", 2, 0);
		perror("Error while opening the file [check_plugin_conf_file].\n");
                exit(EXIT_FAILURE);
        }
        while ((fgets(buffer, 1000, fPtr)) != NULL){
                for(i = 0; i < 1000; i++) {
                        if (buffer[i] == '#')
                                break;
                        else {
                                if (parse__conf_line(buffer) > 0) {
                                        retval = 2;
                                }
                                break;
                        }
                }
        }
        fclose(fPtr);
        fPtr = NULL;
        return retval;
}

void checkSchedulerCount() {
	if (g_current_scheduler_cnt != decCount) {
		writeLog("Reinitate scheduler since number of plugins changed.", 0, 0);
		free(scheduler);
		scheduler = NULL;
		g_current_scheduler_cnt = decCount;
		initTimeScheduler(true);
	}
}

void rescheduleChecks() {
	size_t n = (size_t)decCount;
        writeLog("Schedule new exectution times.", 0, 0);
	checkSchedulerCount();
        qsort(scheduler, n, sizeof(struct Scheduler), compare_timestamps);
        flushLog();
}

int updateValuesFromUdfFile(char id[3]) {
	FILE *fp = NULL;
	char* token;
	char filename[30];
	size_t buffer_size = pluginoutput_size + 100;
	char buffer[buffer_size];
	char columns[2][pluginoutput_size];
	int columnCount = 0;
	int pId = -1;

	strcpy(filename, "/opt/almond/api_cmd/");
	strncat(filename, trim(id), 3);
	strncat(filename, ".udf", 5);

	fp = fopen(filename, "r");
	if (fp == NULL) {
		writeLog("Could not open update file in api_cmd directory.", 1, 0);
		return 2;
	}
	while (fgets(buffer, buffer_size, fp)) {
		char *saveptr = NULL;
		token = strtok_r(buffer, "\t", &saveptr);
		while (token != NULL) {
			strncpy(columns[columnCount], token, pluginoutput_size - 1);
			columns[columnCount][pluginoutput_size - 1] = '\0';
			columnCount++;
			token = strtok_r(NULL, "\t", &saveptr);
		}
		if (strcmp(columns[0], "item_id") == 0) {
			snprintf(infostr, infostr_size, "Updating pluginitem with id '%s' from update file.", trim(columns[1]));
			writeLog(infostr, 0, 0);
			pId = atoi(columns[1]);
		}
		if (pId != -1) {
			if (strcmp(columns[0], "item_lastruntimestamp") == 0) {
				//strncpy(g_plugins[pId].lastRunTimestamp, trim(columns[1]), 20);
				snprintf(g_plugins[pId]->lastRunTimestamp, 20, "%s", trim(columns[1])); 
			}
			else if (strcmp(columns[0], "item_lastchangetimestamp") == 0) {
				//strncpy(g_plugins[pId].lastChangeTimestamp, trim(columns[1]), 20);
				snprintf(g_plugins[pId]->lastChangeTimestamp, 20, "%s", trim(columns[1]));
			}
			else if (strcmp(columns[0], "item_nextruntimestamp") == 0) {
				//strncpy(g_plugins[pId].nextRunTimestamp, trim(columns[1]), 20);
				snprintf(g_plugins[pId]->nextRunTimestamp, 20, "%s", trim(columns[1]));
			}
			else if (strcmp(columns[0], "item_statuschanged") == 0) {
				//strncpy(g_plugins[pId].statusChanged, trim(columns[1]), 1);
				snprintf(g_plugins[pId]->statusChanged, 2, "%s", trim(columns[1]));
			}
			else if (strcmp(columns[0], "output_retcode") == 0) {
				//strcpy(outputs[pId].retCode, trim(columns[1]));
				g_plugins[pId]->output.retCode = atoi(trim(columns[1]));
			}
			else if (strcmp(columns[0], "output_retstring") == 0) {
				//strcpy(outputs[pId].retString, trim(columns[1]));
				snprintf(g_plugins[pId]->output.retString, pluginoutput_size, "%s", trim(columns[1])); 
			}
		}
		columnCount = 0;
	}
	fclose(fp);
	fp = NULL;
	remove(filename);
	if (pId >= 0) {
		struct tm tm_struct;
		time_t time_var;
		char *timestamp = g_plugins[pId]->nextRunTimestamp;
		if (strptime(timestamp,"%Y-%m-%d %H:%M:%S", &tm_struct)) {
			time_var = mktime(&tm_struct);
			if (time_var != -1) {
				g_plugins[pId]->nextRun = time_var;
				if (timeScheduler) {
					scheduler[pId].timestamp = time_var;
					rescheduleChecks();
				}
				writeLog("A nextRun timestamp was updated from udf-file.", 0, 0);
			}
			else {
				writeLog("Could not update next run time stamp from udf-file.", 1, 0);
			}
		}
		else {
			writeLog("Error parsing nextRunTimestamp to t_time object in udf-file.", 1, 0);
		}
	}
	return 0;
}

void parseExArgsCmd(char command[100]) {
	const char* sNum;
	char* cmdRun;
	int num;

	sNum = strtok(command, ";");
	cmdRun = strtok(NULL, ";");

	num = atoi(sNum);
	runPluginCommand(num, cmdRun);
}

void setApiCmdFile(char * name, char * value) {
        FILE * fp;
        char filename[100] = "/opt/almond/api_cmd/";
        char content[100];
	snprintf(filename, sizeof(filename), "/opt/almond/api_cmd/%s.cmd", name);
	int written = snprintf(content, sizeof(content), "%s\t%s", name, value);
    	if (written < 0 || written >= sizeof(content)) {
        	writeLog("Content too long or formatting error.", 2, 0);
        	return;
    	}
        fp = fopen(filename, "w");
	if (fp == NULL) {
		perror("Failed to open command file.");
		writeLog("Failed to open command file.", 2, 0);
		return;
	}
        /*strncpy(content, name, sizeof(content)-1);
        strcat(content, "\t");
        strcat(content, value);*/
        fprintf(fp, "%s\n",content);
        fclose(fp);
	fp = NULL;
	writeLog("Command file written from API call.", 0, 0);
}

int runApiCmds(char * cmd) {
        FILE * cmdfile;
        char* token;
        int columnCount = 0;
        char line[100];
        char columns[2][100];
	char filename[PATH_MAX];

	int written = snprintf(filename, sizeof(filename), "/opt/almond/api_cmd/%s", cmd);
	if (written < 0 || (size_t)written >= sizeof(filename)) {
		writeLog("Command filename too long", 1, 0);
		return 2;
	}
        cmdfile = fopen(filename, "r");
        if (cmdfile == NULL) {
                perror("Failed to open file");
		writeLog("Could not open command file.", 1, 0);
                return 1;
        }
	while (fgets(line, sizeof(line), cmdfile)) {
		char *saveptr = NULL;
		token = strtok_r(line, "\t", &saveptr);
		while (token != NULL) {
			strncpy(columns[columnCount], token, sizeof(columns[columnCount]) - 1);
			columns[columnCount][sizeof(columns[columnCount]) - 1] = '\0';
			columnCount++;
			token = strtok_r(NULL, "\t", &saveptr);
		}
		columnCount = 0;
	}
        fclose(cmdfile);
	cmdfile = NULL;
        if (strcmp(columns[0], "hostname") == 0) {
		writeLog("Hostname will be updated in memory and config by API call.", 0, 0);
                updateHostName(trim(columns[1]));
		toggleHostName(trim(columns[1]));
        }
	else if (strcmp(columns[0], "kafkatag") == 0) {
		if (kafka_tag == NULL) {
			kafka_tag = malloc((strlen(columns[1]) + 1) * sizeof(char));
                 	if (kafka_tag != NULL)
				memset(kafka_tag, 0, strlen(columns[1]) + 1);
		 	else {
				 writeLog("Failed to allocate memory for variable 'kafka_tag'.", 1, 0);
				 return 2;
		 	}
		}
		int i = 0;
		while (columns[1][i] != '\n' && columns[1][i] != '\0') {
        		kafka_tag[i] = columns[1][i];
        		i++;
    		}
    		kafka_tag[i] = '\0'; 
                snprintf(infostr, infostr_size, "Kafka tag is set to '%s'", kafka_tag);
		writeLog(infostr, 0, 0);
	}
	else if (strcmp(columns[0], "kafkatopic") == 0) {
		if (kafka_topic == NULL) {
			kafka_topic = malloc((strlen(columns[1]) + 1) * sizeof(char));
			if (kafka_topic != NULL)
				memset(kafka_topic, 0, strlen(columns[1]) + 1);
			else {
				writeLog("Failed to allocate memory for variable 'kafka_topic'.", 1, 0);
				return 2;
			}
		}
		int i = 0;
		while (columns[1][i] != '\n' && columns[1][i] != '\0') {
        		kafka_topic[i] = columns[1][i];
        		i++;
    		}
    		kafka_topic[i] = '\0'; 
		snprintf(infostr, infostr_size, "Kafka topic is set to '%s'.", kafka_topic);
		if (useKafkaConfigFile) {
			setKafkaTopic(kafka_topic);
		}
		writeLog(infostr, 0, 0);
	}
	else if (strcmp(columns[0], "jsonfilename") == 0) {
		updateFileName(columns[1], 0);
	}
	else if (strcmp(columns[0], "metricsfilename") == 0) {
		updateFileName(columns[1], 1);
        }
	else if (strcmp(columns[0], "execute") == 0) {
		int id = atoi(columns[1]);
		writeLog("Execute plugin from command file.", 0, 0);
		//runPlugin(id, 0);
                PluginItem *item = g_plugins[id];
        	if (item) {
            		run_plugin(item);
        	}
		else {
			printf("DEBUG: Failed to execute item id %d.\n", id);
		}
		if (timeScheduler) {
			rescheduleChecks();
		}
	}
	else if (strcmp(columns[0], "executeargs") == 0) {
		writeLog("Execute plugin with added arguments from command file.", 0, 0);
		parseExArgsCmd(columns[1]);
		if (timeScheduler) {
			rescheduleChecks();
		}
	}
	else if (strcmp(columns[0], "metricsprefix") == 0) {
                memset(metricsOutputPrefix, '\0', metricsoutputprefix_size);
		size_t len = strlen(columns[1]);
		for (int i = 0; i < (int)len; i++) {
			if (columns[1][i] == '\n')
				break;
			else
				metricsOutputPrefix[i] = columns[1][i];
			if (columns[1][i] == '\0')
				break;
		}
	        snprintf(infostr, infostr_size, "Metrics prefix is set to '%s'", metricsOutputPrefix);
		writeLog(infostr, 0, 0);

	}
	else if (strcmp(columns[0], "update") == 0) {
		writeLog("Ready to run updates from udf-file.", 0, 0);
		updateValuesFromUdfFile(columns[1]);
	}
	else if (strcmp(columns[0], "scheduler") == 0) {
		char* scheduler_type;
		scheduler_type = malloc((size_t)strlen(columns[1])+1);
		for (int i = 0; i < strlen(columns[1]); i++) {
                        if (columns[1][i] == '\n')
                                break;
                        else
                                scheduler_type[i] = columns[1][i];
                        if (columns[1][i] == '\0')
                                break;
                }
		if (strcmp(trim(scheduler_type), "external") == 0) {
			external_scheduler = true;
			writeLog("Almond scheduler type is set to external through command file.", 0, 0);
		}
		else {
			external_scheduler = false;
			writeLog("Almond scheduler type is set to internal after running command file.", 0, 0);
		}
		free(scheduler_type);
	}
	else if (strcmp(columns[0], "pushurl") == 0) {
		if (push_url == NULL) {
			push_url = malloc((strlen(columns[1]) + 1) * sizeof(char));
                        if (push_url != NULL)
                                memset(push_url, 0, strlen(columns[1]) + 1);
                        else {
                                writeLog("Failed to allocate memory for variable 'push_url'.", 1, 0);
                                return 2;
                        }
                }
		else {
			size_t olen = sizeof(push_url);
			memset(push_url, '\0', olen);
		}
		size_t len = strlen(columns[1]);
                for (int i = 0; i < (int)len; i++) {
                        if (columns[1][i] == '\n')
                                break;
                        else
                                push_url[i] = columns[1][i];
                        if (columns[1][i] == '\0')
                                break;
                }
                snprintf(infostr, infostr_size, "Push url is set to '%s'", push_url);
                writeLog(infostr, 0, 0);
	}
	if (remove(filename) == 0) {
        	writeLog("Command file was deleted.", 0, 0);
        }
        else {
                writeLog("Unable to delete command file. The command will run again!", 1, 0);
        }
        return 0;
}

int checkApiCmds() {
    DIR *d;
    struct dirent *entry;
    const char *dirPath = "/opt/almond/api_cmd";  // Define your directory path

    if (!(d = opendir(dirPath))) {
        perror("Failed to open directory");
        writeLog("Failed to open command file directory.", 1, 0);
        return 1;
    }

    while ((entry = readdir(d)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len < 4)
            continue;

        if (strcmp(entry->d_name + len - 4, ".cmd") != 0)
            continue;

        if (entry->d_type == DT_REG) {
            runApiCmds(entry->d_name);
        }
        else if (entry->d_type == DT_UNKNOWN) {
            char fullPath[PATH_MAX];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", dirPath, entry->d_name);
            struct stat st;
            if (stat(fullPath, &st) == 0 && S_ISREG(st.st_mode)) {
                runApiCmds(entry->d_name);
            }
        }
    }
    closedir(d);
    return 0;
}

void initConstants() {
	logmessage = calloc(logmessage_size+1, sizeof(char));
	if (logmessage == NULL) {
                fprintf(stderr, "Failed to allocate memory [logmessage].\n");
        }
        else {
                strncpy(logmessage, "", logmessage_size+1);
		logmessage[logmessage_size] = '\0';
	}
        logfile = malloc(logfile_size);
        if (logfile == NULL) {
                 fprintf(stderr, "Failed to allocate memory [logFile].\n");
        }
        else
                memset(logfile, '\0', logfile_size);
	if (isConstantsEnabled() > 0) {
		getConstants();
        }
	confDir = malloc(confdir_size);
	if (confDir != NULL) {
		memset(confDir, 0, confdir_size);
	}
	dataDir = malloc(datadir_size);
	if (dataDir != NULL)
		memset(dataDir, '\0', datadir_size);
	pluginDir = malloc(plugindir_size);
	if (pluginDir != NULL)
		memset(pluginDir, '\0', plugindir_size);
	pluginDeclarationFile = malloc(plugindeclarationfile_size);
	if (pluginDeclarationFile != NULL)
		memset(pluginDeclarationFile, '\0', plugindeclarationfile_size);
	jsonFileName = calloc(jsonfilename_size+1, sizeof(char));
	if (jsonFileName == NULL) {
                fprintf(stderr, "Failed to allocate memory [jsonFileName].\n");
        }
	else
		strncpy(jsonFileName, "monitor_data.json", 18);
	metricsFileName = calloc(metricsfilename_size+1, sizeof(char));
	if (metricsFileName == NULL) {
		fprintf(stderr, "Failed to allocate memory [metricsFileName].\n");
	}
	else
		strncpy(metricsFileName, "monitor.metrics", 16);
	gardenerScript = calloc(gardenerscript_size+1, sizeof(char));
	if (gardenerScript == NULL) {
                fprintf(stderr, "Failed to allocate memory [gardenerScript].\n");
        }
        else
		strncpy(gardenerScript, "/opt/almond/gardener.py", 24);
	storeDir = malloc(storedir_size);
	if (storeDir == NULL) {
		fprintf(stderr, "Failed to allocate memory [storeDir].\n");
	}
	else
		memset(storeDir, '\0', storedir_size);
	logDir = malloc(logdir_size);
	if (logDir != NULL)
		memset(logDir, '\0', logdir_size);
	infostr = malloc((size_t)infostr_size * sizeof(char));
	if (infostr == NULL) {
		fprintf(stderr, "Failed to allocate memory [infostr].\n");
	}
	else
		memset(infostr, '\0', (size_t)infostr_size * sizeof(char));
	hostName = calloc(hostname_size+1, sizeof(char));
	if (hostName == NULL) {
		fprintf(stderr, "Failed to allocate memory [hostName].\n");
	}
	else
		strncpy(hostName, "None", 5);
	fileName = malloc((size_t)filename_size * sizeof(char));
	if (fileName == NULL) {
		fprintf(stderr, "Failed to allocate memory [fileName].\n");
	}
	else
		memset(fileName, '\0', (size_t)filename_size * sizeof(char));
	metricsOutputPrefix = calloc(metricsoutputprefix_size+1, sizeof(char));
	if (metricsOutputPrefix == NULL) {
		fprintf(stderr, "Failed to allocate memory [metricsOutputPrefix].\n");
	}
	else
		strncpy(metricsOutputPrefix, "almond", 7);
	dataFileName = malloc(datafilename_size);
	memset(dataFileName, '\0', datafilename_size);
	backupDirectory = malloc(backupdirectory_size);
	memset(backupDirectory, '\0', backupdirectory_size);
	newFileName = malloc(newfilename_size);
	memset(newFileName, '\0', (size_t)(150 * sizeof(char)));
	gardenerRetString = malloc((size_t)gardenermessage_size * sizeof(char));
	memset(gardenerRetString, '\0', (size_t)(sizeof(char) * gardenermessage_size));
	pluginCommand = malloc((size_t)100 * sizeof(char));
	memset(pluginCommand, '\0', sizeof(char) * 100);
	pluginReturnString = malloc((size_t)pluginmessage_size * sizeof(char));
	memset(pluginReturnString, '\0', (size_t)(pluginmessage_size * sizeof(char)));
	storeName = malloc((size_t)storename_size * sizeof(char));
	memset(storeName, '\0', (size_t)(sizeof(char) * storename_size));
	//apiMessage = malloc(apimessage_size * sizeof(char));
	checkCtMemoryAlloc();
}

int getConstants() {
	int count = 0;

	if (logmessage == NULL) {
		logmessage = malloc(logmessage_size);
		if (logmessage == NULL) {
			printf("Could not allocate memory for logmessage!\n");
			return -1;
		}
		else {
			memset(logmessage, 0, logmessage_size);
                	logmessage[0] = '\0';
		}
	}

	writeLog("Reading memory variable constants.", 0, 1);

        FILE *file = fopen(constantsFile, "r");
        if (file == NULL) {
		printf("Could not read constants file. Not found.");
                return 1;
        }

        while (fscanf(file, "%s %d", constants[count], &values[count]) == 2) {
                count++;
		if (count == MAX_CONSTANTS) break;
        }
        for (int i = 0; i < count; i++) {
                if (strcmp(constants[i], "CONFDIR_SIZE") == 0) {
                        writeLog("Memory for variable 'confDir' will be allocated by constants file.", 0, 1);
			confdir_size = (size_t)(values[i] * sizeof(char)+1);
                }
		else if (strcmp(constants[i], "DATADIR_SIZE") == 0) {
			writeLog("Memory for variable 'dataDir' will be allocated by constants file.", 0, 1);
			datadir_size = (size_t)(values[i] * sizeof(char) + 1);
		}
		else if (strcmp(constants[i], "PLUGINDECLARATIONFILE_SIZE") == 0) {
			writeLog("Memory for variable 'pluginDeclarationSize' will be allocated by constants file.", 0, 1);
			plugindeclarationfile_size = (size_t)(values[i] * sizeof(char)+1);
		}
		else if (strcmp(constants[i], "JSONFILENAME_SIZE") == 0) {
			writeLog("Memory for variable 'jsonFileName' will be allocated by constants file.", 0, 1);
			jsonfilename_size = (size_t)(values[i] * sizeof(char)+1);
		}
		else if (strcmp(constants[i], "METRICSFILENAME_SIZE") == 0) {
                        writeLog("Memory for variable 'metricsFileName' will be allocated by constants file.", 0, 1);
			metricsfilename_size = (size_t)(values[i] * sizeof(char)+1);
                }
		else if (strcmp(constants[i], "GARDENERSCRIPT_SIZE") == 0) {
                        writeLog("Memory for variable 'gardenerScript' will be allocated by constants file.", 0, 1);
			gardenerscript_size = (size_t)(values[i] * sizeof(char)+1);
		} 
		else if (strcmp(constants[i], "HOSTNAME_SIZE") == 0) {
			writeLog("Memory for variable 'hostName' will be allocated by constants file.", 0, 1);
			hostname_size = (size_t)(values[i] * sizeof(char)+1);
		}
		else if (strcmp(constants[i], "METRICSOUTPUTPREFIX_SIZE") == 0) {
                        writeLog("Memory for variable 'metricsOutputPrefix' will be allocated by constants file.", 0, 1);
			metricsoutputprefix_size = (size_t)(values[i] * sizeof(char)+1);
                }
		else if (strcmp(constants[i], "STOREDIR_SIZE") == 0) {
                        writeLog("Memory for variable 'storeDir' will be allocated by constants file.", 0, 1);
			storedir_size = (size_t)(values[i] * sizeof(char)+1);
                }
		else if (strcmp(constants[i], "LOGDIR_SIZE") == 0) {
                        writeLog("Memory for variable 'logDir' will be allocated by constants file.", 0, 1);
			logdir_size = (size_t)(values[i] * sizeof(char)+1);
                }
		else if (strcmp(constants[i], "INFOSTR_SIZE") == 0) {
			writeLog("Memory for 'info_str' will be allocated by constants file.", 0, 1);
			infostr_size = (size_t)(values[i] * sizeof(char)+1);
		}
		else if (strcmp(constants[i], "PLUGINDIR_SIZE") == 0) {
                        writeLog("Memory for variable 'pluginDir' will be allocated by constants file.", 0, 1);
			plugindir_size =  (size_t)(values[i] * sizeof(char)+1);
                }
		else if (strcmp(constants[i], "FILENAME_SIZE") == 0) {
                        writeLog("Memory for variable 'fileName' will be allocated by constants file.", 0, 1);
			filename_size = (size_t)(values[i] * sizeof(char)+1);
                }
		else if (strcmp(constants[i], "LOGMESSAGE_SIZE") == 0) {
			writeLog("Memory for variable 'logmessage' will be allocated by constants file.", 0, 1);
			logmessage_size = (size_t)(values[i] * sizeof(char)+1);
		}
		else if (strcmp(constants[i], "LOGFILE_SIZE") == 0) {
			writeLog("Memory for variable 'logfile' will be allocated by constants file.", 0, 1);
			logfile_size = (size_t)(values[i] * sizeof(char)+1);
		}
		else if (strcmp(constants[i], "DATAFILENAME_SIZE") == 0) {
                        writeLog("Memory for variable 'dataFileName' will be allocated by constants file.", 0, 1);
			datafilename_size = (size_t)(values[i] * sizeof(char)+1);
                }
		else if (strcmp(constants[i], "BACKUPDIRECTORY_SIZE") == 0) {
                        writeLog("Memory for variable 'backupDirectory' will be allocated by constants file.", 0, 1);
			backupdirectory_size = (size_t)(values[i] * sizeof(char)+1);
                }
		else if (strcmp(constants[i], "NEWFILENAME_SIZE") == 0) {
                        writeLog("Memory for variable 'newFileName' will be allocated by constants file.", 0, 1);
			newfilename_size = (size_t)(values[i] * sizeof(char)+1);
                }
		else if (strcmp(constants[i], "GARDENERMESSAGE_SIZE") == 0) {
			writeLog("Memory for gardener return message will be allocated by constants file.", 0, 1);
			gardenermessage_size = (size_t)(values[i] * sizeof(char)+1);
		}
		else if (strcmp(constants[i], "PLUGINCOMMAND_SIZE") == 0) {
			writeLog("Memory for plugin command size will be allocated by constants file.", 0, 1);
			plugincommand_size = (size_t)(values[i] * sizeof(char)+1);
		}
		else if (strcmp(constants[i], "PLUGINMESSAGE_SIZE") == 0) {
                        writeLog("Memory for plugin message size will be allocated by constants file.", 0, 1);
			pluginmessage_size = (size_t)(values[i] * sizeof(char)+1);
                }
		else if (strcmp(constants[i], "STORENAME_SIZE") == 0) {
                        writeLog("Memory for variable 'storeName' will be allocated by constants file.", 0, 1);
                        storename_size = (size_t)(values[i] * sizeof(char)+1);
                }
		else if (strcmp(constants[i], "APIMESSAGE_SIZE") == 0) {
                        writeLog("Memory for API messages will dynamically be allocated by size inconstants file.", 0, 1);
                        apimessage_size = (size_t)(values[i] * sizeof(char)+1);
                }
		else if (strcmp(constants[i], "SOCKETSERVERMESSAGE_SIZE") == 0) {
			writeLog("Memory for socket server messages will dynamically be allocated by size in constants file.", 0, 1);
			socketservermessage_size = (size_t)(values[i] * sizeof(char)+1);
		}
		else if (strcmp(constants[i], "SOCKETCLIENTMESSAGE_SIZE") == 0) {
			writeLog("Memory for socket client messages will dynamically be allocated by size in constants file.", 0, 1);
			socketclientmessage_size = (size_t)(values[i] * sizeof(char)+1);
		}
		else if (strcmp(constants[i], "PLUGINITEMNAME_SIZE") == 0) {
			writeLog("Memory for pluginitem name size will dynamically be allocated by size in constants file.", 0, 1);
                        pluginitemname_size = (size_t)(values[i] * sizeof(char)+1);
                }
 		else if (strcmp(constants[i], "PLUGINITEMDESC_SIZE") == 0) {
                        writeLog("Memory for pluginitem description size will dynamically be allocated by size in constants file.", 0, 1);
                        pluginitemdesc_size = (size_t)(values[i] * sizeof(char)+1);
                }
 		else if (strcmp(constants[i], "PLUGINITEMCMD_SIZE") == 0) {
                        writeLog("Memory for pluginitem command size will dynamically be allocated by size in constants file.", 0, 1);
                        pluginitemcmd_size = (size_t)(values[i] * sizeof(char)+1);
                }
		else if (strcmp(constants[i], "PLUGINOUTPUT_SIZE") == 0) {
                        writeLog("Memory for plugin output will dynamically be allocated by size in constants file.", 0, 1);
                        pluginoutput_size = (size_t)(values[i] * sizeof(char)+1);
                }
		else {
			snprintf(infostr, infostr_size, "Constant '%s' not implemented by Almond %s", constants[i], VERSION);
			writeLog(trim(infostr), 1, 1);
		}
	}
        return 0;
}

void constructSocketMessage(const char* action, const char* message) {
	/*int size = strlen(action) + strlen(message);
	size += 11;*/
	int needed = snprintf(NULL, 0, "{ \"%s\":\"%s\" }\n", action, message);
	if (needed <  0) {
		perror("[constructSocketMessage] snprintf");
		writeLog("Could not compute size of socket message.", 2, 0);
		return;
	} 
	socket_message = malloc((size_t)needed + 1);
    	if (socket_message == NULL) {
        	printf("Memory allocation failed.\n");
		writeLog("Memory allocation failed [constructSocketMessage:socket_message]", 2, 0);
        	return;
    	}
	//else
	//	memset(socket_message, '\0', (size_t)size * sizeof(char));
    	int written = snprintf(socket_message, (size_t)needed + 1, "{ \"%s\":\"%s\" }\n", action, message);
	if (written != needed) {
		writeLog("[constructSocketMessage] snprintf mismatch. This should not really ever happen.", 2, 0);
		free(socket_message);
		socket_message = NULL;
	}
}

int directoryExists(const char *checkDir, size_t length) {
        snprintf(infostr, infostr_size, "Checking directory %s", checkDir);
        writeLog(trim(infostr), 0, 1);

        DIR* dir = opendir(checkDir);
        if (dir) {
                closedir(dir);
                return 0;
        }
        else if (ENOENT == errno) {
                return 1;
        }
        else { return 2; }
}

int getIdFromName(char *plugin_name) {
	char* pluginName = NULL;
	int retVal = -1;

	for (int i = 0; i < decCount; i++) {
		pluginName = malloc((size_t)pluginitemname_size * sizeof(char)+1);
		if (pluginName == NULL) {
			fprintf(stderr, "Failed to allocate memory.\n");
			writeLog("Failed to allocate memory [getIdFromName:pluginName]", 2, 0);
			return -1;
		}
		else
			memset(pluginName, '\0', (size_t)pluginitemname_size+1 * sizeof(char));
                strncpy(pluginName, g_plugins[i]->name, pluginitemname_size);
		pluginName[pluginitemname_size] = '\0';
		removeChar(pluginName, '[');
		removeChar(pluginName, ']');
		if (strcmp(trim(plugin_name), pluginName) == 0) {
			retVal = g_plugins[i]->id;
			break;
		}
		free(pluginName);
		pluginName = NULL;
	}
	if (pluginName != NULL) {
		free(pluginName);
		pluginName = NULL;
	}
	return retVal +1;
}

void* apiThread(void* data) {
	int retrys = 3;
	int retry_count = 0;
	int createSocketRetVal = 0;
        pthread_detach(pthread_self());
	createSocketRetVal = createSocket(server_fd);
        while ((createSocketRetVal != 0)  && (retry_count > retrys)) {
		perror("Create socket.");
		printf("Could not create socket!\n");
		writeLog("Could not create socket for API thread.", 1, 0);
		sleep(1);
		createSocketRetVal = createSocket(server_fd);
		retry_count++;
	}
	total_threads_run++;
	pthread_mutex_lock(&mtx);
	thread_counter--;
	pthread_mutex_unlock(&mtx);
        pthread_exit(NULL);
	total_threads_run++;
}

void startApiSocket() {
        pthread_t thread_id;
        int rc;

        rc = pthread_create(&thread_id, NULL, apiThread, "almondapi");
        if(rc != 0) {
		printf("Error creating phtread\n");
                snprintf(infostr, infostr_size, "Error: return code from phtread_create is %d\n", rc);
                writeLog(trim(infostr), 2, 0);
		return;
        }
	pthread_detach(thread_id);
	pthread_setspecific(thread_id, "API Connection Listener");
	printf("New thread accepting socket created.\n");
        snprintf(infostr, infostr_size, "Created new thread (%lu) listening for connections on port %d \n", thread_id, local_port);
        writeLog(trim(infostr), 0, 0);
	total_threads_run++;
	pthread_mutex_lock(&mtx);
	thread_counter++;
	pthread_mutex_unlock(&mtx);
}

void changeSetValue(int id, int newval) {
	if (id > 10) {
		if (newval > 0)
			newval = 1;
		else
			newval = 0;
	}
	switch (id) {
		case 1:
			logPluginOutput = (newval > 0);
			break;
		case 2:
			saveOnExit = (newval > 0);
			break;
		case 10:
                        if ((newval < 1000) || (newval > 60000)) {
				writeLog("API call is trying to set sleep to unsupported value.", 1, 0);
				writeLog("Scheduler sleep value is unchanged.", 0, 0);
			}
			else
				schedulerSleep = newval;
			break;
		case 11:
			kafka_start_id = newval;
			break;
		case 12:
			push_port = newval;
			break;
		case 13:
			if (newval < 15) {
				writeLog("API call is trying to set push interval to unsupported value.", 1, 0);
				writeLog("Push interval value is unchanged.", 0, 0);
			}
			else
				push_interval = newval;
			break;
		default:
			writeLog("changeSetValue called with wrong index", 1, 0);
	}
}

void setMaintenanceStatus(int id, char* value) {
	int maintenance_status_value = 1;
	if (strcmp(value, "true") == 0) {
		maintenance_status_value = 0;
	}
	if (g_plugins[id]->active != maintenance_status_value)
		g_plugins[id]->active = maintenance_status_value;
        snprintf(infostr, infostr_size, "Updating maintenance status to %d for plugin '%s'.", maintenance_status_value, g_plugins[id]->name);
	writeLog(infostr, 1, 0);
}

void setPluginOutput(int newval) {
	if (newval > 0)
	       	newval = 1 ;
	else newval = 0;
	logPluginOutput = (newval > 0);
}

int toggleQuickStart(int on) {
	FILE * fPtr = NULL;
	FILE * fTemp = NULL;
	char * filename = NULL;
	char * tempfile = NULL;

	char buffer[1000];
	char enable[30] = "scheduler.quickStart=1";
	char disable[30] = "scheduler.quickStart=0";
	filename = "/etc/almond/almond.conf";
	tempfile = "/etc/almond/almond.temp";

	fPtr = fopen(filename, "r");
	fTemp = fopen(tempfile, "w");

	if (fPtr == NULL || fTemp == NULL) {
		writeLog("Could not update quick start value in configuration file. Read error.", 1, 0);
		exit(EXIT_SUCCESS);
	}

	while ((fgets(buffer, 1000, fPtr)) != NULL){
                char *pch = strstr(buffer, "quickStart");
	       	if (pch) {
			if (on > 0)	
				fputs(enable, fTemp);
			else
				fputs(disable, fTemp);
			fputs("\n", fTemp); 
		}
		else
			fputs(buffer, fTemp);
	}
	fclose(fPtr);
	fPtr = NULL;
	fclose(fTemp);
	fTemp = NULL;
	remove(filename);
	rename(tempfile, filename);
	writeLog("Updated almond.conf file", 0, 0);
	return 0;
}

void send_socket_message(int socket, SSL* ssl,  int id, int aflags) {
        //char header[100] = "HTTP/1.1 200 OK\nContent-Type:application/txt\nContent-Length: ";
	const char *fmt = 
		"HTTP/1.1 200 OK\n"
  		"Content-Type:application/txt\n"
  		"Content-Length: %zu\n\n";
	char * send_message = NULL;
	size_t content_length = 0;
	size_t total = 0;
	//char lenbuf[21];

	if (args_set == 0) {
		switch (api_action) {
        		case API_READ:
				apiReadData(id, aflags);
                        	break;
			case API_MONITOR:
                        	apiMonitorItem(id, aflags);
                        	break;
			case API_RUN:
				apiRunPlugin(id, aflags);
				break;
                	case API_DRY_RUN:
				apiDryRun(id);	
                       	 	break;
                	case API_EXECUTE_AND_READ:
				apiRunAndRead(id, aflags);
                        	break;
			case API_GET_METRICS:
				apiGetMetrics();
				break;
			case API_READ_ALL:
				apiReadAll();
				break;
			case API_EXECUTE_GARDENER:
                                executeGardener();
				constructSocketMessage("execute", "Almond gardener script executed.");
                                break;
                        case API_ENABLE_TIMETUNER:
                                enableTimeTuner = true;
                                writeLog("Time tuner enabled through API call.", 0, 0);
				constructSocketMessage("enable", "Time tuner is now enabled.");
                                break;
                        case API_DISABLE_TIMETUNER:
                                enableTimeTuner = false;
                                writeLog("Time tuner disabled through API call.", 0, 0);
				constructSocketMessage("disable", "Time tuner is now disabled.");
                                break;
                        case API_ENABLE_GARDENER:
                                enableGardener = true;
                                writeLog("Gardener enabled through API call.", 0, 0);
				constructSocketMessage("enable", "Gardener is now enabled.");
                                break;
                        case API_DISABLE_GARDENER:
                                enableGardener = false;
                                writeLog("Gardener disabled through API call.", 0, 0);
				constructSocketMessage("disable", "Gardener is now disabled.");
                                break;
			case API_ENABLE_CLEARCACHE:
                                enableClearDataCache = true;
                                writeLog("ClearDataCache enabled through API call.", 0, 0);
				constructSocketMessage("enable", "ClearDataCache is now enabled.");
                                break;
                        case API_DISABLE_CLEARCACHE:
                                enableClearDataCache = false;
                                writeLog("ClearDataCache disabled through API call.", 0, 0);
				constructSocketMessage("disable", "ClearDataCache is now disabled.");
                                break;
                        case API_ENABLE_QUICKSTART:
                                quick_start = true;
				toggleQuickStart(1);
                                writeLog("Quick start enabled through API call.", 0, 0);
				constructSocketMessage("enable", "Quick start is now enabled.");
                                break;
                        case API_DISABLE_QUICKSTART:
                                quick_start = false;
				toggleQuickStart(0);
                                writeLog("Quick start disabled through API call.", 0, 0);
				constructSocketMessage("disable", "Quick start is now disabled");
                                break;
                        case API_ENABLE_STANDALONE:
                                standalone = true;
                                writeLog("Standalone mode enabled through API call.", 0, 0);
				constructSocketMessage("enable", "Standalone mode is now enabled");
                                break;
                        case API_DISABLE_STANDALONE:
                                standalone = false;
                                writeLog("Standalone mode disabled through API call.", 0, 0);
				constructSocketMessage("disable", "Standalone mode is now disabled.");
                                break;
			case API_ENABLE_PUSH:
                                use_push = true;
                                writeLog("Almond push enabled through API call.", 0, 0);
                                constructSocketMessage("enable", "Almond push is now enabled.");
                                break;
			case API_DISABLE_PUSH:
                                use_push = false;
                                writeLog("Almond push disabled through API call.", 0, 0);
                                constructSocketMessage("disable", "Almond push is now disabled.");
                                break;
                        case API_ENABLE_METRICS_PUSH:
                                use_metrics_push = true;
                                writeLog("Almond metrics push enabled through API call.", 0, 0);
                                constructSocketMessage("enable", "Almond metrics push is now enabled.");
                                break;
                        case API_DISABLE_METRICS_PUSH:
                                use_metrics_push = false;
                                writeLog("Almond metricd push disabled through API call.", 0, 0);
                                constructSocketMessage("disable", "Almond metrics push is now disabled.");
				break;
			case API_SET_PLUGINOUTPUT:
                                writeLog("Log plugin output toggled through API call.", 0, 0);
				constructSocketMessage("set", "Log plugin output toggled.");
                                break;
			case API_SET_SAVEONEXIT:
                                writeLog("Save on exit is toggled through API call.", 0, 0);
				constructSocketMessage("set", "Save on exit output toggled.");
                                break;
			case API_SET_SLEEP:
				writeLog("Scheduler sleep toggled through API call.", 1, 0);
				constructSocketMessage("set", "Scheduler sleep toggled");
                                break;
			case API_SET_KAFKATAG:
                                writeLog("Kafka tag toggled through API call.", 0, 0);
				constructSocketMessage("set", "Kafka tag toggled");
                                break;
			case API_SET_KAFKA_START_ID:
                                writeLog("Kafka start id toggled through API call.", 0, 0);
				constructSocketMessage("set","Kafka start id toggled.");
				break;
			case API_SET_HOSTNAME:
				writeLog("The virtual hostname of the unit has been changed through API call.", 1, 0);
				constructSocketMessage("set", "Virtual hostname has been toggled.");
				break;
			case API_SET_METRICSPREFIX:
				writeLog("Metrics prefix is toggled through API call.", 0, 0);
				constructSocketMessage("set", "Metrics prefix will be changed.");
				break;
			case API_SET_KAFKATOPIC:
				writeLog("Kafka topic name toggled through API call.", 1, 0);
				constructSocketMessage("set", "Kafka topic toggled.");
				break;
			case API_SET_JSONFILENAME:
				writeLog("Json file name is toggled through API call.", 1, 0);
				constructSocketMessage("set", "Json export file name toggled.");
				break;
			case API_SET_METRICSFILENAME:
				writeLog("Metrics file name is toggled through API call.", 1, 0);
				constructSocketMessage("set", "Metrics file name toggled.");
				break;
                        case API_SET_MAINTENANCE_STATUS:
                                writeLog("Maintenance has been toggled through API call.", 1, 0);
                                constructSocketMessage("maintenance", "Maintenance status has been updated.");
                                break;
			case API_SET_SCHEDULER_TYPE:
				writeLog("Scheduler type changed through API call.", 1, 0);
				constructSocketMessage("scheduler", "Scheduler type changed");
				break;
			case API_SET_PUSH_URL:
				writeLog("Push url changed through API call.", 1, 0);
				constructSocketMessage("pushurl", "Push url changed");
				break;
			case API_SET_PUSH_PORT:
				writeLog("Push port changed through API call.", 1, 0);
				constructSocketMessage("pushport", "Push port changed");
				break;
			case API_SET_PUSH_INTERVAL:
				writeLog("Push interval changed through API call.", 1, 0);
				constructSocketMessage("pushinterval", "Push interval changed");
				break;
			case API_GET_HOSTNAME:
				apiGetHostName();
				break;
			case API_GET_KAFKATAG:
				apiGetVars(1);
				break;
			case API_GET_METRICSPREFIX:
				apiGetVars(2);
				break;
			case API_GET_JSONFILENAME:
				apiGetVars(3);
				break;
			case API_GET_METRICSFILENAME:
				apiGetVars(4);
				break;
			case API_GET_KAFKATOPIC:
				apiGetVars(5);
				break;
			case API_GET_SLEEP:
				apiGetVars(6);
				break;
			case API_GET_SAVEONEXIT:
				apiGetVars(7);
				break;
			case API_GET_PLUGINOUTPUT:
				apiGetVars(8);
				break;
			case API_GET_KAFKA_START_ID:
				apiGetVars(9);
				break;
		        case API_GET_PLUGIN_RELOAD_TS:
				apiGetVars(10);
				break;
			case API_GET_SCHEDULER:
				apiGetVars(11);
				break;
			case API_GET_PUSH_URL:
				apiGetVars(12);
				break;
			case API_GET_PUSH_PORT:
				apiGetVars(13);
				break;
			case API_GET_PUSH_INTERVAL:
				apiGetVars(14);
				break;
			case API_CHECK_PLUGIN_CONFIG:
				apiCheckPluginConf();
				break;
			case API_RELOAD_CONFIG_HARD:
				apiReloadConfigHard();
				break;
			case API_RELOAD_CONFIG_SOFT:
				apiReloadConfigSoft();
				break;
			case API_RELOAD_ALMOND:
				apiReload();
				break;
			case API_ALMOND_VERSION:
				apiShowVersion();
				break;
			case API_ALMOND_STATUS:
				apiShowStatus();
				break;
			case API_ALMOND_PLUGINSTATUS:
				apiShowPluginStatus();
				break;
			case API_DENIED:
				constructSocketMessage("return", "Access denied: You need a valid token.");
                                break;
                        case API_ERROR:
				constructSocketMessage("return", "Error: Could not parse API call parameters.");
				break;
                	default:
                        	//printf("The request did not trigger any action.\n");
				constructSocketMessage("return", "The request id did not trigger any action.");
		}
        }
	else {
		if (api_action == API_MONITOR) {
                       	apiMonitorItem(id, aflags);
		}
		args_set = 0;
	}
	content_length = (size_t)strlen(socket_message); 
	int hdr_len = snprintf(NULL, 0, fmt, content_length);
	if (hdr_len < 0) {
		writeLog("[send_socket_message] snprintf size calculation failed.", 2, 0);
		return;
	}
	//sprintf(len, "%li", content_length);
	/*int written = snprintf(lenbuf, sizeof(lenbuf), "%zu", content_length);
	if (written < 0) {
		writeLog("[send_socket_message] snprintf error.", 2, 0);
		return;
	}
	if (written >= sizeof(lenbuf)) {
		writeLog("[send_socket_message] snprintf truncated output", 1, 0);
	}
        strcat(header, trim(lenbuf));
        strcat(header, "\n\n");*/
	char *header = malloc((size_t)hdr_len +1);
	if (!header) {
		writeLog("[send_socket_message] Out of memory allocating header.", 2, 0);
		return;
	}
	snprintf(header, (size_t)hdr_len + 1, fmt, content_length);
	//content_length += (size_t)strlen(header);
	total = (size_t)hdr_len + content_length;
	send_message = malloc(total +1);
	if (send_message == NULL) {
		perror("Failed to allocate memory for send_message");
		writeLog("Could not allocate memory [send_socket_message:send_message]", 2, 0);
		free(header);
		return;
	}
	//else
	//	memset(send_message, '\0', (content_length+1) * sizeof(char));
	memcpy(send_message, header, hdr_len);
        //strncpy(send_message, header, (size_t)(sizeof(header)));
	//strcat(send_message, socket_message);
	memcpy(send_message + hdr_len, socket_message, content_length);
	send_message[total] = '\0';
	if (use_ssl) {
		if (SSL_write(ssl, send_message, strlen(send_message)) <= 0) {
			writeLog("Could not send ssl message to client", 1, 0);
		}
	}
	else {
        	if (send(socket, send_message, strlen(send_message), 0) < 0) {
                	writeLog("Could not send message to client.", 1, 0);
        	}
	}
	writeLog("Message sent on socket. Closing connection.", 0, 0);
        close(socket);
	free(send_message);
	free(header);
	send_message = NULL;
	if (socket_message != NULL) {
		free(socket_message);
		socket_message = NULL;
	}
}

struct json_object* getJsonValue(struct json_object *jobj, const char* key) {
        struct json_object *tmp;
        if (json_object_object_get_ex(jobj, key, &tmp)) {
                return tmp;
        }
        return NULL;
}

void parseClientMessage(char str[], int arr[], bool jwt_valid) {
        struct json_object *jobj, *jaction, *jid, *jname,  *jflags;
        struct json_object *jargs, *jvalue, *jmode, *joption;
	struct json_object *jtoken;
        char *value = NULL;
        char action[13] = {0};
        char sid[10] = {0};
	char flags[10] = {0};
	char args[100] = {0};
	char sval[100] = {0};
	char name[50] = {0};
	char mode[10] = {0};
	char * fname = NULL;
        char * lname = NULL;
        char username[40] = {0};
        char* token = NULL;
        char line[100] = {0};
        int id = -1;
	int aflags = 0;
	int bExecute = 0;
        enum json_tokener_error jerr;

	args_set = 0;
	//printf("DEBUG: [parseClientMessage] str = %s\n", str);
        json_tokener *tok = json_tokener_new();
	if (str != NULL)
        	jobj = json_tokener_parse_ex(tok, str, (size_t)(strlen(str)));
	else {
		fprintf(stderr, "parseClientMessage: str is NULL.");
		writeLog("[parseClientMessage] Recieved NULL instead of string.", 1, 0);
           	json_tokener_free(tok);
		return;
	}
        jerr = json_tokener_get_error(tok);
        if (jerr != 0) {
                printf("jerr = %s\n", json_tokener_error_desc(jerr));
                printf("j = %p\n", jobj);
                printf("jerr_raw = %d\n", jerr);
		snprintf(infostr, infostr_size, "Json error: %s", json_tokener_error_desc(jerr));
		writeLog(trim(infostr), 1, 0);
		writeLog("Could not parse API call. Wrong syntax.", 1, 0);
		json_object_put(jobj);
           	json_tokener_free(tok);
                return;
        }
        json_object_object_foreach(jobj, key, val) {
                value = (char *) json_object_get_string(val);
		(void)key;
        }
        jaction = getJsonValue(jobj, "action");
        jid = getJsonValue(jobj, "id");
	jname = getJsonValue(jobj, "name");
	jflags = getJsonValue(jobj, "flags");
	jargs = getJsonValue(jobj, "args");
	jtoken = getJsonValue(jobj, "token");
	jvalue = getJsonValue(jobj, "value");
	jmode = getJsonValue(jobj, "mode");
	joption = getJsonValue(jobj, "option");
	if (jid != NULL) {
        	//strncpy(sid, json_object_to_json_string_ext(jid, JSON_C_TO_STRING_PLAIN), 5);
		snprintf(sid, sizeof(sid), "%s", json_object_to_json_string_ext(jid, JSON_C_TO_STRING_PLAIN));
        	removeChar(sid, '"');
	}
	if (jaction != NULL) {
        	//strncpy(action, json_object_to_json_string_ext(jaction, JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY), 12);
		snprintf(action, sizeof(action), "%s", json_object_to_json_string_ext(jaction, JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY));
        	removeChar(action, '"');
	}
	if (jname != NULL) {
		//strncpy(name, json_object_to_json_string_ext(jname, JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY), 50);
		snprintf(name, sizeof(name), "%s", json_object_to_json_string_ext(jname, JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY));
		removeChar(name, '"');
	}
	if (jmode != NULL) {
		//strncpy(mode, json_object_to_json_string_ext(jmode, JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY), 5);
		snprintf(mode, sizeof(mode), "%s", json_object_to_json_string_ext(jmode, JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY));
		removeChar(mode, '"');
	}
        if (jflags != NULL) {
		//strncpy(flags, json_object_to_json_string_ext(jflags, JSON_C_TO_STRING_PLAIN | JSON_C_TO_STRING_PRETTY), 10);
		snprintf(flags, sizeof(flags), "%s", json_object_to_json_string_ext(jflags, JSON_C_TO_STRING_PLAIN | JSON_C_TO_STRING_PRETTY));
		removeChar(flags, '"');
		if (strcmp(trim(flags), "verbose") == 0) {
			aflags = 1;
		}
		else if (strcmp(trim(flags), "dry") == 0) {
			aflags = API_DRY_RUN;
			api_action = API_DRY_RUN;
		}
		else if (strcmp(trim(flags), "all") == 0) {
			aflags = 10;
		}
		else if (strcmp(trim(flags), "soft") == 0) {
                        aflags = 200;
        	}
		else if (strcmp(trim(flags), "hard") == 0) {
			aflags = 205;
		}
		else aflags = 0;
	}
	if (jargs != NULL) {
		//strncpy(args, json_object_to_json_string_ext(jargs, JSON_C_TO_STRING_PLAIN | JSON_C_TO_STRING_PRETTY), 100);
		snprintf(args, sizeof(args), "%s", json_object_to_json_string_ext(jargs, JSON_C_TO_STRING_PLAIN | JSON_C_TO_STRING_PRETTY));
		removeChar(args, '"');
		if (aflags > 199) {
			if (joption != NULL) {
				// Make customMonitorVals atomic
				char option[25] = {0};
				snprintf(option, sizeof(option), "%s", json_object_to_json_string_ext(joption, JSON_C_TO_STRING_PLAIN | JSON_C_TO_STRING_PRETTY));
				removeChar(option, '"');
				if (customMonitorVals != NULL) {
					free(customMonitorVals);
					customMonitorVals = NULL;
				}
				size_t cmv_size = sizeof(args) + sizeof(option);
				customMonitorVals = malloc(cmv_size);
				snprintf(customMonitorVals, cmv_size, "%s;%s", args, option);
				aflags++;
			} 
			else {
				printf("DEBUG: [parseClientMessage] joption == NULL\n");
			}
		}
		args_set++;
	}
	else args_set = 0;
	if (jvalue != NULL) {
		//strncpy(sval, json_object_to_json_string_ext(jvalue, JSON_C_TO_STRING_PLAIN | JSON_C_TO_STRING_PRETTY), 100);
		snprintf(sval, sizeof(sval), "%s", json_object_to_json_string_ext(jvalue, JSON_C_TO_STRING_PLAIN | JSON_C_TO_STRING_PRETTY));
		removeChar(sval, '"');
	}
	bExecute = jwt_valid ? 1 : 0;
	if (jtoken != NULL && !bExecute) {
		token = malloc(30);
		if (token == NULL) {
			writeLog("Could not allocate memory for execute token", 1, 0);
		}
		else
			memset(token, '\0', 30 * sizeof(char));
                //strncpy(token, json_object_to_json_string_ext(jtoken, JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY), 30);
		snprintf(token, 30, "%s", json_object_to_json_string_ext(jtoken, JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY));
                removeChar(token, '"');
		trim(token);
                FILE *in_file = fopen("/etc/almond/tokens", "r");
                if (in_file == NULL)
                {
                        writeLog("Could not find token file.", 1, 0);
                }
                else {
                        int i = 1;
                        while (fscanf(in_file, "%s", line) == 1) {
                                if (i == 1){
					/*fname = malloc((size_t)sizeof(line)+1);
					if (fname == NULL) {
						writeLog("Could not allocate message [parseClientMessage:fname]", 2, 0);
						json_object_put(jobj);
   						json_tokener_free(tok);
						return;
					}
					else
						memset(fname, '\0', (size_t)sizeof(line)+1 * sizeof(char));
					strncpy(fname, trim(line), sizeof(line));*/
					char *trimmed_line = trim(line);
					size_t len = strlen(trimmed_line);
					fname = malloc(len +1);
					if (fname == NULL) {
						writeLog("Could not allocate message [parseClientMessage:fname]", 2, 0);
						json_object_put(jobj);
   						json_tokener_free(tok);
       						return;
    					}
					strcpy(fname, trimmed_line); 
                                }
                                if (i == 2){
                                        lname = malloc((size_t)sizeof(line)+1);
					if (lname == NULL) {
						writeLog("Could not allocate memory [parseClientMessage:lname]", 2, 0);
						json_object_put(jobj);
   						json_tokener_free(tok);
						return;
					}
					else
						memset(lname, '\0', (size_t)sizeof(line)+1 * sizeof(char));
					strncpy(lname, trim(line), sizeof(line));
                                }
                                i++;
                                if (strstr(line, token) != 0) {
                                        bExecute = 1;
                                        // Get username from file to log
					/*strncpy(username, "", 2);
					strcat(username, fname);
                                        strcat(username, " ");
                                        strcat(username, lname);*/
					snprintf(username, sizeof(username), "%s %s", fname, lname);
                                        snprintf(infostr, infostr_size, "User '%s' granted API execution rights from token.", username);
                                        writeLog(trim(infostr), 0, 0);
                                        flushLog();
					free(fname);
					free(lname);
					fname = lname = NULL;
                                        break;
                                }
                                if (i == 4){
                                        i = 1;
                                        free(fname);
                                        free(lname);
					fname = NULL;
					lname = NULL;
                                }
                        }
			fclose(in_file);
			in_file = NULL;
                }
		free(token);
		token = NULL;
        }
        if ((strcmp(trim(action), "read") == 0) || (strcmp(trim(action), "get") == 0)) {
		if (aflags == 10) {
			api_action = API_READ_ALL;
		}
		else {
                	api_action = API_READ;
		}
        }
	else if (strcmp(trim(action), "monitor") == 0) {
		api_action = API_MONITOR;
	}
        else if ((strcmp(trim(action), "execute") == 0)|| (strcmp(trim(action), "run") == 0)) {
		if (bExecute > 0) {
                        if (strcmp(trim(name), "gardener") == 0) {
                                api_action = API_EXECUTE_GARDENER;
                        }
                        else if (api_action != API_DRY_RUN)
                                api_action = API_RUN;
                }
                else api_action = API_DENIED;
        }
	else if ((strcmp(trim(action), "runread") == 0) || (strcmp(trim(action), "exread") == 0)) {
		if (bExecute != 0)
			api_action = API_EXECUTE_AND_READ;
		else 
			api_action = API_DENIED;
	}
	else if ((strcmp(trim(action), "metrics") == 0) || (strcmp(trim(action), "getm") == 0)) { 
		api_action = API_GET_METRICS;
	}
        else if (strcmp(trim(action), "maintenance") == 0) {
                if (jid == NULL) {
                	id = getIdFromName(trim(name));
                }
                else {
			id = atoi(sid);
		}
		if (id < 0) {
			api_action = API_ERROR;
		}
		else {
			if ((strcmp(trim(value), "true") == 0) || (strcmp(trim(value), "false") == 0)) {
				setMaintenanceStatus(id, trim(value));
        			api_action = API_SET_MAINTENANCE_STATUS;
			}
			else {
				api_action = API_ERROR;
			}
		}
        }	
	else if ((strcmp(trim(action), "enable") == 0) || (strcmp(trim(action), "disable") == 0)) {
 		if (bExecute != 0) {
 			if (strcmp(trim(name), "timetuner") == 0) {
 				if (strcmp(trim(action), "enable") == 0)
 					api_action = API_ENABLE_TIMETUNER;
 				else if (strcmp(trim(action), "disable") == 0)
 					api_action = API_DISABLE_TIMETUNER;
 			}
 			if (strcmp(trim(name), "gardener") == 0) {
 				if (strcmp(trim(action), "enable") == 0)
 					api_action = API_ENABLE_GARDENER;
 				else if (strcmp(trim(action), "disable") == 0)
 					api_action = API_DISABLE_GARDENER;
 			}
                        if (strcmp(trim(name), "cleancache") == 0) {
                                if (strcmp(trim(action), "enable") == 0)
                                        api_action = API_ENABLE_CLEARCACHE;
                                else if (strcmp(trim(action), "disable") == 0)
                                        api_action = API_DISABLE_CLEARCACHE;
                        }
			if (strcmp(trim(name), "quickstart") == 0) {
                                if (strcmp(trim(action), "enable") == 0)
                                        api_action = API_ENABLE_QUICKSTART;
                                else if (strcmp(trim(action), "disable") == 0)
                                        api_action = API_DISABLE_QUICKSTART;
                        }
			if (strcmp(trim(name), "standalone") == 0) {
                                if (strcmp(trim(action), "enable") == 0)
                                        api_action = API_ENABLE_STANDALONE;
                                else if (strcmp(trim(action), "disable") == 0)
                                        api_action = API_DISABLE_STANDALONE;
                        }
			if (strcmp(trim(name), "push") == 0) {
                                if (strcmp(trim(action), "enable") == 0)
                                        api_action = API_ENABLE_PUSH;
                                else if (strcmp(trim(action), "disable") == 0)
                                        api_action = API_DISABLE_PUSH;
                        }
			if (strcmp(trim(name), "pushmetrics") == 0) {
                                if (strcmp(trim(action), "enable") == 0)
                                        api_action = API_ENABLE_METRICS_PUSH;
                                else if (strcmp(trim(action), "disable") == 0)
                                        api_action = API_DISABLE_METRICS_PUSH;
                        }
 		}
		else
			api_action = API_DENIED;
	}
	else if ((strcmp(trim(action), "set") == 0) || (strcmp(trim(action), "setvar") == 0)) {
		if (bExecute != 0) {
			pthread_mutex_lock(&update_mtx);
			//printf("DEBUG: sval = %s\n", trim(sval));
			if (strcmp(trim(name), "pluginoutput") == 0) {
				int val = atoi(trim(sval));
				setPluginOutput(val);
				changeSetValue(1, val);
				api_action = API_SET_PLUGINOUTPUT;
			}
			else if (strcmp(trim(name), "saveonexit") == 0) {
				int val = atoi(trim(sval));
				changeSetValue(2, val);
				api_action = API_SET_SAVEONEXIT;
			}
			else if (strcmp(trim(name), "sleep") == 0) {
				int val = atoi(trim(sval));
				changeSetValue(10, val);
				api_action = API_SET_SLEEP;
			}
			else if (strcmp(trim(name), "kafkatag") == 0) {
				setApiCmdFile("kafkatag", trim(sval));
				writeLog("A command file for changing kafkatag has been created.", 0, 0);
				api_action = API_SET_KAFKATAG;
			}
			else if (strcmp(trim(name), "kafkatopic") == 0) {
				setApiCmdFile("kafkatopic", trim(sval));
				writeLog("A command file for changing Kafka topic name has been created.", 0, 0);
				api_action = API_SET_KAFKATOPIC;
			}
			else if (strcmp(trim(name), "jsonfilename") == 0) {
				setApiCmdFile("jsonfilename", trim(sval));
				writeLog("A command file for changing json export file name has been created.", 0, 0);
				api_action = API_SET_JSONFILENAME;
			}
			else if (strcmp(trim(name), "metricsfilename") == 0) {
				setApiCmdFile("metricsfilename", trim(sval));
				writeLog("A command file for changing metrics file name has been created.", 0, 0);
				api_action = API_SET_METRICSFILENAME;
			}
			else if (strcmp(trim(name), "pushurl") == 0) {
				setApiCmdFile("pushurl", trim(sval));
				writeLog("A command file for changing push url has been created.", 0, 0);
				api_action = API_SET_PUSH_URL;
			}
			else if (strcmp(trim(name), "pushport") == 0) {
				int val = atoi(trim(sval));
				changeSetValue(12, val);
				api_action = API_SET_PUSH_PORT;
			}
			else if (strcmp(trim(name), "pushinterval") == 0) {
				int val = atoi(trim(sval));
				changeSetValue(13, val);
				api_action = API_SET_PUSH_INTERVAL;
			}
			else if (strcmp(trim(name), "kafkastartid") == 0) {
				int val = atoi(trim(sval));
				if (val > 0) {
					changeSetValue(11, val);
                                	snprintf(infostr, infostr_size, "Kafka start id is set to '%d'", val);
                                	writeLog("Kafka start id is toggled through API call.", 0, 0);
                                	writeLog(trim(infostr), 0, 0);
				}
				else {
					snprintf(infostr, infostr_size, "Could not set Kafka start id to '%s'", sval);
					writeLog("Kafka start id was toggled through API call.", 0, 0);
					writeLog(trim(infostr), 1, 0);
				}
				api_action = API_SET_KAFKA_START_ID;
                        }
			else if (strcmp(trim(name), "hostname") == 0) {
				char* newname = malloc(256);
				if (!newname) {
					perror("Failed to allocate memory");
					exit(EXIT_FAILURE);
				}
				else
					memset(newname, '\0', 256);
				strncpy(newname, trim(sval), strlen(sval));
				snprintf(infostr,  infostr_size, "Virtal hostname set to '%s'", newname);
				writeLog("Hostname (virtual) is toggled through API call.", 1, 0);
				writeLog(trim(infostr), 1, 0);
				free(newname);
				setApiCmdFile("hostname", trim(sval));
				api_action = API_SET_HOSTNAME;
			}
			else if (strcmp(trim(name), "metricsprefix") == 0) {
				char* newname = malloc(31);
				if (!newname) {
					writeLog("Could not allocate memory [parseClientMessage:metricsprefix->newname]\n", 1, 0);
					exit(EXIT_FAILURE);
				}
				else
					memset(newname, '\0', 31);
				strncpy(newname, trim(sval), strlen(sval));
				free(newname);
				setApiCmdFile("metricsprefix", trim(sval));
				api_action = API_SET_METRICSPREFIX;
			}
			else if (strcmp(trim(name), "scheduler") == 0) {
				char* s_type = malloc(9);
				if (!s_type) {
					writeLog("Could not allocate memory[parseClientMessage: scheduler_type]\n", 1, 0);
					exit(EXIT_FAILURE);
				}
				else
					memset(s_type, '\0', 9);
				strncpy(s_type, trim(sval), strlen(sval));
				if (strcmp(s_type, "external") == 0) {
					writeLog("External scheduler activated. Almond scheduler is now inactive.", 1, 0);
					setApiCmdFile("scheduler", "external");
					api_action = API_SET_SCHEDULER_TYPE;
				}
				else if (strcmp(s_type, "internal") == 0) {
					writeLog("Almond scheduler now activated through API call.", 0, 0);
					setApiCmdFile("scheduler", "internal");
					api_action = API_SET_SCHEDULER_TYPE;
				}
				else {
					writeLog("Failed to change scheduler type. Unrecognized value supplied.", 1, 0);
					api_action = -1;
				}
				free(s_type);
			}
			else {
				api_action = -1;
			}
			pthread_mutex_unlock(&update_mtx);
		}
		else {
			writeLog("API action was denied. Wrong or no token supplied.", 1, 0);
			api_action = API_DENIED;
		}
	}
	else if (strcmp(trim(action), "getvar") == 0) {
                if (strcmp(trim(name), "hostname") == 0) {
                        api_action = API_GET_HOSTNAME;
                }
		else if (strcmp(trim(name), "kafkatag") == 0) {
			api_action = API_GET_KAFKATAG;
		}
		else if (strcmp(trim(name), "metricsprefix") == 0) {
			api_action = API_GET_METRICSPREFIX;
		}
		else if (strcmp(trim(name), "jsonfilename") == 0) {
			api_action = API_GET_JSONFILENAME;
		}
		else if (strcmp(trim(name), "metricsfilename") == 0) {
			api_action = API_GET_METRICSFILENAME;
		}
		else if (strcmp(trim(name), "kafkatopic") == 0) {
			api_action = API_GET_KAFKATOPIC;
		}
		else if (strcmp(trim(name), "sleep") == 0) {
                        api_action = API_GET_SLEEP;
                }
		else if (strcmp(trim(name), "saveonexit") == 0) {
			api_action = API_GET_SAVEONEXIT;
		}
		else if (strcmp(trim(name), "pluginoutput") == 0) {
			api_action = API_GET_PLUGINOUTPUT;
		}
		else if (strcmp(trim(name), "kafkastartid") == 0) {
			api_action = API_GET_KAFKA_START_ID;
		}
		else if (strcmp(trim(name), "scheduler") == 0) {
			api_action = API_GET_SCHEDULER;
		}
		else if (strcmp(trim(name), "pushurl") == 0) {
			api_action = API_GET_PUSH_URL;
		}
		else if (strcmp(trim(name), "pushport") == 0) {
			api_action = API_GET_PUSH_PORT;
		}
		else if (strcmp(trim(name), "pushinterval") == 0) {
			api_action = API_GET_PUSH_INTERVAL;
		}
		else {
			api_action = -1;
		}
        }
	else if (strcmp(trim(action), "check") == 0) {
		if (strcmp(trim(name), "pluginconfig") == 0) {
			api_action = API_CHECK_PLUGIN_CONFIG;
		}
		else if (strcmp(trim(name), "pluginconfigts") == 0) {
			api_action = API_GET_PLUGIN_RELOAD_TS;
		}
		else {
			api_action = -1;
		}
	}
	else if (strcmp(trim(action), "reload") == 0) {
		if (bExecute != 0) {
			if (strcmp(trim(name), "almond") == 0) {
				// Reload Almond
				api_action = API_RELOAD_ALMOND;
			}
			else if (strcmp(trim(name), "plugins") == 0) {
				if (strcmp(trim(mode), "hard") == 0) {
					// Hard reload
					api_action = API_RELOAD_CONFIG_HARD;
				}
				else if (strcmp(trim(mode), "soft") == 0) {
					// Soft reload
					api_action = API_RELOAD_CONFIG_SOFT;
				}
				else {
					api_action = -1;
				}
			}
			else {
				api_action = -1;
			}
		}
		else {
			api_action = API_DENIED;
		}
	}
	else if(strcmp(trim(action), "almond") == 0) {
		if (strcmp(trim(name), "version") == 0) {
			api_action = API_ALMOND_VERSION;
		}
		else if (strcmp(trim(name), "status") == 0) {
			api_action = API_ALMOND_STATUS;
		}
		else if (strcmp(trim(name), "plugins") == 0) {
			api_action = API_ALMOND_PLUGINSTATUS;
		}
		else {
			api_action = -1;
		}
	}
        else {
                api_action = 0;
        }
        if (api_action > 0) {
                id = atoi(sid);
                if (id == 0) {
			if (jname != NULL) {
				id = getIdFromName(name);
				if (id == -1) {
					// Some api action does not need name
					if (api_action > API_NAME_END && api_action < API_NAME_START) {
						snprintf(infostr, infostr_size, "Try to run API command with name '%s', which does not exist.", name);
                                        	writeLog(trim(infostr), 1, 0);
						api_action = 0;
						json_object_put(jobj);
   						json_tokener_free(tok);
						return;
					}
					else {
						json_object_put(jobj);
   						json_tokener_free(tok);
						return;
					}
				}
			}	
			else {
				writeLog("Received a bad json-request. API call is aborted.", 1, 0);
				api_action = 0;
				json_object_put(jobj);
   				json_tokener_free(tok);
                        	return;
			}
			if (id < 0) {
				writeLog("Could not get id from name. This might cause strange things to happen. Aborting API call.", 1, 0);
				api_action = 0;
				json_object_put(jobj);
   				json_tokener_free(tok);
				return;
			}
                }
                id--;
		if (args_set > 0 && (api_action == API_RUN || api_action == API_DRY_RUN || api_action == API_EXECUTE_AND_READ || api_action == API_MONITOR)) {
			size_t arg_len = strlen(args) + 1;
			api_args = malloc(arg_len);
			if (api_args == NULL) {
				fprintf(stderr, "Could not allocate memory.\n");
				writeLog("Could not allocate memory [parseClientMessage:api_args]", 2, 0);
				json_object_put(jobj);
   				json_tokener_free(tok);
				return;
			}
			else
				memset(api_args, '\0', (size_t)strlen(args)+1 * sizeof(char));
			//size_t len = strlen(args)+ 1;
			/*strncpy(api_args, args, len-1);
			api_args[len-1] = '\0';*/
			//snprintf(api_args, len, "%s", args);
			snprintf(api_args, arg_len, "%s", args);
			if (api_action != API_MONITOR) {
				runPluginArgs(id, aflags, api_action);
				if (timeScheduler) {
					rescheduleChecks();
				}
				free(api_args);
				api_args = NULL;
			}
		}
        }
	json_tokener_free(tok);
	if (jobj) json_object_put(jobj);
	arr[0] = id;
	arr[1] = aflags;
}

SSL_CTX *create_context() {
    	const SSL_METHOD *method;
    	SSL_CTX *ctx;

   	method = TLS_server_method();

    	ctx = SSL_CTX_new(method);
	if (!ctx) {
        	perror("Unable to create SSL context");
        	ERR_print_errors_fp(stderr);
        	exit(EXIT_FAILURE);
    	}
    	return ctx;
}

void configure_context(SSL_CTX *ctx) {
	/* Set the key and cert */
   	if (SSL_CTX_use_certificate_file(ctx, almondCertificate, SSL_FILETYPE_PEM) <= 0) {
        	ERR_print_errors_fp(stderr);
        	exit(EXIT_FAILURE);
    	}
    	if (SSL_CTX_use_PrivateKey_file(ctx, almondKey, SSL_FILETYPE_PEM) <= 0 ) {
        	ERR_print_errors_fp(stderr);
        	exit(EXIT_FAILURE);
    	}
	/*if (!SSL_CTX_use_certificate_chain_file(ctx, "almonds.crt"))
        	ERR_print_errors_fp(stderr);*/
	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);

    	if(!SSL_CTX_check_private_key(ctx)) {
        	fprintf(stderr, "Private key does not match the certificate public key\n");
        	exit(EXIT_FAILURE);
    	}
}

int initSocket () {
        int opt = 1;
        if ((server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
                perror("Socket failed");
                writeLog("Could not initiate socket.", 2, 0);
                return -1;
        }
	/*int flags = fcntl(server_fd, F_GETFL, 0);
	if (flags == -1) {
    		perror("fcntl F_GETFL failed");
    		writeLog("Failed to get socket flags.", 2, 0);
    		return -1;
	}
	if (fcntl(server_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    		perror("fcntl F_SETFL failed");
    		writeLog("Failed to set socket to non-blocking.", 2, 0);
    		return -1;
	}*/
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt,sizeof(opt))) {
                perror("setsockopt");
                writeLog("Setsockopt failed.", 2, 0);
                return -1;
        }
	bzero((char *)&address, sizeof(address));
	//memset(&address, 0, sizeof(address);
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        if (local_port == ALMOND_API_PORT)
                address.sin_port = htons((uint16_t)ALMOND_API_PORT);
        else
                address.sin_port = htons((uint16_t)local_port);
        if (bind(server_fd, (struct sockaddr*)&address,sizeof(address))< 0) {
                perror("bind failed");
                writeLog("Failed to bind port.", 2, 0);
                return -1;
        }
	if (use_ssl) {
    		OpenSSL_add_all_algorithms();
		SSL_load_error_strings();
        	ctx = create_context();
                configure_context(ctx);
        }
        writeLog("Almond socket initialized.", 0, 0);
        socket_is_ready = 1;
        return socket_is_ready;
}

int createSocket(int server_fd) {
    char local_msg[infostr_size];
    int client_socket;
    socklen_t client_size;
    struct sockaddr_in client_addr;
    int params[2];
    SSL *ssl = NULL;

    memset(local_msg, 0, infostr_size);

    server_message = malloc((size_t)socketservermessage_size + 1);
    if (server_message == NULL) {
        fprintf(stderr, "Failed to allocate memory for servermessage.\n");
        writeLog("Failed to allocate memory [createSocket:servermessage].", 1, 0);
        return -1;
    }
    memset(server_message, '\0', (size_t)socketservermessage_size + 1);

    if (iam_public_key_file == NULL)
        iam_public_key = NULL;
    else
        iam_public_key = load_file_to_string(iam_public_key_file);

    if (!iam_public_key) {
        writeLog("IAM public key not found — JWT authentication disabled", 0, 0);
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        writeLog("Failed listening...", 2, 0);
        socket_is_ready = 0;
        free(server_message);
        return -1;
    }

    snprintf(local_msg, infostr_size, "Ready listening on port %d", local_port);
    writeLog(trim(local_msg), 0, 0);

    client_size = sizeof(client_addr);

    while (!is_stopping) {

        client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_size);
        if (client_socket < 0) {
            int e = errno;
            if (e == EINTR || e == EBADF || e == EINVAL || is_stopping)
                break;
            writeLog("Could not accept client socket.", 1, 0);
            continue;
        }

        // SSL handshake
        if (use_ssl) {
            ssl = SSL_new(ctx);
            SSL_set_fd(ssl, client_socket);
            if (SSL_accept(ssl) <= 0) {
                writeLog("SSL handshake failed.", 1, 0);
                SSL_free(ssl);
                close(client_socket);
                continue;
            }
        }

        // Allocate per-request buffer
        char *client_message = malloc(socketclientmessage_size + 1);
        if (!client_message) {
            writeLog("Failed to allocate memory for client_message.", 1, 0);
            if (use_ssl) {
                SSL_shutdown(ssl);
                SSL_free(ssl);
            }
            close(client_socket);
            continue;
        }
        memset(client_message, 0, socketclientmessage_size + 1);

        bool jwt_valid = false;

        // Read request
        int n = use_ssl
            ? SSL_read(ssl, client_message, socketclientmessage_size)
            : recv(client_socket, client_message, socketclientmessage_size, 0);

        if (n <= 0) {
            writeLog("Could not receive client message.", 1, 0);
            free(client_message);
            if (use_ssl) {
                SSL_shutdown(ssl);
                SSL_free(ssl);
            }
            close(client_socket);
            continue;
        }

        // JWT
        char username[128] = {0};
	char fullname[128] = {0};
        char *auth_header = extract_authorization_header(client_message);
        char *auth_token  = auth_header ? extract_bearer_token(auth_header) : NULL;
        if (auth_token && iam_public_key && 
            validate_jwt(auth_token, iam_public_key, username, sizeof(username), fullname, sizeof(fullname))) {
            jwt_valid = true;
	    snprintf(infostr, infostr_size,"User '%s' (%s) granted API execution rights from IAM provider.",fullname, username);
    	    writeLog(trim(infostr), 0, 0);
        } else if (auth_token) {
            writeLog("JWT decode or validation failed", 1, 0);
        }

        free(auth_header);
        free(auth_token);

        // Extract JSON payload
        char *json_start = strchr(client_message, '{');
        char message[250] = {0};

        if (json_start) {
        	size_t len = strlen(json_start);
            	if (len >= sizeof(message)) {
			len = sizeof(message) - 3;
			memcpy(message, json_start, len);
			message[len] = '"';
			message[len+1] = '}';
			message[len+2] = '\0';
		}
		else {
            		memcpy(message, json_start, len);
            		message[len] = '\0';
		}
        } else {
        	writeLog("JSON payload not found [clientMessage]", 1, 0);
            	message[0] = '\0';
        }

        parseClientMessage(message, params, jwt_valid);

        // Send response
        if (use_ssl)
            send_socket_message(NO_SOCKET, ssl, params[0], params[1]);
        else
            send_socket_message(client_socket, NULL, params[0], params[1]);

        // Cleanup
        free(client_message);
        if (use_ssl) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
        }
        close(client_socket);
    }

    close(server_fd);
    free(server_message);
    return 0;
}

void closeSocket() {
        writeLog("Closing socket.", 0, 0);
        shutdown(server_fd, SHUT_RDWR);
}

void closejsonfile() {
	const char bFolderName[7] = "backup";
	char ch = '/';
	char dot = '.';
        
	snprintf(dataFileName, datafilename_size, "%s%c%s", dataDir, ch, jsonFileName);


	if (saveOnExit == false) {
		//printf("\nDEBUG: Save on exit. Remove %s\n", dataFileName);
		remove(dataFileName);
	}
	else {
		char date[13];
		time_t now = time(NULL);
		struct tm *t = localtime(&now);
                strftime(date, sizeof(date), "%Y%m%d%H%M", t);
		snprintf(backupDirectory, backupdirectory_size, "%s%c%s", dataDir, ch, bFolderName);
		if (directoryExists(backupDirectory, 100) != 0) {
			int status = mkdir(trim(backupDirectory), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
			if (status != 0 && errno != EEXIST) {
				printf("Failed to create backup directory. Errno: %d\n", errno);
				return;
			}
		}
		char bd[backupdirectory_size];
		char jfn[filename_size];
		memset(bd, 0, sizeof(bd));
		memset(jfn, 0, sizeof(jfn));
		strncpy(bd, backupDirectory, backupdirectory_size);
		strncpy(jfn, jsonFileName, filename_size);
		snprintf(newFileName, newfilename_size, "%s%c%s%c%s", bd, ch, jfn, dot, date);
		rename(dataFileName, newFileName);
	}	
}

void safe_free(void** ptr) {
	if (*ptr != NULL) {
		free(*ptr);
		*ptr = NULL;
	}
}

void safe_free_str(char **ptr) {
	if (ptr && *ptr) {
        	free(*ptr);
        	*ptr = NULL;
    	}
}

void free_kafka_vars() {
	if (kafkaexportreqs > 0) {
		free(kafka_brokers);
		if (kafka_topic != NULL) {
			free(kafka_topic);
			kafka_topic = NULL;
		}
		if (kafka_tag != NULL) { 
			free(kafka_tag);
			kafka_tag = NULL;
		}
		free(kafkaCACertificate);
		free(kafkaProducerCertificate);
		free(kafkaSSLKey);
		kafka_brokers = NULL;
		kafka_topic = NULL;
		kafka_tag = NULL;
		kafkaCACertificate = NULL;
		kafkaProducerCertificate = NULL;
		kafkaSSLKey = NULL;
	}
}

void free_iam_roles(void) {
	if (iam_roles_accepted) {
		for (int i = 0; i < iam_roles_count; i++) {
            		safe_free_str(&iam_roles_accepted[i]);  
        	}
        	free(iam_roles_accepted);          
		iam_roles_accepted = NULL;
    	}
    	iam_roles_count = 0;
}

void free_constants() {
	safe_free_str(&confDir);
	safe_free_str(&dataDir);
	safe_free_str(&storeDir);
	safe_free_str(&logDir);
	safe_free_str(&pluginDeclarationFile);
	safe_free_str(&jsonFileName);
	safe_free_str(&metricsFileName);
	safe_free_str(&gardenerScript);
	safe_free_str(&infostr);
	safe_free_str(&pluginDir);
	safe_free_str(&hostName);
	safe_free_str(&fileName);
	safe_free_str(&metricsOutputPrefix);
	safe_free_str(&logfile);
	safe_free_str(&dataFileName);
	safe_free_str(&backupDirectory);
	safe_free_str(&newFileName);
	safe_free_str(&gardenerRetString);
	safe_free_str(&pluginCommand);
	safe_free_str(&pluginReturnString);
	safe_free_str(&storeName);
	safe_free_str(&schemaRegistryUrl);
	safe_free_str(&socket_message);
	//safe_free_str(&client_message);
	safe_free_str(&kafkaConfigFile);
	safe_free_str(&push_url);
        safe_free_str(&iam_issuer);
	safe_free_str(&iam_public_key_file);
	safe_free_str(&iam_aud);
	
	//safe_free_str(&logmessage);
	writeLog("All constants freed from memory.", 0, 0);
}

static void free_plugin_item(PluginItem *item) {
    if (!item) return;

    HASH_DEL(g_plugin_map, item);

    free(item->name);
    free(item->description);
    free(item->command);

    free(item->output.retString);

    free(item);
}

void free_all_plugins(void) {
    PluginItem *item, *tmp;

    HASH_ITER(hh, g_plugin_map, item, tmp) {
        free_plugin_item(item);
    }
    g_plugin_map = NULL;

    free(g_plugins);
    g_plugins       = NULL;
    g_plugin_count  = 0;
}

void free_structures(int numOfS) {
	free_all_plugins();
	/*if (scheduler != NULL) {
		free(scheduler);
	}*/
}

void freemem() {
	confDir = NULL;
	dataDir = NULL;
	storeDir = NULL;
	pluginDir = NULL;
	logDir = NULL;
	pluginDeclarationFile = NULL;
	hostName = NULL;
	fileName = NULL;
	jsonFileName = NULL;
	metricsFileName = NULL;
	gardenerScript = NULL;
	infostr = NULL;
	if (socket_message != NULL) {
		free(socket_message);
		socket_message = NULL;
	}
	dataFileName = NULL;
	backupDirectory = NULL;
	newFileName = NULL;
	gardenerRetString = NULL;
	pluginCommand = NULL;
	pluginReturnString = NULL;
	storeName = NULL;
        if (update_g_plugins != NULL) {
		for (int i = 0; i < update_declaration_size; i++) {
                	free(update_g_plugins[i].name);
                        free(update_g_plugins[i].description);
                        free(update_g_plugins[i].command);
			update_g_plugins[i].name = NULL;
			update_g_plugins[i].description = NULL;
			update_g_plugins[i].command = NULL;
                }
                free(update_g_plugins);
		update_g_plugins = NULL;
	}
	/*if (update_outputs != NULL) {
		for (int i=0; i < update_output_size; i++) {
			free(update_outputs[i].retString);
			update_outputs[i].retString = NULL;
		}
		free(update_outputs);
	}*/
	if (api_args != NULL) {
		free(api_args);
		api_args = NULL;
	}
}

void destroy_mutexes() {
	pthread_mutex_destroy(&mtx);
	pthread_mutex_destroy(&update_mtx);
	destroy_log_mutex();
}

void sig_exit_app() {
	is_file_open = 1;
	pthread_cond_broadcast(&file_opened);
	closeSocket();
	shutdown_phase = 1;
        flushLog();
	printf("\nClosing ");
	for (int i = 0; i < 6; i++) {
		printf("%i ", i+1);
		fflush(stdout);
		sleep(1);
	}
	writeLog("Almond says goodbye.", 0, 0);
	shutdown_phase = 2;
	closeLog();
        closejsonfile();
        //int try_count = 0;
        /*while (thread_counter > 0) {
                writeLog("Waiting for threads to finish...", 0, 0);
                fflush(fptr);
                sleep(2);
                printf("There are %i threads waiting to finish.\n", thread_counter);
                try_count++;
                if (try_count >= max_try) break;
        }*/
	for (int i = 0; i < thread_counter; ++i) {
        	pthread_join(threadIds[i], NULL);
    	}
        free_structures(decCount);
	if (scheduler) {
		free(scheduler);
		scheduler = NULL;
	}
        free(g_plugins);
	if (useKafkaConfigFile) {
  		free_kafka_memalloc();
	}	
        free_kafka_vars();
	free_iam_roles();
        free_constants();
        //free(threadIds);
        freemem();

	destroy_mutexes();
	if (fptr != NULL) {
		fclose(fptr);
        	fptr = NULL;
	}
        fflush(stdout);
        fflush(stderr);
	if (logmessage) {
        	memset(logmessage, 0, strlen(logmessage));  // Optional: zero out content
                free(logmessage);
                logmessage = NULL;
        }
        printf("\nExiting application.\n");
}

void install_signals(void) {
	struct sigaction sa;
    	memset(&sa, 0, sizeof(sa));
    	sa.sa_handler = sig_handler;
    	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART | SA_ONSTACK;;
	/*sigemptyset(&signal_set);
        sigaddset(&signal_set, SIGINT);
    	sigaddset(&signal_set, SIGTERM);
    	pthread_sigmask(SIG_BLOCK, &signal_set, NULL);*/
    	sigaction(SIGINT,  &sa, NULL);
    	sigaction(SIGTERM, &sa, NULL);
}

void sig_handler(int sig){
	if (already_exiting) return;
	is_stopping = 1;
	already_exiting = 1;
	shutdown_reason = (sig == SIGINT ? SR_SIGINT
        	: sig == SIGTERM ? SR_SIGTERM
                : SR_NORMAL);
    	/*switch (sigl) {
        	case SIGINT:
			shutdown_reason = SR_SIGINT;
			break;
		case SIGKILL:
			shutdown_reason = SR_SIGKILL;
			break;
		case SIGTERM:
			shutdown_reason = SR_SIGTERM;
			break;
		case SIGSTOP:
			shutdown_reason = SR_SIGSTOP;
			break;
    	}*/
	if (server_fd >= 0) {
		shutdown(server_fd, SHUT_RDWR);
       		close(server_fd);
        	server_fd = -1;
	}
}

int fileExists(const char *checkFile) {
	if (access(checkFile, F_OK) == 0) 
		return 0;
	else
		return 1;
}

int checkPluginFileStat(const char *path, time_t oldMTime, int set) {
	struct stat file_stat;
	int err = stat(path, &file_stat);
	if (err != 0) {
		perror(" [file_is_modified] stat");
		exit(errno);
	}
	tPluginFile = file_stat.st_mtime;
	if (set > 0) 
		return 0;
	else
		return file_stat.st_mtime > oldMTime;
}

char *getHostName() {
	struct addrinfo hints, *info, *p;
	int gai_result;
        char host_name[1024];
	char *ret = malloc(255);

	host_name[1023] = '\0';
	gethostname(host_name, 1023);
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;

	if ((gai_result = getaddrinfo(host_name, "http", &hints, &info)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_result));        }
	for (p = info; p != NULL; p = p->ai_next) {
		size_t dest_size = 255;
                snprintf(ret, dest_size, "%s", p->ai_canonname);
	}
	freeaddrinfo(info);
	info = NULL;
	return ret;
}

#if 0
/* process_almond_api moved to config.c */
void process_almond_api(ConfVal value) {
	if ((strcmp(value.strval, "true") == 0) || (value.intval >= 1)) {
		local_api = true;
	}
}
#endif

/* process_almond_certificate moved to config.c */
#if 0
void process_almond_certificate(ConfVal value) {
	almondCertificate = malloc((size_t)strlen(value.strval)+1);
	if (almondCertificate == NULL) {
		fprintf(stderr, "Failed to allocate memory [almondCertificate].\n");
		writeLog("Failed to allocate memory [almondCertificate]", 2, 1);
		config_memalloc_fails++;
		return;
	}
	strncpy(almondCertificate, value.strval, strlen(value.strval));
	almondCertificate[strlen(value.strval)] = '\0';
	writeLog("Almond certificate provided if TLS for API is enabled.", 0, 1);
}
#endif

/* process_almond_key moved to config.c */
#if 0
void process_almond_key(ConfVal value) {
	almondKey = malloc((size_t)strlen(value.strval)+1);
	if (almondKey == NULL) {
		fprintf(stderr, "Failed to allocate memory [almondSSLKey].\n");
		writeLog("Failed to allocate memory [almondSSLKey]", 2, 1);
		config_memalloc_fails++;
		return;
	}
	strncpy(almondKey, value.strval, strlen(value.strval));
	almondKey[strlen(value.strval)] = '\0';
	writeLog("Almond certificate key provided to be used by API to run with  SSL encryption.", 0, 1);
}
#endif

/* process_almond_port moved to config.c */
#if 0
void process_almond_port(ConfVal value) {
	if (value.intval >= 1) {
        	local_port = value.intval;
	}
	else local_port = ALMOND_API_PORT;
	if (local_api) {
        	writeLog("Almond will enable local api.", 0, 1);
        }
}
#endif

/* process_almond_standalone moved to config.c */
#if 0
void process_almond_standalone(ConfVal value) {
	if ((strcmp(value.strval, "true") == 0) || (value.intval >= 1)) {
		writeLog("Almond will run standalone. No monitor data will be sent to HowRU.", 0, 1);
		standalone = true;
	}
}
#endif

/* process_almond_api_tls moved to config.c */
#if 0
#if 0
void process_almond_api_tls(ConfVal value) {
	if ((strcmp(value.strval, "true") == 0) || (value.intval >= 1)) {
		writeLog("Almond scheduler use TLS encryption.", 0, 1);
		use_ssl = true;
	}
}
#endif
#endif

void process_almond_format(ConfVal value) {
	if (strcmp(value.strval, "json") == 0){
		printf ("Export to json\n");
		writeLog("Export to format 'json'.", 0, 1);
		output_type= JSON_OUTPUT;
	}
	else if (strcmp(value.strval, "metrics") == 0) {
		printf ("Export to metrics file\n");
		writeLog("Export to standard metrics.", 0, 1);
		output_type = METRICS_OUTPUT;
	}
	else if (strcmp(value.strval, "jsonmetrics") == 0) {
		printf ("Export both to json and metrics file.\n");
		writeLog("Exporting both to json and to metrics file.", 0, 1);
		output_type = JSON_AND_METRICS_OUTPUT;
	}
	else if (strcmp(value.strval, "prometheus") == 0) {
		printf("Export to prometheus.\n");
		writeLog("Export to prometheus style metrics.", 0, 1);
		output_type = PROMETHEUS_OUTPUT;
	}
	else if (strcmp(value.strval, "jsonprometheus") == 0) {
		printf("Export to both json and Prometheus style metrics.\n");
		writeLog("Exporting to both json and prometheus style metrics.", 0, 1);
		output_type = JSON_AND_PROMETHEUS_OUTPUT;
	}
	else {
		printf("%s is not a valid value.  supported at this moment.\n", value.strval);
		writeLog("Unsupported value in configuration scheduler.format.", 1, 1);
		writeLog("Using standard output (JSON_OUTPUT).", 0, 1);
		output_type = JSON_OUTPUT;
	}
}

void process_conf_dir(ConfVal value) {
	if (confDir == NULL) {
		confDir = malloc((size_t)50 * sizeof(char));
		if (!confDir) {
			writeLog("Failed to allocate memory.", 1, 1);
			return;
		}
	}
	if (confDir != NULL)
        	memset(confDir, '\0', 50 * sizeof(char));
	if (directoryExists(value.strval, 255) == 0) {
        	//strncpy(confDir, value.strval, strlen(value.strval));
		//confDir[strlen(value.strval)] = '\0';
		snprintf(confDir, 50, "%s", value.strval);
        	confDirSet = true;
	}
        else {
        	int status = mkdir(value.strval, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        	if(status != 0 && errno != EEXIST){
        		printf("Failed to create directory. Errno: %d\n", errno);
        		writeLog("Error creating configuration directory.", 2, 1);
        	}
        	else {
        		//strncpy(confDir, value.strval, strlen(value.strval));
			//confDir[strlen(value.strval)] = '\0';
			snprintf(confDir, 50, "%s", value.strval);
        		confDirSet = true;
        	}
        }
        writeLog("Configuration directory is set.", 0, 1);
}

void process_almond_quickstart(ConfVal value) {
	if ((strcmp(value.strval, "true") == 0) || (value.intval >= 1)) {
		writeLog("Almond scheduler have quick start activated.", 0, 1);
		quick_start = true;
	}
}

void process_init_sleep(ConfVal value) {
	int i = strtol(value.strval, NULL, 0);
	if (i < 2000)
		i = 6000;
	initSleep = i;
	writeLog("Init sleep for scheduler read.", 0, 1);
}

void process_almond_scheduler_type(ConfVal value) {
	if (strcmp(value.strval, "time") == 0){
		timeScheduler = true;
		writeLog("Almond will use a time scheduler.", 0, 1);
	}
	else {
		writeLog("Almond will use classic scheduler.", 0, 1);
	}
}

void process_almond_sleep(ConfVal value) {
	int i = strtol(value.strval, NULL, 0);
	if (i < 1000)
		i = 1000;
	snprintf(infostr, infostr_size, "Scheduler sleep time is %d ms.", i);
	writeLog(trim(infostr), 0, 1);
	schedulerSleep = i;
}

void process_data_dir(ConfVal value) {
	if (directoryExists(value.strval, 255) == 0) {
		//strncpy(dataDir, value.strval, strlen(value.strval));
		//dataDir[strlen(value.strval)] = '\0';
		snprintf(dataDir, datadir_size, "%s", value.strval);
		dataDirSet = true;
	}
	else {
		int status = mkdir(value.strval, 0755);
		if (status != 0 && errno != EEXIST) {
			printf("Failed to create directory. Errno: %d\n", errno);
			writeLog("Error creating Almond data directory.", 2, 1);
			return;
		}
		else {
			//strncpy(dataDir, value.strval, strlen(value.strval));
			//dataDir[strlen(value.strval)] = '\0';
			snprintf(dataDir, datadir_size, "%s", value.strval);
			dataDirSet = true;
		}
	}
	snprintf(infostr, infostr_size, "Almond data dir is set to %s.", dataDir);
	writeLog(infostr, 0, 1);
}

void process_store_dir(ConfVal value) {
	if (directoryExists(value.strval, 255) == 0) {
		strncpy(storeDir, value.strval, storedir_size);
                storeDirSet = true;
        }
        else {
        	int status = mkdir(value.strval, 0755);
		if (status != 0 && errno != EEXIST) {
                	printf("Failed to create directory. Errno: %d\n", errno);
                        writeLog("Error creating Almond store directory.", 2, 1);
			return;
                }
                else {
                	//strncpy(storeDir, value.strval, strlen(value.strval));
			//storeDir[strlen(value.strval)] = '\0';
			snprintf(storeDir, storedir_size, "%s", value.strval);
                        storeDirSet = true;
                }
	}
	snprintf(infostr, infostr_size, "Almond store dir is set to %s.", storeDir);
        writeLog(infostr, 0, 1);
}

void process_truncate_log(ConfVal value) {
        if ((strcmp(value.strval, "true") == 0) || (value.intval >= 1)) {
                writeLog("Almond will truncate it logs..", 0, 1);
                truncateLog = true;
        }
}

void process_external_scheduler(ConfVal value) {
	if ((strcmp(value.strval, "true") == 0) || (value.intval >= 1)) {
		writeLog("Almond is set to use external scheduler.", 0, 1);
		writeLog("Almond will after initialization only respond to api calls to execute commands.", 1, 1);
		external_scheduler = true;
		writeLog("Almond scheduler is inactivated for running command checks.", 0, 1);
	}
}

void process_use_kafka_config(ConfVal value) {
	if ((strcmp(value.strval, "true") == 0) || (value.intval >= 1)) {
		writeLog("Almond will use '/etc/almond/kafka.conf' for Kafka configurations.", 0, 1);
		useKafkaConfigFile = true;
	}
}

void process_truncate_log_interval(ConfVal value) {
	int i = strtol(value.strval, NULL, 0);
	if (i < 3600) {
		writeLog("Truncate log interval configuration value too low. Minumum value is 3600.", 1, 1);
		writeLog("Truncate log interval value will not be changed.", 0, 1);
	}
	else if (i > 2147483647) {
		writeLog("Truncate log interval configuration value too high. Maximum value is 2147483647.", 1, 1);
		writeLog("Truncate log interval value will not be changed.", 0, 1);
	}
	else {
		truncateLogInterval = i;
		writeLog("Truncate log interval value updated from configuration.", 0, 1);
	}
}

void process_log_to_stdout(ConfVal val) {
	if ((strcmp(val.strval, "true") == 0) || (val.intval > 0)) {
		dockerLog = true;
		writeLog("Log to stdout is set. Mostly useful for containers this option.", 0, 1);
		writeLog("DEBUG: docker log should be enabled, writing to stdout. TODO: enabled in code.", 1, 1);
	}
}

void process_log_dir(ConfVal val) {
	if (directoryExists(val.strval, 255) == 0) {
		//strncpy(logDir, val.strval, strlen(val.strval));
		//logDir[strlen(val.strval)] = '\0';
		snprintf(logDir, logdir_size, "%s", val.strval);
                logDirSet = true;
        }
        else {
        	int status = mkdir(val.strval, 0755);
		if (status != 0 && errno != EEXIST) {
                	printf("Failed to create directory. Errno: %d\n", errno);
                        writeLog("Error creating log directory.", 2, 1);
                }
                else {
                	//strncpy(logDir, val.strval, strlen(val.strval));
			//logDir[strlen(val.strval)] = '\0';
			snprintf(logDir, logdir_size, "%s", val.strval);
                        logDirSet = true;
                }
	}
	if (strcmp(val.strval, "/var/log/almond") != 0) {
		char ch =  '/';
                FILE *logFile;
                /*strcpy(fileName, logDir);
                strncat(fileName, &ch, 1);
                strcat(fileName, "almond.log");*/
		snprintf(fileName, filename_size, "%s/%s", logDir, "almond.log");
                writeLog("Closing logfile...", 0, 1);
                fclose(fptr);
                fptr = NULL;
                sleep(0.2);
                logFile = fopen("/var/log/almond/almond.log", "r");
                fptr = fopen(fileName, "a");
                if (fptr == NULL) {
                	fclose(logFile);
                        logFile = NULL;
                        fptr = fopen("/var/log/almond/almond.log", "a");
                        writeLog("Could not create new logfile.", 1, 1);
                        writeLog("Reopened logfile '/var/log/almond/almond.log'.", 0, 1);
                        strcpy(logfile, "/var/log/almond/almond.log");
                }
                else {
			while ( (ch = fgetc(logFile)) != EOF)
                        	fputc(ch, fptr);
                        fclose(logFile);
                        logFile = NULL;
                        writeLog("Created new logfile.", 0, 1);
                        strcpy(logfile, fileName);
		
		}
	}
       	else {
       		strcpy(logfile, "/var/log/almond/almond.log");
       }
}

void process_log_plugin_output(ConfVal value) {
	if ((strcmp(value.strval, "true") == 0) || (value.intval > 0)) {
		writeLog("Plugin outputs will be written to the log file", 0, 1);
		logPluginOutput = true;
	}
        else {
        	writeLog("Plugin outputs will not be written to the log file", 0, 1);
        }
}

void process_store_results(ConfVal value) {
	if ((strcmp(value.strval, "true") == 0) || (value.intval > 0)) {
	        writeLog("Plugin results will be stored in csv file.", 0, 1);
                pluginResultToFile = true;
        }
        else {
                writeLog("Plugin results is not stored in specific csv file.", 0, 1);
        }
}

void process_host_name(ConfVal value) {
	/*strncpy(hostName, value.strval, strlen(value.strval));
	hostName[strlen(value.strval)] = '\0';*/
	snprintf(hostName, hostname_size, "%s", value.strval);
	snprintf(infostr, infostr_size, "Scheduler will give this host the virtual name: %s", hostName);
	writeLog(trim(infostr), 0, 1);
}

void process_plugin_directory(ConfVal value) {
	if (directoryExists(value.strval, 255) == 0) {
       		//strncpy(pluginDir, value.strval, strlen(value.strval));
		//pluginDir[strlen(value.strval)-1] = '\0';
		snprintf(pluginDir, plugindir_size, "%s", value.strval);
                pluginDirSet = true;
        }
        else {
        	int status = mkdir(value.strval, 0755);
                if (status != 0 && errno != EEXIST) {
                	printf("Failed to create directory. Errno: %d\n", errno);
                        writeLog("Error creating plugins directory.", 2, 1);
                }
                else {
			//strncpy(pluginDir, value.strval, strlen(value.strval));
			//pluginDir[strlen(value.strval)-1] = '\0';
			snprintf(pluginDir, plugindir_size, "%s", value.strval);
			pluginDirSet = true;
			writeLog("Created new plugin directory. It most likely is empty!", 1, 1);
                }
        }
}

void process_plugin_declaration(ConfVal v) {
	if (access(v.strval, F_OK) == 0){
		/*strncpy(pluginDeclarationFile, v.strval, strlen(v.strval));
		pluginDeclarationFile[strlen(v.strval)] = '\0';*/
		//strlcpy(pluginDeclarationFile, v.strval, sizeof(pluginDeclarationFile);
		snprintf(pluginDeclarationFile, plugindeclarationfile_size, "%s", v.strval);
        }
        else {
        	printf("ERROR: Plugin declaration file does not exist.");
        	writeLog("Plugin declaration file does not exist.", 2, 1);
		config_memalloc_fails++;
		return;
	}
	snprintf(infostr, infostr_size, "Plugin g_plugins file is set to '%s'.", pluginDeclarationFile);
	writeLog(trim(infostr), 0, 1);
}

void process_enable_gardener(ConfVal value) {
	if ((strcmp(value.strval, "true") == 0) || (value.intval > 0)) {
		writeLog("Gardener script is enabled.", 0, 1);
                enableGardener = true;
	}
	else {
		writeLog("Gardener script is not enabled.", 0, 1);
	}
}

void process_enable_kafka_export(ConfVal v) {
	if ((strcmp(v.strval, "true") == 0) || (v.intval > 0)) {
		writeLog("Exporting results to Kafka is enabled.", 0, 1);
                enableKafkaExport = true;
	}
	else {
                writeLog("Export to Kafka is not enabled.", 0, 1);
	}
}

void process_enable_kafka_tags(ConfVal v){
	if ((strcmp(v.strval, "true") == 0) || (v.intval > 0)) {
		writeLog("Use of tag to Kafka message is enabled.", 0, 1);
                enableKafkaTag = true;
	}
	else {
		writeLog("Use of tag to Kafka message is not enabled.", 0, 1);
	}
}

void process_enable_kafka_id(ConfVal v) {
	if ((strcmp(v.strval, "true") == 0) || (v.intval > 0)) {
		writeLog("Use of Kafka id is enabled.", 0, 1);
                enableKafkaId = true;
	}
	else {
		writeLog("Use of Kafka id is not enabled.", 0, 1);
       }
}

void process_kafka_start_id(ConfVal val) {
	int i = strtol(val.strval, NULL, 0);
        if (i > 0) {
        	kafka_start_id = i;
        	writeLog("Kafka start id check ok", 0, 1);
        }
        else {
        	writeLog("Could not read kafka_start_id.", 1, 1);
        	kafka_start_id = 0;
        }
}

void process_kafka_brokers(ConfVal value) {
	kafkaexportreqs++;
	size_t kf_len = strlen(value.strval) + 1;
	kafka_brokers = malloc(kf_len);
	if (kafka_brokers == NULL) {
		fprintf(stderr, "Failed to allocate memory for kafka brokers.\n");
		writeLog("Failed to allocate memory [kafka_brokers]", 2, 1);
		config_memalloc_fails++;
		return;
	}
	else
		memset(kafka_brokers, '\0', (size_t)(strlen(value.strval)+1) * sizeof(char));
	//strncpy(kafka_brokers, value.strval, strlen(value.strval));
	snprintf(kafka_brokers, kf_len, "%s", value.strval);
	snprintf(infostr, infostr_size, "Kafka export brokers is set to '%s'", kafka_brokers);
	writeLog(trim(infostr), 0, 1);
}

void process_kafka_config_file(ConfVal value) {
	size_t cf_len = strlen(value.strval) + 1;
	kafkaConfigFile = malloc(cf_len);
	if (kafkaConfigFile == NULL) {
		fprintf(stderr, "Failed to allocate memory for kafka config file.\n");
                writeLog("Failed to allocate memory [kafka_config_file]", 2, 1);
                config_memalloc_fails++;
                return;
	}
	snprintf(kafkaConfigFile, cf_len, "%s", value.strval);
	snprintf(infostr, infostr_size, "Kafka config file is set to '%s'", kafkaConfigFile);
	writeLog(trim(infostr), 0, 1);
}

#if 0
void process_kafka_topic(ConfVal val) {
	kafkaexportreqs++;
	size_t l = strlen(val.strval) + 1;
	/* allocate once (avoid double allocation/leak) */
	char *tmp = malloc(l);
	if (tmp == NULL) {
		fprintf(stderr, "Failed to allocate memory [kafka_topic].\n");
		writeLog("Failed to allocate memory [kafka_topic]", 2, 1);
		config_memalloc_fails++;
		return;
	}
	memcpy(tmp, val.strval, l);
	free(kafka_topic);
	kafka_topic = tmp;
        snprintf(infostr, infostr_size, "Kafka export topic is set to '%s'", kafka_topic);
        writeLog(trim(infostr), 0, 1);
}
#endif

void process_kafka_tag(ConfVal value) {
	/* allocate via strdup and free previous value to avoid leaks */
	char *tmp = strdup(value.strval);
	if (tmp == NULL) {
		fprintf(stderr, "Failed to allocate memory [kafka_tag].\n");
		writeLog("Failed to allocate memory [kafka_tag]", 2, 1);
		config_memalloc_fails++;
		return;
	}
	free(kafka_tag);
	kafka_tag = tmp;
	snprintf(infostr, infostr_size, "Kafka tag is set to '%s'", kafka_tag);
	writeLog(trim(infostr), 0, 1);
}

void process_enable_kafka_ssl(ConfVal value) {
	if ((strcmp(value.strval, "true") == 0) || (value.intval > 0)) {
		writeLog("Kafka producer will connect to cluster with SSL.", 0, 1);
                writeLog("Make sure you use a certificate with accordance to Kafka ACL list.", 0, 1);
                enableKafkaSSL = true;
	}
	else {
		writeLog("Kafka producer will connect with plain text", 0, 1);
	}
}

void process_kafka_ca_certificate(ConfVal val) {
	kafkaCACertificate = malloc((size_t)strlen(val.strval)+1);
	if (kafkaCACertificate == NULL) {
		fprintf(stderr, "Failed to allocate memory [kafkaCACertificate].\n");
		writeLog("Failed to allocate memory [kafkaCACertificate]", 2, 1);
		config_memalloc_fails++;
		return;
	}
	strncpy(kafkaCACertificate, val.strval, strlen(val.strval));
	kafkaCACertificate[strlen(val.strval)] = '\0';
	writeLog("Kafka CA certificate location stored from configuration file.", 0, 1);
}

void process_kafka_producer_certificate(ConfVal value) {
	kafkaProducerCertificate = malloc((size_t)strlen(value.strval)+1);
	if (kafkaProducerCertificate == NULL) {
		fprintf(stderr, "Failed to allocate memory [kafkaProducerCertificate].\n");
		writeLog("Failed to allocate memory [kafkaProducerPertificate", 2, 1);
		config_memalloc_fails++;
		return;
	}
	strncpy(kafkaProducerCertificate, value.strval, strlen(value.strval));
	kafkaProducerCertificate[strlen(value.strval)] = '\0';
	writeLog("Kafka Producer certificate location stored from configuration file.", 0, 1);
}

void process_kafka_ssl_key(ConfVal val) {
	kafkaSSLKey = malloc((size_t)strlen(val.strval)+1);
	if (kafkaSSLKey == NULL) {
		fprintf(stderr, "Failed to allocate memory [kafkaSSLKey].\n");
		writeLog("Failed to allocate memory [kafkaSSLKey]", 2, 1);
		config_memalloc_fails++;
		return;
	}
	strncpy(kafkaSSLKey, val.strval, strlen(val.strval));
	kafkaSSLKey[strlen(val.strval)] = '\0';
	writeLog("Kafka SSL Key provided from configuration file.", 0, 1);
}

void process_schema_name(ConfVal val) {
	if (val.strval == NULL) {
		fprintf(stderr, "Schema registry name is NULL in config.\n");
                writeLog("Schema registry name is NULL in configuration file.", 1, 1);
		return;
	}
	if (strlen(val.strval) > 100) {
		writeLog("Schema registry name is too long. Should be maximum 100 characters.", 1, 1);
		return;
	}
        strncpy(schemaName, val.strval, sizeof(schemaName)-1);
	schemaName[sizeof(schemaName)-1] = '\0';
    	snprintf(infostr, infostr_size, "Kafka schema name is set to '%s'", schemaName);
        writeLog(trim(infostr), 0, 1);
}

void process_schema_registry_url(ConfVal val) {
        if (val.strval == NULL) {
        	fprintf(stderr, "Schema registry URL is NULL\n");
    		writeLog("Schema registry URL is NULL", 2, 1);
    		config_memalloc_fails++;
    		return;
  	}
	size_t len = strlen(val.strval);
        schemaRegistryUrl = malloc(len+1);
        if (schemaRegistryUrl == NULL) {
                fprintf(stderr, "Failed to allocate memory [kafka_schemaRegistryUrl].\n");
                writeLog("Failed to allocate memory [kafka_schemaRegistryUrl]", 2, 1);
                config_memalloc_fails++;
                return;
        }
        strncpy(schemaRegistryUrl, val.strval, len);
	schemaRegistryUrl[len] = '\0';
        snprintf(infostr, infostr_size, "Kafka schema registry url is set to '%s'", schemaRegistryUrl);
        writeLog(trim(infostr), 0, 1);
}

void process_gardener_run_interval(ConfVal value) {
	int i = strtol(value.strval, NULL, 0);
	if (i < 60)
		i = 43200;
	snprintf(infostr, infostr_size, "Gardener run interval is %d seconds.", i);
        writeLog(trim(infostr), 0, 1);
        gardenerInterval = i;
}

void process_clear_data_cache_interval(ConfVal v) {
	int i = strtol(v.strval, NULL, 0);
	if (i < 60)
		i = 300;
	snprintf(infostr, infostr_size, "Clear data cache is %d seconds.", i);
	writeLog(trim(infostr), 0, 1);
	clearDataCacheInterval = i;
}

void process_data_cache_time_frame(ConfVal val) {
	int i = strtol(val.strval, NULL, 0);
	if (i < 180)
		i = 330;
	snprintf(infostr, infostr_size, "Data cache time frame is set to %d seconds.", i);
	writeLog(trim(infostr), 0, 1);
	dataCacheTimeFrame = i;
}

void process_tune_timer(ConfVal value) {
	if ((strcmp(value.strval, "true") == 0) || (value.intval > 0)) {
		writeLog("Timer tuner is enabled.", 0, 1);
                enableTimeTuner = true;
	}
	else {
		writeLog("Timer tuner is not enabled.", 0, 1);
	}
}

void process_tune_cycle(ConfVal val) {
	int i = strtol(val.strval, NULL, 15);
	snprintf(infostr, infostr_size, "Time tuner cycle is set to %d.", i);
	writeLog(trim(infostr), 0, 1);
	timeTunerCycle = i;
}

void process_tune_master(ConfVal value) {
	int i = strtol(value.strval, NULL, 1);
	snprintf(infostr, infostr_size, "Time tuner cycle is set to %d.", i);
	writeLog(trim(infostr), 0, 1);
	timeTunerMaster = i;
}

void process_run_gardener_at_start(ConfVal v) {
	if ((strcmp(v.strval, "true") == 0) || (v.intval > 0)) {
		writeLog("Gardener will run during startup.", 0, 1);
                runGardenerAtStart = true;
        }
}

void process_gardener_script(ConfVal value) {
	if (access(value.strval, F_OK) == 0){
		strncpy(gardenerScript, value.strval, gardenerscript_size);
		//gardenerScript[strlen(value.strval)] = '\0';
		gardenerScript[gardenerscript_size] = '\0';
	}
	else {
		enableGardener = false;
		writeLog("Gardener script file could not be found", 1, 1);
		writeLog("Gardener is disabled.", 2, 1);
	}
}

void process_enable_clear_data_cache(ConfVal value) {
	if ((strcmp(value.strval, "true") == 0) || (value.intval > 0)) {
		writeLog("Clear data cache is enabled.", 0, 1);
                enableClearDataCache = true;
        }
        else {
                writeLog("Clear data cache is not enabled.", 0, 1);
        }
}

/* process_json_file moved to config.c */
#if 0
void process_json_file(ConfVal value) {
	//strncpy(jsonFileName, value.strval, strlen(value.strval));
	//jsonFileName[strlen(value.strval)] = '\0';
	snprintf(jsonFileName, jsonfilename_size, "%s", value.strval);
	snprintf(infostr, infostr_size, "Json data will be collected in file: %s.", jsonFileName);
	writeLog(trim(infostr), 0, 1);
}
#endif

/* process_metrics_file moved to config.c */
#if 0
void process_metrics_file(ConfVal val) {
	/*strncpy(metricsFileName, val.strval, strlen(val.strval));
        metricsFileName[strlen(val.strval)] = '\0';*/
	snprintf(metricsFileName, metricsfilename_size, "%s", val.strval);
	snprintf(infostr, infostr_size, "Metrics will be collected in file: %s", metricsFileName);
	writeLog(trim(infostr), 0, 1);
}
#endif

void process_metrics_output_prefix(ConfVal value) {
	if ((int)strlen(value.strval) <= 30) {
		//size_t len = strlen(value.strval);
		//strncpy(metricsOutputPrefix, value.strval, len);
		//metricsOutputPrefix[len] = '\0';
		snprintf(metricsOutputPrefix, 31, "%s", value.strval);
		snprintf(infostr, infostr_size, "Metrics output prefix is set to '%s'", metricsOutputPrefix);
		writeLog(trim(infostr), 0, 1);
	}
	else {
		writeLog("Could not change metricsOutputPrefix. Prefix too long.", 1, 1);
	}
}

void process_save_on_exit(ConfVal value) {
	if ((strcmp(value.strval, "true") == 0) || (value.intval > 0)) {
		writeLog("Data file will be saved in data directory after shutdown.", 0, 1);
		saveOnExit = true;
	}
	else {
		writeLog("Json data will be deleted on shutdown.", 0, 1);
	}
}

int getConfigurationValues() {
	char* file_name = NULL;
        char* line = NULL;
        size_t len = 0;
        ssize_t read;
        FILE *fp = NULL;
        int index = 0;
        file_name = "/etc/almond/almond.conf";
        fp = fopen(file_name, "r");
        char confName[MAX_STRING_SIZE] = "";
        char confValue[MAX_STRING_SIZE] = "";
	
	if (fp == NULL)
        {
                perror("Error while opening the configuration file.\n");
                writeLog("Error opening configuration file", 2, 1);
                exit(EXIT_FAILURE);
        }

	while ((read = getline(&line, &len, fp)) != -1) {
		char *trimmed = trim(line);
		if (trimmed[0] == '#' || trimmed[0] == '\0') {
			continue;
		}
		char * token = strtok(trimmed, "=");
		while (token != NULL) {
			if (index == 0) {
				//strncpy(confName, token, sizeof(confName));
				snprintf(confName, sizeof(confName), "%s", token);
                   	}
                   	else {
				//strncpy(confValue, token, sizeof(confValue));
				snprintf(confValue, sizeof(confValue), "%s", token);
                   	}
                   	token = strtok(NULL, "=");
                   	index++;
                   	if (index == 2) index = 0;
           	}
		ConfVal cvu;
		cvu.intval = strtol(trim(confValue), NULL, 0);
		cvu.strval = trim(confValue);
		for (int i = 0; i < sizeof(config_entries)/sizeof(ConfigEntry);i++) {
			if (strcmp(confName, config_entries[i].name) == 0) {
				config_entries[i].process(cvu);
				break;
			}
		}
	}
	updateInterval = 60;
	if (enableKafkaExport) {
       		if (kafkaexportreqs < 2 && !useKafkaConfigFile) {
                	writeLog("Not sufficient configuration to export to Kafka. Brokers and or topic is unknown.", 1, 1);
                	writeLog("Kafka export is not enabled.", 0, 1);
                	enableKafkaExport = false;
		}
        }
	// Also check Almond SSL like Kafka
        fclose(fp);
        fp = NULL;
        if (line){
                free(line);
                line = NULL;
        }
	if (config_memalloc_fails > 0) {
		config_memalloc_fails = 0;
		return 2;
	}
        return 0;
}

int truncateLogs() {
	size_t compressed_name_size = logfile_size + 28;
	char* compressed_name = malloc(compressed_name_size * sizeof(char));
	strcpy(compressed_name, logfile);
	strncat(compressed_name, getCurrentTimestamp(), 20);
	strncat(compressed_name, ".tar.gz", 8);
	if (compress_log(logfile, compressed_name) == -1) {
		return -1;
	}
	if (truncate(logfile, 0) == -1) {
		fprintf(stderr, "Failed to truncate log: %s\n", strerror(errno));
		writeLog("Truncation of log file failed.", 1, 1); 
		unlink(compressed_name);
		return -1;
	}
	return 0;
}

int check_file_truncation() {
	struct stat filestat;
	time_t current_time, diff_seconds;

	if (stat(logfile, &filestat) == -1) {
		writeLog("Failed to get filestat from logfile. This will make truncation impossible", 1, 1);
		return 0;
	}
	current_time = time(NULL);
	#ifdef HAS_BIRTHTIME
		diff_seconds = current_time - filestat.st_birthtime;
	#else
		diff_seconds = current_time - filestat.st_mtime;
		writeLog("Could not get birthtime from file. Truncation will be omitted.", 1, 1);
	#endif
	if (diff_seconds > truncateLogInterval) {
		printf("Will start truncating the Almond log.");
		writeLog("It is time to truncate the Almond log.", 0, 1);
		sleep(1);
		truncateLogs();
	}
	return diff_seconds;
}

/* apiDryRun moved to api.c */

#if 0
/* apiRunPlugin moved to api.c */
void apiRunPlugin(int plugin_id, int flags) {
	char* pluginName = NULL;
	char* message = NULL;
	int waitCount = 0;

	message = (char *) malloc(sizeof(char) * (apimessage_size+1));
	if (message == NULL) {
		writeLog("Failed to allocate memory for api message", 1, 0);
		return;
	}
	else
		memset(message, '\0', (size_t)(apimessage_size+1) * sizeof(char));
	pluginName = malloc((size_t)(pluginitemname_size + 1) * sizeof(char));
	if (pluginName == NULL) {
		fprintf(stderr, "Failed to allocate memory in apiRunPlugin.\n");
                writeLog("Failed to allocate memory [apiRunPlugin: pluginName]", 2, 0);
                return;
        }
	else
		memset(pluginName, '\0', (size_t)(pluginitemname_size+1) * sizeof(char));
	// In new structure increase id with one
	//plugin_id++;
	pluginName = strdup(g_plugins[plugin_id]->name);
        removeChar(pluginName, '[');
        removeChar(pluginName, ']');
	// Check if same plugin is running in thread, in which case wait...
	while (threadIds[(short)plugin_id] > 0) {
		writeLog("Waiting for thread to finish...", 0, 0);
		sleep(1);
		waitCount++;
		if (waitCount > 10) {
			writeLog("Reached waitCount threshold. Continue.", 1, 0);
			break;
		}
	}
	char p_id[12];
	snprintf(p_id, sizeof(p_id), "%i",plugin_id);
	setApiCmdFile("execute", p_id);
	strcpy(message, "{\n     \"executePlugin\":\"");
	strcat(message, pluginName);
	strcat(message, "\"");
	if (flags == API_FLAGS_VERBOSE) {
		strcat(message, ",\n");
		sleep(10);
		strcat(message, "     \"pluginOutput:\":\"");
		strcat(message, trim(g_plugins[plugin_id]->output.retString));
		strcat(message, "\"");
        }
	strcat(message, "\n}\n");
	socket_message = malloc((size_t)(apimessage_size + 1) * sizeof(char));
	if (socket_message == NULL) {
		fprintf(stderr, "Failed to allocate memory in apiRunPlugin.\n");
                writeLog("Failed to allocate memory [apiRunPlugin: socket_message]", 2, 0);
                return;
        }
	else
		memset(socket_message, '\0', (size_t)(apimessage_size + 1) * sizeof(char));
	if (strlen(message) > apimessage_size) {
		printf("DEBUG: [apiRunPlugin] Message is larger than size.\n");
		message[apimessage_size-1] = '\0';
	}
	strncpy(socket_message, message, (size_t)apimessage_size);
	free(pluginName);
	pluginName = NULL;
	if (message != NULL) {
		free(message);
		message = NULL;
	}	
}
#endif

#if 0
void apiReadData(int plugin_id, int flags) {
	char* pluginName = NULL;
	char rCode[12];
	char* message = NULL;
	unsigned short is_error = 0;

	if (plugin_id < 0) {
		printf("Strange things happen...\n");
		return;
	}

	message = malloc((size_t)apimessage_size * sizeof(char)+1);
	if (message == NULL) {
		writeLog("Failed to allocate memory for api message.", 1, 0);
	}
	else
       		message[0] = '\0';
	pluginName = malloc((size_t)pluginitemname_size * sizeof(char)+1);
	if (pluginName == NULL) {
		fprintf(stderr, "Failed to allocate memory in apiReadData.\n");
		writeLog("Failed to allocate memory [apiReadData:pluginName]", 2, 0);
		return;
	}
	if (plugin_id == 0 && flags == 0) {
		printf("This is an invalid check.\n");
		is_error++;
	}
	if (plugin_id > decCount || flags > 100) {
		printf("This is an invalid check.\n");
		is_error++;
	}	
	if (is_error > 0) {
		strcat(message, "{\n     \"almond\":\"Invalid check - no such plugin or flag\"\n}\n");
		socket_message = malloc((size_t)(apimessage_size + 1) * sizeof(char));
                if (socket_message == NULL) {
                        fprintf(stderr, "Failed to allocate memory.\n");
                        writeLog("Failed to allocate memory in [apiReadData:socket_message]", 2, 0);
                        return;
                }
		else
			memset(socket_message, '\0', (size_t)(apimessage_size + 1) * sizeof(char));
                strcpy(socket_message, message);
                free(message);
                free(pluginName);
                message = pluginName = NULL;
		return;
	}
	// In new structure I need to increase id with 1
	//plugin_id += 1;
	pluginName = strdup(g_plugins[plugin_id]->name);
        removeChar(pluginName, '[');
        removeChar(pluginName, ']');
	if (flags == API_FLAGS_VERBOSE) {
		strcat(message,"{\n     \"name\":\"");
        	strcat(message, pluginName);
        	strcat(message, "\",\n");
		strcat(message, "     \"description\":\"");
	        strcat(message, g_plugins[plugin_id]->description);
		strcat(message, "\",\n");
		switch (g_plugins[plugin_id]->output.retCode) {
			case 0:
				strcat(message, "     \"pluginStatus\":\"OK\",\n");
				break;
			case 1:
				strcat(message, "     \"pluginStatus\":\"WARNING\",\n");	
				break;
			case 2: 
				strcat(message, "     \"pluginStatus\":\"CRITICAL\",\n");
				break;
			default:
				strcat(message, "     \"pluginStatus\":\"UNKNOWN\",\n");
				break;
		}
		strcat(message, "     \"pluginStatusCode\":\"");
		sprintf(rCode, "%d", g_plugins[plugin_id]->output.retCode); 
	   	strcat(message, trim(rCode));
		strcat(message,  "\",\n");
		strcat(message, "     \"pluginOutput\":\"");
		strcat(message, trim(g_plugins[plugin_id]->output.retString));
		strcat(message, "\",\n");
		strcat(message, "     \"pluginStatusChanged\":\"");
		strcat(message, g_plugins[plugin_id]->statusChanged);
		strcat(message, "\",\n");
		strcat(message, "     \"lastChange\":\"");
		strcat(message, g_plugins[plugin_id]->lastChangeTimestamp);
		strcat(message, "\",\n");
		strcat(message, "     \"lastRun\":\"");
		strcat(message, g_plugins[plugin_id]->lastRunTimestamp);
		strcat(message, "\",\n");
                strcat(message, "     \"nextScheduledRun\":\"");
		strcat(message, g_plugins[plugin_id]->nextRunTimestamp);
		strcat(message, "\"\n");
	}
        else {
		strcat(message,"{\n     \"");
                strcat(message, pluginName);
                strcat(message, "\":\"");
                strcat(message, trim(g_plugins[plugin_id]->output.retString));
                strcat(message, "\"\n");
	}
	strcat(message, "}\n");
	free(pluginName);
	pluginName = NULL;
	socket_message = malloc((size_t)(apimessage_size + 1) * sizeof(char));
	if (socket_message == NULL) {
		fprintf(stderr, "Failed to allocate memory.\n");
		writeLog("Failed to allocate memory in [apiReadData:socket_message]", 2, 0);
		return;
	}
	else
		memset(socket_message, '\0', (size_t)(apimessage_size + 1) * sizeof(char));
	strncpy(socket_message, message, (size_t)apimessage_size);
	free(message);
	message = NULL;
}
#endif

void __deprecated_createUpdateFile(struct PluginItem *item, struct PluginOutput *output, char name[3]) {
	FILE *fp = NULL;
	char filename[30];
       	
	strcpy(filename, "/opt/almond/api_cmd/");
	strncat(filename, name, 3);
	strncat(filename, ".udf", 5);
	filename[strlen(filename)] = '\0';
	fp = fopen(filename, "w");
	fprintf(fp, "item_id\t%s\n", name);
	fprintf(fp, "item_lastruntimestamp\t%s\n", item->lastRunTimestamp);
	fprintf(fp, "item_nextruntimestamp\t%s\n", item->nextRunTimestamp);
	fprintf(fp, "item_lastchangetimestamp\t%s\n", item->lastChangeTimestamp);
	fprintf(fp, "item_statuschanged\t%s\n", item->statusChanged);
	fprintf(fp, "item_nextrun\t");
	//fwrite(&item->nextRun, sizeof(time_t), 1, fp);
	fprintf(fp, "\noutput_retcode\t%i\n", output->retCode);
	fprintf(fp, "output_retstring\t%s\n", output->retString);
	fclose(fp);
	fp = NULL;
}

/* apiGetMetrics moved to api.c */
#if 0
	char* pluginName = NULL;
	char rCode[12];
	char strNum[12];
        char* message = NULL;
	unsigned short is_error = 0;
        
	message = malloc((size_t)apimessage_size+1 * sizeof(char));
	if (message == NULL) {
		writeLog("Could not allocate memory for apimessage", 2, 0);
		return;
	}
	else {
		memset(message, '\0', (size_t)apimessage_size+1 * sizeof(char));
	}
	if (plugin_id == 0 && flags == 0) {
                printf("This is an invalid check.\n");
                is_error++;
        }
        if (plugin_id > decCount || flags > 100) {
                printf("This is an invalid check.\n");
                is_error++;
        }
        if (is_error > 0) {
                strcat(message, "{\n     \"almond\":\"Invalid check - no such plugin or flag\"\n}\n");
                socket_message = malloc((size_t)strlen(message)+1);
                if (socket_message == NULL) {
                        fprintf(stderr, "Failed to allocate memory.\n");
                        writeLog("Failed to allocate memory in [apiReadData:socket_message]", 2, 0);
                        return;
                }
                strcpy(socket_message, message);
                free(message);
                free(pluginName);
                message = pluginName = NULL;
                return;
        }
	// In new structure increase id with 1
	//plugin_id += 1;
        snprintf(strNum, sizeof(strNum), "%d", plugin_id);
        setApiCmdFile("update", strNum);
	pluginName = (char *)malloc((size_t)(pluginitemname_size+1) * sizeof(char));
	if (pluginName == NULL) {
		fprintf(stderr, "Memory allocation failed.\n");
		writeLog("Failed to allocate memory [apiRunAndRead:pluginName]", 2, 0);
		return;
	}
	else
		memset(pluginName, '\0', (size_t)(pluginitemname_size+1) * sizeof(char));
        strncpy(pluginName, g_plugins[plugin_id]->name, (size_t)pluginitemname_size+1);
        removeChar(pluginName, '[');
        removeChar(pluginName, ']');
        //runPlugin(plugin_id, 0);
        PluginItem *item = g_plugins[plugin_id];
        if (item) {
            run_plugin(item);
        }
	if (timeScheduler)
		rescheduleChecks();
        createUpdateFile(g_plugins[plugin_id], strNum);
	strcpy(message, "{\n     \"executePlugin\":\"");
        strcat(message, pluginName);
        strcat(message, "\",\n");
        strcat(message, "      \"result\": {\n");
	sleep(10);
	if (flags == API_FLAGS_VERBOSE) {
		strcat(message, "          \"name\":\"");
		strcat(message, pluginName);
		free(pluginName);
		pluginName = NULL;
		strcat(message, "\",\n");
		strcat(message, "          \"description\":\"");
                strcat(message, g_plugins[plugin_id]->description);
                strcat(message, "\",\n");
                switch (g_plugins[plugin_id]->output.retCode) {
                        case 0:
                                strcat(message, "          \"pluginStatus\":\"OK\",\n");
                                break;
                        case 1:
                                strcat(message, "          \"pluginStatus\":\"WARNING\",\n");
                                break;
                        case 2:
                                strcat(message, "          \"pluginStatus\":\"CRITICAL\",\n");
                                break;
                        default:
                                strcat(message, "          \"pluginStatus\":\"UNKNOWN\",\n");
                                break;
                }
                strcat(message, "          \"pluginStatusCode\":\"");
                sprintf(rCode, "%d", g_plugins[plugin_id]->output.retCode);
                strcat(message, trim(rCode));
                strcat(message,  "\",\n");
                strcat(message, "          \"pluginOutput\":\"");
                strcat(message, trim(g_plugins[plugin_id]->output.retString));
                strcat(message, "\",\n");
                strcat(message, "          \"pluginStatusChanged\":\"");
                strcat(message, g_plugins[plugin_id]->statusChanged);
                strcat(message, "\",\n");
                strcat(message, "          \"lastChange\":\"");
                strcat(message, g_plugins[plugin_id]->lastChangeTimestamp);
                strcat(message, "\",\n");
                strcat(message, "          \"lastRun\":\"");
                strcat(message, g_plugins[plugin_id]->lastRunTimestamp);
                strcat(message, "\",\n");
                strcat(message, "          \"nextScheduledRun\":\"");
                strcat(message, g_plugins[plugin_id]->nextRunTimestamp);
                strcat(message, "\"\n     }\n");
	}
	else {
		strcat(message, "          \"returnString\":\"");
		strcat(message, trim(g_plugins[plugin_id]->output.retString));
		strcat(message, "\"\n     }\n");
	}
	strcat(message, "}\n");
	if (socket_message != NULL) {
		free(socket_message);
		socket_message = NULL;
	}
	socket_message = malloc((size_t)(apimessage_size+1) * sizeof(char)); 
	if (socket_message == NULL) {
		fprintf(stderr, "Failed to allocate memory.\n");
		writeLog("Failed to allocate memory [apiRunAndRead:socket_message]", 2, 0);
		return;
	}
	if (strlen(message) > apimessage_size) {
		printf("Message is to big. Try increase apimessage_size.\n");
		message[apimessage_size-1] = '\0';
	}
	strncpy(socket_message, message, (size_t)apimessage_size);
	if (pluginName != NULL) {
		free(pluginName);
		pluginName = NULL;
	}
	if (message != NULL) {
		free(message);
		message = NULL;
	}
}
#endif

/* apiGetMetrics moved to api.c */
#if 0
void apiGetMetrics() {
	char ch = '/';

	snprintf(storeName, storename_size, "%s%c%s", storeDir, ch, metricsFileName);
	apiReadFile(storeName, 2);
}

#if 0
void apiGetHostName() {
	char nm[9];
	strcpy(nm, "hostname");
	constructSocketMessage(nm, hostName);
}
#endif

#if 0
void apiShowVersion() {
	char version[8];
	strcpy(version, "version");
	constructSocketMessage(version, VERSION);
}
#endif

#if 0
void apiShowStatus() {
	FILE *fp;
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);

	double user_time = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6;
	double system_time = usage.ru_stime.tv_sec + usage.ru_stime.tv_usec /1e6; 
	pid_t pid = getppid();

        json_object *jobj = json_object_new_object();
	json_object_object_add(jobj, "hostname", json_object_new_string(hostName));
        json_object_object_add(jobj, "almond_version", json_object_new_string(VERSION));
	json_object_object_add(jobj, "pid", json_object_new_int(pid));
	fp = fopen("/proc/uptime", "r");
    	if (fp) { 
		double uptime = 0.0;
    		if (fscanf(fp, "%lf", &uptime) == 1) {
        		json_object_object_add(jobj, "uptime_seconds", json_object_new_double(uptime));
    		}
    		fclose(fp);
	}
	json_object_object_add(jobj, "plugin_count", json_object_new_int(decCount));
	json_object_object_add(jobj, "user_cpu_time", json_object_new_double(user_time));
	json_object_object_add(jobj, "system_cpu_tume", json_object_new_double(system_time));
	struct mallinfo2 mi = mallinfo2();  // glibc >= 2.33
        json_object_object_add(jobj, "heap_allocated_kb", json_object_new_int64(mi.uordblks / 1024));
        json_object_object_add(jobj, "heap_total_kb",     json_object_new_int64(mi.arena / 1024));
	json_object_object_add(jobj, "max_resident_set_size_kb", json_object_new_int64(usage.ru_maxrss));
	struct rlimit rl;
    	if (getrlimit(RLIMIT_STACK, &rl) == 0) {
        	json_object_object_add(jobj, "stack_size_kb", json_object_new_int64(rl.rlim_cur / 1024));
    	}
	fp = fopen("/proc/self/statm", "r");
    	if (fp) {
		long rss_pages = 0;
    		if (fscanf(fp, "%*s %ld", &rss_pages) == 1) {
        		long page_size_kb = sysconf(_SC_PAGESIZE) / 1024;
        		json_object_object_add(jobj, "rss_kb", json_object_new_int64(rss_pages * page_size_kb));
    		}
    		fclose(fp);
	}
	json_object_object_add(jobj, "minor_page_faults", json_object_new_int64(usage.ru_minflt));
	json_object_object_add(jobj, "major_page_faults", json_object_new_int64(usage.ru_majflt));
	json_object_object_add(jobj, "swaps", json_object_new_int64(usage.ru_nswap));
	json_object_object_add(jobj, "block_input_ops", json_object_new_int64(usage.ru_inblock));
	json_object_object_add(jobj, "block_output_ops", json_object_new_int64(usage.ru_oublock));
	json_object_object_add(jobj, "ipc_msgs_sent", json_object_new_int64(usage.ru_msgsnd));
	json_object_object_add(jobj, "ipc_msgs_received", json_object_new_int64(usage.ru_msgrcv));
	json_object_object_add(jobj, "signals_received", json_object_new_int64(usage.ru_nsignals));
	json_object_object_add(jobj, "voluntary_context_switches", json_object_new_int64(usage.ru_nvcsw));
	json_object_object_add(jobj, "involuntary_context_switches", json_object_new_int64(usage.ru_nivcsw));
	json_object_object_add(jobj, "thread_count", json_object_new_int(get_thread_count()));
	json_object_object_add(jobj, "open_file_descriptors", json_object_new_int64(get_fd_count()));
	fp = fopen("/proc/self/io", "r");
	if (fp) {
		char line[256];
    		while (fgets(line, sizeof(line), fp)) {
        		char key[64];
        		unsigned long long value;
			if (sscanf(line, "%63[^:]: %llu", key, &value) == 2) {
            			json_object_object_add(jobj, key, json_object_new_int64(value));
        		}
    		}
		fclose(fp);
	}
        const char *json_str = json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY);
        int size = strlen(json_str) + 2;
        socket_message = malloc((size_t)size);
        if (socket_message == NULL) {
                printf("Memory allocation failed.\n");
                writeLog("Memory allocation failed [constructSocketMessage:socket_message]", 2, 0);
                return;
        }
        else
                memset(socket_message, '\0', (size_t)size * sizeof(char));
        snprintf(socket_message, (size_t)size, "%s\n", json_str);
        json_object_put(jobj);
}
#endif

#if 0
void apiShowPluginStatus() {
	int num_of_oks = 0, num_of_warnings = 0, num_of_criticals = 0, num_of_unknowns = 0;
	for (int i = 0; i < decCount; i++) {
		switch(g_plugins[i]->output.retCode) {
			case 0:
				num_of_oks++;
				break;
			case 1:
				num_of_warnings++;
				break;
			case 2:
				num_of_criticals++;
				break;
			default:
				num_of_unknowns++;
				break;
		}
	}
	json_object *jobj = json_object_new_object();
	json_object_object_add(jobj, "number_of_checks", json_object_new_int(decCount));
	json_object_object_add(jobj, "ok", json_object_new_int(num_of_oks));
	json_object_object_add(jobj, "warning", json_object_new_int(num_of_warnings));
	json_object_object_add(jobj, "critical", json_object_new_int(num_of_criticals));
	json_object_object_add(jobj, "unknown", json_object_new_int(num_of_unknowns));
	const char *json_str = json_object_to_json_string(jobj);
	int size = strlen(json_str) + 2;
        socket_message = malloc((size_t)size);
        if (socket_message == NULL) {
                printf("Memory allocation failed.\n");
                writeLog("Memory allocation failed [constructSocketMessage:socket_message]", 2, 0);
                return;
        }
        else
                memset(socket_message, '\0', (size_t)size * sizeof(char));
        snprintf(socket_message, (size_t)size, "%s\n", json_str);
	json_object_put(jobj);
}
#endif

#if 0
void apiCheckPluginConf() {
	int res = check_plugin_conf_file(pluginDeclarationFile);
	if (res == 0) {
		constructSocketMessage("pluginconfiguration", "true");
	}
	else
		constructSocketMessage("pluginconfiguration", "false");
}
#endif

#if 0
void apiGetVars(int v) {
	switch (v) {
		case 1:
			if (kafka_tag == NULL)
                        	constructSocketMessage("kafkatag", "NULL");
                	else
                        	constructSocketMessage("kafkatag", kafka_tag);
			break;
		case 2:
			constructSocketMessage("metricsprefix", metricsOutputPrefix);
			break;
		case 3:
			constructSocketMessage("jsonfilename", jsonFileName);
			break;
		case 4:
			constructSocketMessage("metricsfilename", metricsFileName);
			break;
		case 5:
			if (useKafkaConfigFile) {
				char* currentTopic = getKafkaTopic();
				if (currentTopic != NULL) {
					constructSocketMessage("kafkatopic", currentTopic);
				}
				else {
					constructSocketMessage("kafkatopic", "NULL");
				}
			}
			else if (kafka_topic == NULL)
                        	constructSocketMessage("kafkatopic", "NULL");
			else
                        	constructSocketMessage("kafkatopic", kafka_topic);
			break;
		case 6:
			int length = snprintf(NULL, 0, "%d", schedulerSleep);
			char* sleep_num = malloc(length + 1);
			snprintf(sleep_num, length + 1,  "%d", schedulerSleep);
			constructSocketMessage("schedulersleep", sleep_num);
			free(sleep_num);
			break;
		case 7:
			char soe_val[6];
			sprintf(soe_val, "%s", saveOnExit ? "true" : "false");
			constructSocketMessage("saveonexit", soe_val);
			break;
		case 8:
			char plo_val[6];
			sprintf(plo_val, "%s", logPluginOutput ? "true" : "false");
			constructSocketMessage("pluginoutput", plo_val);
			break;
		case 9:
			char s_kStartId[2];
			sprintf(s_kStartId, "%d", kafka_start_id);
			constructSocketMessage("kafkastartid", s_kStartId);
			break;
		case 10:
			char plts[14];
			sprintf(plts, "%ld", tPluginFile);
			constructSocketMessage("pluginslastchangets", plts);
			break;
		case 11:
			if (!external_scheduler) {
				constructSocketMessage("scheduler", "internal");
			}
			else {
				constructSocketMessage("scheduler", "external");
			}
			break;
		case 12:
			if (push_url == NULL) 
				constructSocketMessage("pushurl", "NULL");
			else
				constructSocketMessage("pushurl", push_url);
			break;
		case 13:
			char a_port[10];
			sprintf(a_port, "%d", push_port);
			constructSocketMessage("pushport", a_port);
			break;
		case 14:
			int length = snprintf(NULL, 0, "%d", push_interval);
                        char* p_interval = malloc(length + 1);
                        snprintf(p_interval, length + 1,  "%d", push_interval);
                        constructSocketMessage("pushinterval", p_interval);
                        free(p_interval);
			break;
		default:
			constructSocketMessage("getvar", "No matching object found");
	}
}
#endif
#endif

void apiReadAll() {
	//char ch = '/';

	/*strcpy(fileName, dataDir);
	strncat(fileName, &ch, 1);
	strcat(fileName, jsonFileName);*/
	int written = snprintf(fileName, filename_size, "%s/%s", dataDir, jsonFileName);
	if (written < 0) {
		writeLog("Could not read from jsonfile. Encoding error getting file name.", 1, 0);
	}
	else if ((size_t)written >= filename_size) {
		writeLog("Could not get jsonfile. Name is too long.", 1, 0);
	}
	else 
		apiReadFile(fileName, 0); 
}

/*void collectJsonData(int decLen){
	//char ch = '/';
	char* pluginName = NULL;
	char plts[14];
	FILE *fp = NULL;
        clock_t t;

	if (fileName == NULL || dataDir == NULL) {
		printf("Variabels in collectJsonData is empty.\n");
		return;
	}
	pthread_mutex_lock(&update_mtx);
	//strcpy(fileName, dataDir);
	//strncat(fileName, &ch, 1);
	//strcat(fileName, jsonFileName)/
	int written = snprintf(fileName, filename_size, "%s/%s", dataDir, jsonFileName);
	if (written < 0) {
		writeLog("Could not write to json file", 2, 0);
	}
	if ((size_t)written >= filename_size) {
		writeLog("Json file name truncated. Name is too long.", 1, 0);
	}
	snprintf(infostr, infostr_size, "Collecting data to file: %s", fileName);
	writeLog(trim(infostr), 0, 0);
	t = clock();
	fp = fopen(fileName, "w");
	fputs("{\n", fp);
	fprintf(fp, "   \"host\": {\n");
	fprintf(fp, "      \"name\":\"");
	fputs(hostName, fp);
	fprintf(fp, "\",\n");
	fprintf(fp, "      \"pluginfileupdatetime\":\"");
	sprintf(plts, "%ld", tPluginFile);
        fputs(plts, fp);
        fprintf(fp, "\"\n");
	fputs("   },\n", fp);
	fputs("   \"monitoring\": [\n", fp);
	for (int i = 0; i < decLen; i++) {
		//pluginName = (char *)malloc((size_t)pluginitemname_size * sizeof(char)+1);
		pluginName = strdup(g_plugins[i]->name);
		if (pluginName == NULL) {
			fprintf(stderr, "Memory allocation failed.\n");
			writeLog("Failed to allocate memory [collectJsonData:pluginName]", 2, 0);
			return;
		}
		removeChar(pluginName, '[');
		removeChar(pluginName, ']');
		fputs("      {\n", fp);
		fprintf(fp, "         \"name\":\"%s\",\n", pluginName);
		free(pluginName);
		pluginName = NULL;
		fprintf(fp, "         \"pluginName\":\"%s\",\n", g_plugins[i]->description);
		switch(g_plugins[i]->output.retCode) {
			case 0:
			   fputs("         \"pluginStatus\":\"OK\",\n", fp);
			   break;
			case 1:
			   fputs("         \"pluginStatus\":\"WARNING\",\n", fp);
			   break;
			case 2:
			   fputs("         \"pluginStatus\":\"CRITICAL\",\n", fp);
                           break;
			default:
			   fputs("         \"pluginStatus\":\"UNKNOWN\",\n", fp);
                           break;
		}
		fprintf(fp, "         \"pluginStatusCode\":\"%d\",\n", g_plugins[i]->output.retCode);
		fprintf(fp, "         \"pluginOutput\":\"%s\",\n", trim(g_plugins[i]->output.retString));
		fprintf(fp, "         \"pluginStatusChanged\":\"%s\",\n", g_plugins[i]->statusChanged);
		if (g_plugins[i]->active > 0)
                        fputs("         \"maintenance\":\"false\",\n", fp);
                else
                        fputs("         \"maintenance\":\"true\",\n", fp);
		fprintf(fp, "         \"lastChange\":\"%s\",\n", g_plugins[i]->lastChangeTimestamp);
		fprintf(fp, "         \"lastRun\":\"%s\", \n", g_plugins[i]->lastRunTimestamp);
		fprintf(fp, "         \"nextRun\":\"%s\"\n", g_plugins[i]->nextRunTimestamp);
		if (i == decLen-1) {
			fputs("      }\n", fp);
		}
		else {
			fputs("      },\n", fp);
		}
	}
        fputs("   ]\n", fp);
	fputs("}\n", fp);
	fclose(fp);
	fp = NULL;
	t = clock() -t;
	//double collection_time = ((double)t)/CLOCKS_PER_SEC;
	//printf("Data collection took %f seconds to execute.\n", collection_time);
	//printf("Data collection took %.0f miliseconds to execute.\n", (double)t);
	//free(dataFName);
	snprintf(infostr, infostr_size, "Data collection took %.0f miliseconds to execute.", (double)t);
	writeLog(trim(infostr), 0, 0);
	pthread_mutex_unlock(&update_mtx);
}*/

void collectJsonData(int decLen){
    char *pluginName = NULL;
    char plts[32];
    FILE *tf = NULL;
    int tmpfd = -1;
    char tmpname[1024];
    char targetname[1024];
    clock_t t;

    if (fileName == NULL || dataDir == NULL) {
        printf("Variables in collectJsonData is empty.\n");
        return;
    }

    pthread_mutex_lock(&update_mtx);

    /* build target path */
    int written = snprintf(targetname, sizeof(targetname), "%s/%s", dataDir, jsonFileName);
    if (written < 0) {
        writeLog("Could not write to json file", 2, 0);
        pthread_mutex_unlock(&update_mtx);
        return;
    }
    if ((size_t)written >= sizeof(targetname)) {
        writeLog("Json file name truncated. Name is too long.", 1, 0);
        pthread_mutex_unlock(&update_mtx);
        return;
    }

    /* build temp template in same directory; mkstemp requires XXXXXX */
    written = snprintf(tmpname, sizeof(tmpname), "%s/.%s.tmpXXXXXX", dataDir, jsonFileName);
    if (written < 0 || (size_t)written >= sizeof(tmpname)) {
        writeLog("Temp file name truncated. Name is too long.", 1, 0);
        pthread_mutex_unlock(&update_mtx);
        return;
    }

    /* create temp file securely */
    tmpfd = mkstemp(tmpname);
    if (tmpfd == -1) {
        writeLog("Failed to create temp file for JSON output", 2, 0);
        pthread_mutex_unlock(&update_mtx);
        return;
    }

    /* optionally set desired permissions (e.g., 0644) */
    if (fchmod(tmpfd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) == -1) {
        /* non-fatal, but log */
        writeLog("Warning: fchmod on temp file failed", 1, 0);
    }

    /* get FILE* for convenience */
    tf = fdopen(tmpfd, "w");
    if (tf == NULL) {
        writeLog("fdopen failed for temp file", 2, 0);
        close(tmpfd);
        unlink(tmpname);
        pthread_mutex_unlock(&update_mtx);
        return;
    }

    /* write JSON to temp file (same content as before) */
    snprintf(infostr, infostr_size, "Collecting data to temp file: %s", tmpname);
    writeLog(trim(infostr), 0, 0);

    t = clock();
    fputs("{\n", tf);
    fprintf(tf, "   \"host\": {\n");
    fprintf(tf, "      \"name\":\"");
    fputs(hostName, tf);
    fprintf(tf, "\",\n");
    fprintf(tf, "      \"pluginfileupdatetime\":\"");
    sprintf(plts, "%ld", tPluginFile);
    fputs(plts, tf);
    fprintf(tf, "\"\n");
    fputs("   },\n", tf);
    fputs("   \"monitoring\": [\n", tf);

    for (int i = 0; i < decLen; i++) {
        pluginName = strdup(g_plugins[i]->name);
        if (pluginName == NULL) {
            fprintf(stderr, "Memory allocation failed.\n");
            writeLog("Failed to allocate memory [collectJsonData:pluginName]", 2, 0);
            /* cleanup and exit */
            fclose(tf);
            unlink(tmpname);
            pthread_mutex_unlock(&update_mtx);
            return;
        }
        removeChar(pluginName, '[');
        removeChar(pluginName, ']');
        fputs("      {\n", tf);
        fprintf(tf, "         \"name\":\"%s\",\n", pluginName);
        free(pluginName);
        pluginName = NULL;
        fprintf(tf, "         \"pluginName\":\"%s\",\n", g_plugins[i]->description);
        switch(g_plugins[i]->output.retCode) {
            case 0:
               fputs("         \"pluginStatus\":\"OK\",\n", tf);
               break;
            case 1:
               fputs("         \"pluginStatus\":\"WARNING\",\n", tf);
               break;
            case 2:
               fputs("         \"pluginStatus\":\"CRITICAL\",\n", tf);
               break;
            default:
               fputs("         \"pluginStatus\":\"UNKNOWN\",\n", tf);
               break;
        }
        fprintf(tf, "         \"pluginStatusCode\":\"%d\",\n", g_plugins[i]->output.retCode);
        fprintf(tf, "         \"pluginOutput\":\"%s\",\n", trim(g_plugins[i]->output.retString));
        fprintf(tf, "         \"pluginStatusChanged\":\"%s\",\n", g_plugins[i]->statusChanged);
        if (g_plugins[i]->active > 0)
            fputs("         \"maintenance\":\"false\",\n", tf);
        else
            fputs("         \"maintenance\":\"true\",\n", tf);
        fprintf(tf, "         \"lastChange\":\"%s\",\n", g_plugins[i]->lastChangeTimestamp);
        fprintf(tf, "         \"lastRun\":\"%s\", \n", g_plugins[i]->lastRunTimestamp);
        fprintf(tf, "         \"nextRun\":\"%s\"\n", g_plugins[i]->nextRunTimestamp);
        if (i == decLen-1)
            fputs("      }\n", tf);
        else
            fputs("      },\n", tf);
    }

    fputs("   ]\n", tf);
    fputs("}\n", tf);

    /* flush stdio buffers */
    if (fflush(tf) != 0) {
        writeLog("fflush failed on temp file", 2, 0);
        fclose(tf);
        unlink(tmpname);
        pthread_mutex_unlock(&update_mtx);
        return;
    }

    /* ensure data is on disk */
    if (fsync(fileno(tf)) == -1) {
        writeLog("fsync failed on temp file", 1, 0);
        /* continue — depending on your durability needs you may treat this as fatal */
    }

    /* close FILE* (this also closes the underlying fd) */
    if (fclose(tf) != 0) {
        writeLog("fclose failed on temp file", 2, 0);
        unlink(tmpname);
        pthread_mutex_unlock(&update_mtx);
        return;
    }
    tf = NULL;

    /* atomically replace target with temp file */
    if (rename(tmpname, targetname) != 0) {
        snprintf(infostr, infostr_size, "[Collect data] Rename failed: %s", strerror(errno));
        writeLog(trim(infostr), 2, 0);
        unlink(tmpname);
        pthread_mutex_unlock(&update_mtx);
        return;
    }

    t = clock() - t;
    snprintf(infostr, infostr_size, "Data collection took %.0f miliseconds to execute.", (double)t);
    writeLog(trim(infostr), 0, 0);

    pthread_mutex_unlock(&update_mtx);
}

void collectMetrics(int decLen, int style) {
        //char ch = '/';
	char* pluginName = NULL;
	char* serviceName = NULL;
	FILE *mf = NULL;
        clock_t t;
	char *p = NULL;
	int metricsValueLength = 0;
	/*int tmpfd = -1;
    	char tmpname[1024];
    	char targetname[1024];*/

        t = clock();
	pthread_mutex_lock(&update_mtx);
        /*strncpy(storeName, storeDir, storedir_size);
        strncat(storeName, &ch, 1);
        strcat(storeName, metricsFileName);*/
	snprintf(storeName, storename_size, "%s/%s", storeDir, metricsFileName); 
        mf = fopen(storeName, "w");
	if (mf == NULL) {
		writeLog("Failed to open metrics file", 1, 0);
		fprintf(stderr, "Failed to open metrics file\n");
		return;
	}
        snprintf(infostr, infostr_size, "Collecting metrics to file: %s", storeName);
        writeLog(trim(infostr), 0, 0);
	for (int i = 0; i < decLen; i++) {
		/*pluginName = (char *)malloc((size_t)pluginitemname_size * sizeof(char)+1);
		memset(pluginName, '\0', pluginitemname_size+1 * sizeof(char));
		if (pluginName == NULL) {
			fprintf(stderr, "Memory allocation failed.\n");
			writeLog("Memory allocation failed [collectMetrics:pluginName]", 2, 0);
			return;
		}*/
		pluginName = strdup(g_plugins[i]->name);
		if (!pluginName) {
			fprintf(stderr, "Memory allocation failed.\n");
                        writeLog("Memory allocation failed [collectMetrics:pluginName]", 2, 0);
                        return;
		}
        	removeChar(pluginName, '[');
        	removeChar(pluginName, ']');
		for (p = pluginName; *p != '\0'; ++p) {
			//if (*p == '/') *p = '_';
			*p = tolower(*p);
		}
        	// Get metrics
        	char *e;
		char *raw = g_plugins[i]->output.retString;
		char *trimmed_raw = raw ? trim(raw) : "";
		if (raw == NULL || strchr(raw, '|') == NULL) {
			snprintf(infostr, infostr_size, "Plugin %s does not provide metrics. Using plain output.",pluginName);
        		writeLog(trim(infostr), 1, 0);
		//if (strchr(outputs[i].retString, '|') == NULL) {
		//	snprintf(infostr, infostr_size, "Plugin %s does not provide metrics. Using plain output.", pluginName);
		//	writeLog(trim(infostr), 1, 0);
			const char *prefix = trim(metricsOutputPrefix);
			if (style == 0)
                       		fprintf(mf, "%s_%s{hostname=\"%s\",%s_result=\"%s\"} %d\n", prefix, pluginName, hostName, pluginName, trimmed_raw, g_plugins[i]->output.retCode);
			else { 
				// Get service name	
				/*serviceName = (char *)malloc((size_t)pluginitemdesc_size * sizeof(char));
				if (serviceName == NULL) {
					fprintf(stderr, "Failed to allocate memory.\n");
					writeLog("Failed to allocate memory [collectMetrics:serviceName]", 2, 0);
					return;
				}
				memset(serviceName, '\0', pluginitemdesc_size * sizeof(char) + 1);
				strcpy(serviceName, g_plugins[i].description);*/
				const char *service = trim(g_plugins[i]->description);
				fprintf(mf, "%s_%s{hostname=\"%s\", service=\"%s\", value=\"%s\"} %d\n", prefix, pluginName, hostName, service, trimmed_raw, g_plugins[i]->output.retCode);
				free(serviceName);
				serviceName = NULL;
			}
		}
                else {
        	 	e = strchr(g_plugins[i]->output.retString, '|');
        	    	int position = (int)(e - g_plugins[i]->output.retString);
			int len = pluginoutput_size;
			size_t srcSize = strlen(g_plugins[i]->output.retString) - position;
			int sublen = (srcSize < len) ? srcSize : len;
			char * metrics = malloc((size_t)sizeof(char) * sublen);
			memset(metrics, 0, sizeof(char) * sublen);
			if (sublen <= srcSize) {
        			//memcpy(metrics,&outputs[i].retString[position+1],sublen);
				memcpy(metrics, &g_plugins[i]->output.retString[position+1],sublen);
			}
			else {
				writeLog("Invalid memcpy operation: size exceeds buffer limit.", 1, 0);
				fprintf(stderr, "Size exceeds buffer [memcpy].\n");
			}
			if (style == 0)
				fprintf(mf, "%s_%s{hostname=\"%s\", %s_result=\"%s\"} %d\n", trim(metricsOutputPrefix), pluginName, hostName, pluginName, trim(g_plugins[i]->output.retString), g_plugins[i]->output.retCode);
			else {
				serviceName = (char *)malloc((size_t)pluginitemdesc_size * sizeof(char) + 1);
				if (serviceName == NULL) {
					fprintf(stderr, "Memory allocation failed.\n");
					writeLog("Failed to allocate memory [collectMetrics:serviceName]", 2, 0);
					return;
				}
				memset(serviceName, '\0', pluginitemdesc_size * sizeof(char) + 1);
				strcpy(serviceName, g_plugins[i]->description);
				// We need to loop through metrics
				char * token = strtok(metrics, " ");
				while (token != NULL) {
					char* metricsToken;
					char* metricsName;
					char* metricsValue;
					metricsToken = malloc((size_t)strlen(token)+1);
					if (metricsToken == NULL) {
						writeLog("Failed to allocate memory [collectMetrics:metricsToken]", 2, 0);
						return;
					}
					memset(metricsToken, '\0', (size_t)strlen(token)+1 * sizeof(char));
					int do_cut = 0;
					const char *haystring = ";";
					char *c = token;
					while (*c) {
						if (strchr(haystring, *c)) {
							do_cut++;
						}
						c++;
					}
					char *e = strchr(token, ';');
                                        int index = (int)(e - token);
					if (do_cut > 0) {
						strcpy(metricsToken, token);
						metricsToken[index] = '\0';
					}
					else {
						strcpy(metricsToken, token);
					}
					char *f = strchr(metricsToken, '=');
					index = (int)(f - metricsToken);
					metricsName = malloc((size_t)strlen(metricsToken)+1);
					if (metricsName == NULL) {
						writeLog("Failed to allocate memory [collectMetrics:metricsName]", 2, 0);
						return;
					}
					else
						memset(metricsName, '\0', (size_t)strlen(metricsToken)+1);
                                        strcpy(metricsName, metricsToken);
					if (strlen(metricsName) < 5) {
						return;
					}
					char *endOfMetricsName = f+1;
					if (endOfMetricsName != NULL) {
						char *nullTerminator = strchr(endOfMetricsName, '\0');
						if (nullTerminator != NULL) {
							metricsValueLength = strlen(endOfMetricsName);
						}
						else {
							printf("Warn: endOfMetricsName is not null-terminated.\n");
                                                        return;
						}
					}
					else {
						printf("Warn: can not set metric value length.\n");
                                                return;
                                        }

					metricsValue = malloc((size_t)(metricsValueLength+1) * sizeof(char));
					if (metricsValue == NULL) {
						writeLog("Failed to allocate memory [collectMetrics:metricsValue]", 2, 0);
						return;
					}
					else
						memset(metricsValue, '\0', (size_t)(metricsValueLength+1) * sizeof(char));
					strncpy(metricsValue, metricsName + index +1, (size_t)metricsValueLength);
					metricsName[index] = '\0';
					char *pm;
					for (pm = metricsName; *pm != '\0'; ++pm) 
			                        *pm = tolower(*pm);
					removeChar(metricsName, '/');
					 char * cleanMetricsValue = malloc(metricsValueLength +1);
                                        if (!cleanMetricsValue) {
                                                writeLog("Failed to allocate memory [collectMetrics: cleanMetricsValue]", 2, 0);
                                                return;
                                        }
                                        int count = 0;
                                        for (int i = 0; i < metricsValueLength; ++i) {
                                                if (isdigit(metricsValue[i]) || (metricsValue[i] == '.')) {
                                                        cleanMetricsValue[count++] = metricsValue[i];
                                                }
                                        }
                                        cleanMetricsValue[count] = '\0';
					fprintf(mf, "%s_%s_%s{hostname=\"%s\", service=\"%s\", key=\"%s\"} %s\n", trim(metricsOutputPrefix), pluginName, metricsName, hostName, serviceName, metricsName, cleanMetricsValue);
					free(metricsValue);
					metricsValue = NULL;
					free(metricsName);
					metricsName = NULL;
					free(metricsToken);
					metricsToken = NULL;
					free(cleanMetricsValue);
					cleanMetricsValue = NULL;
					token = strtok(NULL, " ");
				}
				free(serviceName);
				serviceName = NULL;
				free(metrics);
				metrics = NULL;
			}
		}
        	free(pluginName);
		pluginName = NULL;
	}
	fclose(mf);
	mf = NULL;
        t = clock() -t;
	pthread_mutex_unlock(&update_mtx);
        snprintf(infostr, infostr_size, "Metrics collection took %.0f miliseconds to execute.", (double)t);
        writeLog(trim(infostr), 0, 0);
}

void timeTune(int seconds) {
	int i;
	size_t dest_size = 20;
	snprintf(infostr, infostr_size, "Tuning up run times %d seconds", seconds);
	writeLog(trim(infostr), 0, 0);
	// Loop through and change nextTimeValue
	for (i = 0; i < decCount; i++) {
		if (i != timeTunerMaster) {
			time_t nextTime = g_plugins[i]->nextRun + seconds;
                	struct tm tNextTime;
                	memset(&tNextTime, '\0', sizeof(struct tm));
               	 	localtime_r(&nextTime, &tNextTime);
                	int len = snprintf(g_plugins[i]->nextRunTimestamp, dest_size, "%04d-%02d-%02d %02d:%02d:%02d", tNextTime.tm_year + 1900, tNextTime.tm_mon +1, tNextTime.tm_mday, tNextTime.tm_hour, tNextTime.tm_min, tNextTime.tm_sec);
			if (len >= dest_size) {
				writeLog("Truncation of timestamp possible in funtion 'timeTune'", 1, 0);
			}
                	g_plugins[i]->nextRun = nextTime;
			if (timeScheduler)
				scheduler[g_plugins[i]->id].timestamp = nextTime;
		}
	}
	if (timeScheduler) {
		checkSchedulerCount();
		qsort(scheduler, decCount, sizeof(struct Scheduler), compare_timestamps);
	}
}

void writePluginResultToFile(int storeIndex, int update) {
	FILE *fp = NULL;
	char* checkName;
	char timestr[35];
	char ch = '/';
	if (update == 0)
		checkName = strdup(g_plugins[storeIndex]->name);
	else
		checkName = strdup(update_g_plugins[storeIndex].name);
	//memmove(checkName, checkName+1,strlen(checkName));
	//checkName[strlen(checkName)-1] = '\0';
	/*strcpy(fileName, storeDir);
	strncat(fileName, &ch, 1);
	strcat(fileName, checkName);*/
	snprintf(fileName, filename_size, "%s%c%s", storeDir, ch, checkName);
	free(checkName);
	checkName = NULL;
	time_t rawtime;
	struct tm * timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strcpy(timestr, asctime(timeinfo));
	timestr[strlen(timestr)-1] = '\0';
	if (fileExists(fileName) == 0) {
		fp = fopen(fileName, "a");
	}
	else {
		fp = fopen(fileName, "w+");
	}
	if (update == 0) {
		if (g_plugins[storeIndex]->name && pluginReturnString) {
			if (fp != NULL)
				fprintf(fp, "%s, %s, %s\n", timestr, g_plugins[storeIndex]->name, g_plugins[storeIndex]->output.retString);
			else {
				printf("DEBUG: Could not find file stream. Error.\n");
				writeLog("Could not find file stream [writePluginResultToFile]", 1, 0);
				return;
			}
		}
		fflush(fp);
	}
	else
		fprintf(fp, "%s, %s, %s\n", timestr, update_g_plugins[storeIndex].name, pluginReturnString);
	fclose(fp);
	fp = NULL;
}

void writeToKafkaTopic(int storeIndex, int update) {
	char *payload;
	char *pluginName;
	char *pluginStatus;
	char currTime[TIME_BUF_LEN];
	size_t dest_size = 20;
        time_t tTime = time(NULL);
        struct tm tm = *localtime(&tTime);

        int len = snprintf(currTime, max_timestamp_size, "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon +1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	if (len >= dest_size) {
		writeLog("Possible truncation of timestamp in function 'writeToKafkaTopic'.", 1, 0);
	}
	pluginName = malloc((size_t)pluginitemname_size+1 * sizeof(char));
        if (pluginName == NULL) {
        	fprintf(stderr, "Memory allocation failed.\n");
        	writeLog("Failed to allocate memory [runPlugin:enableKafkaExport:pluginName]", 2, 0);
        	return;
	}
       	if (update == 0)
       		pluginName = strdup(g_plugins[storeIndex]->name);
	else
       		pluginName = strdup(update_g_plugins[storeIndex].name);
        removeChar(pluginName, '[');
        removeChar(pluginName, ']');
        switch(g_plugins[storeIndex]->output.retCode) {
        	case 0:
        		pluginStatus = malloc(3);
        		strcpy(pluginStatus, "OK");
        		break;
        	case 1:
        		pluginStatus = malloc(8);
        		strcpy(pluginStatus, "WARNING");
        		break;
        	case 2:
        		pluginStatus = malloc(9);
        		strcpy(pluginStatus, "CRITICAL");
        		break;
        	default:
        		pluginStatus = malloc(8);
        		strcpy(pluginStatus, "UNKNOWN");
        		break;
	}
        int count_bytes = strlen(hostName) + strlen(g_plugins[storeIndex]->lastChangeTimestamp) + strlen(g_plugins[storeIndex]->lastRunTimestamp) + strlen(g_plugins[storeIndex]->name) + strlen(g_plugins[storeIndex]->nextRunTimestamp);
        count_bytes += pluginitemdesc_size + pluginoutput_size;
        count_bytes += strlen(pluginStatus) + strlen(g_plugins[storeIndex]->statusChanged);
        count_bytes += 185;
        int kafka_export_addons = 0;
        if (enableKafkaTag) {
        	count_bytes += strlen(kafka_tag);
        	count_bytes += 12; // {"tag":""}
        	kafka_export_addons += 10;
        }
        if (enableKafkaId) {
        	count_bytes += 9; // {"id":""}
        	int length = snprintf(NULL, 0, "%d", kafka_start_id);
        	count_bytes += length;
        	kafka_export_addons += 20;
        }
	payload = malloc((size_t)count_bytes);
        if (payload == NULL) {
        	fprintf(stderr, "Could not allocate memory for payload.\n");
        	writeLog("Failed to allocate memory [runPlugin:enableKafkaExport:payload]", 2, 0);
        	return;
        }
        if (kafka_export_addons < 1) {
        	sprintf(payload, "{\"name\":\"%s\", \"data\": {\"lastChange\":\"%s\", \"lastRun\":\"%s\", \"name\":\"%s\", \"nextRun\":\"%s\", \"pluginName\":\"%s\", \"pluginOutput\":\"%s\", \"pluginStatus\":\"%s\", \"pluginStatusChanged\":\"%s\", \"pluginStatusCode\":\"%d\"}}", hostName, g_plugins[storeIndex]->lastChangeTimestamp, currTime, pluginName, g_plugins[storeIndex]->nextRunTimestamp, g_plugins[storeIndex]->description, g_plugins[storeIndex]->output.retString, pluginStatus, g_plugins[storeIndex]->statusChanged, g_plugins[storeIndex]->output.retCode);
        	printf("Payload = %s\n", payload);
        }
        else {
       		if (kafka_export_addons == KAFKA_EXPORT_TAG) {
        		sprintf(payload, "{\"name\":\"%s\", \"tag\":\"%s\", \"data\": {\"lastChange\":\"%s\", \"lastRun\":\"%s\", \"name\":\"%s\", \"nextRun\":\"%s\", \"pluginName\":\"%s\", \"pluginOutput\":\"%s\", \"pluginStatus\":\"%s\", \"pluginStatusChanged\":\"%s\", \"pluginStatusCode\":\"%d\"}}", hostName, kafka_tag, g_plugins[storeIndex]->lastChangeTimestamp, currTime, pluginName, g_plugins[storeIndex]->nextRunTimestamp, g_plugins[storeIndex]->description, g_plugins[storeIndex]->output.retString, pluginStatus, g_plugins[storeIndex]->statusChanged, g_plugins[storeIndex]->output.retCode);
        	}
        	else {
        		int nKafkaId = kafka_start_id + storeIndex;
        		int length = snprintf(NULL, 0, "%d", nKafkaId);
        		char* kafka_id = malloc((size_t)length + 1);
        		snprintf(kafka_id, (size_t)length+1, "%d", nKafkaId);
        		if (kafka_export_addons == KAFKA_EXPORT_ID) {
        			sprintf(payload, "{\"name\":\"%s\", \"id\":\"%s\", \"data\": {\"lastChange\":\"%s\", \"lastRun\":\"%s\", \"name\":\"%s\", \"nextRun\":\"%s\", \"pluginName\":\"%s\", \"pluginOutput\":\"%s\", \"pluginStatus\":\"%s\", \"pluginStatusChanged\":\"%s\", \"pluginStatusCode\":\"%d\"}}", hostName, kafka_id, g_plugins[storeIndex]->lastChangeTimestamp, currTime, pluginName, g_plugins[storeIndex]->nextRunTimestamp, g_plugins[storeIndex]->description, g_plugins[storeIndex]->output.retString, pluginStatus, g_plugins[storeIndex]->statusChanged, g_plugins[storeIndex]->output.retCode);
        		}
        		else if (kafka_export_addons == KAFKA_EXPORT_IDTAG) {
        			sprintf(payload, "{\"name\":\"%s\", \"id\":\"%s\",\"tag\":\"%s\", \"data\": {\"lastChange\":\"%s\", \"lastRun\":\"%s\", \"name\":\"%s\", \"nextRun\":\"%s\", \"pluginName\":\"%s\", \"pluginOutput\":\"%s\", \"pluginStatus\":\"%s\", \"pluginStatusChanged\":\"%s\", \"pluginStatusCode\":\"%d\"}}", hostName, kafka_id, kafka_tag, g_plugins[storeIndex]->lastChangeTimestamp, currTime, pluginName, g_plugins[storeIndex]->nextRunTimestamp, g_plugins[storeIndex]->description, g_plugins[storeIndex]->output.retString, pluginStatus, g_plugins[storeIndex]->statusChanged, g_plugins[storeIndex]->output.retCode);
                        }
                }
	}
	if (useKafkaConfigFile) {
		send_message_to_gkafka(payload);
		free(pluginName);
		free(pluginStatus);
		free(payload);
		pluginName = NULL;
		pluginStatus = NULL;
		payload = NULL;
		return;
	}
        if (!enableKafkaSSL) {
		if (!kafkaAvro)
			send_message_to_kafka(kafka_brokers, kafka_topic, payload);
		else {
			int nKafkaId = kafka_start_id + storeIndex;
                	int length = snprintf(NULL, 0, "%d", nKafkaId);
                	char* kafka_id = malloc((size_t)length + 1);
                	snprintf(kafka_id, (size_t)length+1, "%d", nKafkaId);
			send_avro_message_to_kafka(kafka_brokers, kafka_topic, hostName, kafka_id, kafka_tag, g_plugins[storeIndex]->lastChangeTimestamp, currTime, pluginName, g_plugins[storeIndex]->nextRunTimestamp, g_plugins[storeIndex]->description, g_plugins[storeIndex]->output.retString, pluginStatus, g_plugins[storeIndex]->statusChanged, g_plugins[storeIndex]->output.retCode);
		}
	}
        else {
		if (!kafkaAvro) {
			send_ssl_message_to_kafka(kafka_brokers, kafkaCACertificate, kafkaProducerCertificate, kafkaSSLKey, kafka_topic, payload);
		}
		else {
			int nKafkaId = kafka_start_id + storeIndex;
                	int length = snprintf(NULL, 0, "%d", nKafkaId);
                	char* kafka_id = malloc((size_t)length + 1);
                	snprintf(kafka_id, (size_t)length+1, "%d", nKafkaId);
        		send_ssl_avro_message_to_kafka(kafka_brokers, kafkaCACertificate, kafkaProducerCertificate, kafkaSSLKey, kafka_topic, hostName, kafka_id, kafka_tag, g_plugins[storeIndex]->lastChangeTimestamp, currTime, pluginName, g_plugins[storeIndex]->nextRunTimestamp, g_plugins[storeIndex]->description, g_plugins[storeIndex]->output.retString, pluginStatus, g_plugins[storeIndex]->statusChanged, g_plugins[storeIndex]->output.retCode);
		}
	}
	free(pluginName);
        free(pluginStatus);
        pluginName = NULL;
        pluginStatus = NULL;
        free(payload);
        payload = NULL;
}

void runPluginCommand(int index, char* command) {
	int prevRetCode = 0;
	clock_t ct;
	time_t t;
	//char currTime[22];
	char currTime[TIME_BUF_LEN];
	int rc = 0;

	if (strlen(command) > 100) {
		writeLog("Command longer than expected. Aborting run.", 1, 0);
		return;
	}
	prevRetCode = g_plugins[index]->output.retCode;
	ct = clock();
	time(&t);
	snprintf(infostr, infostr_size, "Running %s.", trim(command));
	writeLog(trim(infostr), 0, 0);
	TrackedPopen tp = tracked_popen(trim(command));
	if (tp.fp == NULL) {
		printf("Failed to run command\n");
		writeLog("Failed to run command.", 1, 0);
		// Update with failed run
		g_plugins[index]->output.retCode = 3;
		strncpy(g_plugins[index]->output.retString, "UNKNOWN: Failed to run command", pluginoutput_size);
		return;
	}
	add_plugin_pid(tp.pid);
        while (fgets(pluginReturnString, pluginmessage_size, tp.fp) != NULL) {
		// // VERBOSE  printf("%s", pluginReturnString);
	}
	rc = tracked_pclose(&tp);
        if (rc == -1) {
        	snprintf(infostr, infostr_size,
                     "[runPlugin] tracked_pclose failed: errno %d (%s)",
                     errno, strerror(errno));
        	writeLog(trim(infostr), 1, 0);
        }

	if (rc > 0) {
        	if (rc == 256)
        		g_plugins[index]->output.retCode = 1;
        	else if (rc == 512)
        		g_plugins[index]->output.retCode = 2;
        	else
        		g_plugins[index]->output.retCode = rc;
        }
        else
        	g_plugins[index]->output.retCode = rc;
	remove_plugin_pid(tp.pid);
	if (pluginReturnString != NULL && g_plugins[index]->output.retString != NULL) {
		char *trimmed = trim(pluginReturnString);
		size_t trimmed_len = strlen(trimmed);
		size_t copy_len = (trimmed_len < pluginoutput_size - 1) ? trimmed_len : pluginoutput_size - 1;
		//strncpy(g_plugins[index]->output.retString, trimmed, copy_len);
		snprintf(g_plugins[index]->output.retString, pluginoutput_size, "%s", trimmed);
		g_plugins[index]->output.retString[copy_len] = '\0';
	}
	size_t dest_size = 20;
        time_t tTime = time(NULL);
        struct tm tm = *localtime(&tTime);
	int tlen = snprintf(currTime, max_timestamp_size, "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon +1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	if (tlen >= dest_size) {
		writeLog("Possible truncation of timestamp in function 'runPluginCommand'.", 1, 0);
	}
	if (g_plugins[index]->output.prevRetCode != -1){
        	//snprintf(currTime, dest_size, "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon +1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
                if (prevRetCode != g_plugins[index]->output.retCode){
                	strcpy(g_plugins[index]->statusChanged, "1");
                	strcpy(g_plugins[index]->lastChangeTimestamp, currTime);
                }
                else {
                	strcpy(g_plugins[index]->statusChanged, "0");
                }
		strcpy(g_plugins[index]->lastRunTimestamp, currTime);
                time_t nextTime = t + (g_plugins[index]->interval * 60);
                struct tm tNextTime;
                memset(&tNextTime, '\0', sizeof(struct tm));
                localtime_r(&nextTime, &tNextTime);
                int len = snprintf(g_plugins[index]->nextRunTimestamp, dest_size, "%04d-%02d-%02d %02d:%02d:%02d", tNextTime.tm_year + 1900, tNextTime.tm_mon +1, tNextTime.tm_mday, tNextTime.tm_hour, tNextTime.tm_min, tNextTime.tm_sec);
		if (len >= dest_size) {
			writeLog("Possible truncation of timestamp in 'runPluginCommand'.", 1, 0);
		}
                g_plugins[index]->nextRun = nextTime;
                g_plugins[index]->output.prevRetCode = g_plugins[index]->output.retCode;
                if (timeScheduler) {
                	scheduler[0].timestamp = nextTime;
                }
       	}
       	else {
       		g_plugins[index]->output.prevRetCode = 0;
      	}
      	ct = clock() -ct;
        snprintf(infostr, infostr_size, "%s executed. Execution took %.0f milliseconds.\n", g_plugins[index]->name, (double)ct);
        writeLog(trim(infostr), 0, 0);
        if (logPluginOutput == true) {
                char* o_info;
                int o_info_size = pluginmessage_size + 195;
                o_info = malloc((size_t)o_info_size * sizeof(char));
                if (o_info == NULL) {
                        writeLog("Could not allocate memory for variable 'o_info'.", 2, 0);
			return;
                }
                snprintf(o_info, (size_t)o_info_size, "%s : %s", g_plugins[index]->name, pluginReturnString);
                writeLog(trim(o_info), 0, 0);
                free(o_info);
                o_info = NULL;
        }
	if (pluginResultToFile) {
		writePluginResultToFile(index, 0);
	}
	if (enableKafkaExport) {
                writeToKafkaTopic(index, 0);
	}
}

void runPluginOld(int storeIndex, int update) {
	char ch = '/';
	int prevRetCode = 0;
	clock_t ct;
	time_t t;
	//char currTime[22];
	char currTime[TIME_BUF_LEN];
	int rc = 0;
	char sPluginCommand[plugincommand_size];

	if (update > 0)
		prevRetCode = g_plugins[storeIndex]->output.retCode;
	ct = clock();
	time(&t);
	// Test local var
	//strcpy(sPluginCommand, pluginDir);
	//strncat(sPluginCommand, &ch, 1);
	//sPluginCommand[plugincommand_size -1] = '\0';
	snprintf(sPluginCommand, plugincommand_size, "%s%c", pluginDir, ch);
	if (update > 0) {
                strcat(sPluginCommand, update_g_plugins[storeIndex].command);
                snprintf(infostr, infostr_size, "Running: %s.", update_g_plugins[storeIndex].command);
        }
        else {
                strcat(sPluginCommand, g_plugins[storeIndex]->command);
                snprintf(infostr, infostr_size, "Running: %s.", g_plugins[storeIndex]->command);
        }
	writeLog(trim(infostr), 0, 0);
	TrackedPopen tp = tracked_popen(sPluginCommand);
	if (tp.fp == NULL) {
		printf("Failed to run command\n");
		writeLog("Failed to run comman via tracked_popen().", 2, 0);
		rc = -1;
	}
	else {
		add_plugin_pid(tp.pid);
		while (fgets(pluginReturnString, pluginmessage_size, tp.fp) != NULL) {
			// VERBOSE  printf("%s", pluginReturnString);
			// printf("DEBUG: %s\n", pluginReturnString);
		}
		rc = tracked_pclose(&tp);
		if (rc == -1) {
			snprintf(infostr, infostr_size, "[runPlugin] tracked_pclose failed with errno %d (%s)", errno, strerror(errno));
			writeLog(trim(infostr), 1, 0);
		}
		remove_plugin_pid(tp.pid);
	}
	switch (update) {
		case 0:
			if (rc > 0)
			{
				if (rc == 256)
					g_plugins[storeIndex]->output.retCode = 1;
				else if (rc == 512)
					g_plugins[storeIndex]->output.retCode = 2;
				else
					g_plugins[storeIndex]->output.retCode = rc;
			}
			else
				g_plugins[storeIndex]->output.retCode = rc;
			break;
		case 1:
			if (rc > 0) {
				if (rc == 256)
					printf("Depricated.\n");
				else if (rc == 512)
					printf("Depricated.\n");
				else
					printf("Depricated.\n");
			}
			else
				printf("Depricated.\n");			
			break;
		default:
			switch (rc) {
				case 256:
					g_plugins[storeIndex]->output.retCode = 1;
					//update_outputs[storeIndex].retCode = 1;
					break;
				case 512:
					g_plugins[storeIndex]->output.retCode = 1;
                                        //update_outputs[storeIndex].retCode = 1;
					break;
				default:
					g_plugins[storeIndex]->output.retCode = rc;
                                        //update_outputs[storeIndex].retCode = rc;
			}
	}
	//outout.retString size?
	if (update > 0){ 
		//update_outputs[storeIndex].retString = strdup(trim(pluginReturnString));
	}
	else {
		if (pluginReturnString != NULL && g_plugins[storeIndex]->output.retString != NULL){
			if (strlen(trim(pluginReturnString)) < pluginoutput_size) 
				strncpy(g_plugins[storeIndex]->output.retString, trim(pluginReturnString), pluginoutput_size);
			else {
				pluginReturnString[pluginoutput_size] = '\0';
				strncpy(g_plugins[storeIndex]->output.retString, trim(pluginReturnString), pluginoutput_size);
			}
		}
		else
			printf("WARNING: Want to write to variables that is freed. Is system closing?\n");
	}
	size_t dest_size = 20;
        time_t tTime = time(NULL);
        struct tm tm = *localtime(&tTime);
        int len = snprintf(currTime, dest_size, "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon +1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	if (len >= dest_size) {
		writeLog("Possible truncation of timestamp while running plugin.", 1, 0);
	}
	if (update == 0) {
		if (g_plugins[storeIndex]->output.prevRetCode != -1){
                	if (prevRetCode != g_plugins[storeIndex]->output.retCode){
				strcpy(g_plugins[storeIndex]->statusChanged, "1");
				strcpy(g_plugins[storeIndex]->lastChangeTimestamp, currTime);
				// Here something is wrong, it updates even if change is 0?
			}
			else {
				strcpy(g_plugins[storeIndex]->statusChanged, "0");
			}
			if (enableTimeTuner) {
				if (storeIndex == timeTunerMaster) {
					timeTunerCounter++;
					if (timeTunerCounter == timeTunerCycle) {
						timeTunerCounter = 0;
						// Get time diff
						char oldTime[20];
						struct tm time;
						strcpy(oldTime, g_plugins[timeTunerMaster]->lastRunTimestamp);
						strptime(oldTime, "%04d-%02d-%02d %02d:%02d:%02d", &time);
						time_t ttOldTime = 0, ttCurTime = 0;
						int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
						if (sscanf(oldTime, "%04d-%02d-%02d %02d:%02d:%02d", &year, &month, &day, &hour, &minute, &second) == 6) {
							struct tm breakdown = {0};
							breakdown.tm_year = year + 1900;
							breakdown.tm_mon = month - 1;
       							breakdown.tm_mday = day;
       							breakdown.tm_hour = hour;
       							breakdown.tm_min = minute;
							breakdown.tm_sec = second;

							if ((ttOldTime = mktime(&breakdown)) == (time_t)-1) {
          							fprintf(stderr, "Could not convert time input to time_t\n");
       							}
						}
						if (sscanf(currTime, "%04d-%02d-%02d %02d:%02d:%02d", &year, &month, &day, &hour, &minute, &second) == 6) {
                                                        struct tm breakdown = {0};
                                                        breakdown.tm_year = year + 1900;
                                                        breakdown.tm_mon = month - 1;
                                                        breakdown.tm_mday = day;
                                                        breakdown.tm_hour = hour;
                                                        breakdown.tm_min = minute;
                                                        breakdown.tm_sec = second;

                                                        if ((ttCurTime = mktime(&breakdown)) == (time_t)-1) {
                                                                fprintf(stderr, "Could not convert time input to time_t\n");
                                                        }
                                                }
						int difference = ttCurTime - ttOldTime - (g_plugins[timeTunerMaster]->interval * 60);
						// Apply time diff to all nextRuns :)
						timeTune(difference);
					}
				}
                        }
                	strcpy(g_plugins[storeIndex]->lastRunTimestamp, currTime);
                	time_t nextTime = t + (g_plugins[storeIndex]->interval * 60);
                	struct tm tNextTime;
                	memset(&tNextTime, '\0', sizeof(struct tm));
                	localtime_r(&nextTime, &tNextTime);
                	len = snprintf(g_plugins[storeIndex]->nextRunTimestamp, dest_size, "%04d-%02d-%02d %02d:%02d:%02d", tNextTime.tm_year + 1900, tNextTime.tm_mon +1, tNextTime.tm_mday, tNextTime.tm_hour, tNextTime.tm_min, tNextTime.tm_sec);
			if (len >= dest_size) {
				writeLog("Possible truncation of timestamp in 'runPlugin'.", 1, 0);
			}
			g_plugins[storeIndex]->nextRun = nextTime;
                	g_plugins[storeIndex]->output.prevRetCode = g_plugins[storeIndex]->output.retCode;
			if (timeScheduler) {
				scheduler[0].timestamp = nextTime;
			}
		}
		else {
	        	g_plugins[storeIndex]->output.prevRetCode = 0; 
		}
	}
	else {
		// If update = 1 use update_outputs
		// Will this be correct?
		/*if (prevRetCode != update_outputs[storeIndex].retCode){
                	strcpy(update_g_plugins[storeIndex].statusChanged, "1");
                        strcpy(update_g_plugins[storeIndex].lastChangeTimestamp, currTime);
                }
                else {
                	strcpy(update_g_plugins[storeIndex].statusChanged, "0");
                }*/
	}
	ct = clock() -ct;
	if (update == 0)
		snprintf(infostr, infostr_size, "%s executed. Execution took %.0f milliseconds.\n", g_plugins[storeIndex]->name, (double)ct);
	else
		snprintf(infostr, infostr_size, "%s executed. Execution took %.0f milliseconds.\n", update_g_plugins[storeIndex].name, (double)ct);
        writeLog(trim(infostr), 0, 0);
	if (logPluginOutput == true) {
		char* o_info;
		int o_info_size = pluginmessage_size + 195; 
		o_info = malloc((size_t)o_info_size * sizeof(char));
		if (o_info == NULL) {
			writeLog("Could not allocate memory for variable 'o_info'.", 2, 0);
		}
		if (update == 0)
			snprintf(o_info, (size_t)o_info_size, "%s : %s", g_plugins[storeIndex]->name, pluginReturnString);
		else
			snprintf(o_info, (size_t)o_info_size, "%s : %s", update_g_plugins[storeIndex].name, pluginReturnString);
		writeLog(trim(o_info), 0, 0);
		free(o_info);
		o_info = NULL;
	}
	if (pluginResultToFile) {
		writePluginResultToFile(storeIndex, update);
	}
	if (enableKafkaExport) {
		writeToKafkaTopic(storeIndex, update);
	}
}

void runGardener() {
	int rc = 0;

	TrackedPopen tp = tracked_popen(gardenerScript);
        if (tp.fp == NULL) {
                printf("Failed to run gardener script\n");
                writeLog("Failed to run gardener script.", 2, 0);
		rc = -1;
        }
	else {
		add_plugin_pid(tp.pid);
        	while (fgets(gardenerRetString, gardenermessage_size, tp.fp) != NULL) {
                	// VERBOSE  printf("%s", gardenerRetString);
		}
        	rc = tracked_pclose(&tp);
		if (rc == -1) {
			snprintf(infostr, infostr_size,
                     		"[runPlugin] tracked_pclose failed: errno %d (%s)",
                     		errno, strerror(errno));
            		writeLog(trim(infostr), 1, 0);
		}
		remove_plugin_pid(tp.pid);
        }
	snprintf(infostr, infostr_size, "Gardener script executed with return code %i.", rc);
        if (rc > 1) {
		writeLog(trim(infostr), 2, 0);
	}
	else writeLog(trim(infostr), rc, 0);
}

void runClearDataCache() {
	DIR *d = NULL;
	struct dirent *dir;
	d = opendir(dataDir);
	if (d) {
		while ((dir = readdir(d)) != NULL) {
			if (dir->d_type == DT_REG) {
				char buf[1024];
				struct stat filestat;
				sprintf(buf, "%s/%s", dataDir, dir->d_name);
				stat(buf, &filestat);
                                snprintf(infostr, infostr_size, "ClearDataCash checking file: %s", dir->d_name);
                                writeLog(trim(infostr), 0, 0);
				// Now check time 
				time_t now = time(NULL);
				// HERE Set current time to timestamp!!!
				time_t ftime = filestat.st_ctime + dataCacheTimeFrame;
				if (now > ftime) {
                                        snprintf(infostr, infostr_size, "ClearDataCash remove file: %s", dir->d_name);
                                        writeLog(trim(infostr), 0, 0);
					remove(buf);
				}
			}
		}
		closedir(d);
	}
}

void* pluginExeThread(void* data) {
	/*sigset_t sigset;
    	sigemptyset(&sigset);
    	sigaddset(&sigset, SIGCHLD);
    	pthread_sigmask(SIG_BLOCK, &sigset, NULL);*/
	intptr_t storeIndex = (intptr_t)data;
	pthread_detach(pthread_self());
	// VERBOSE printf("Executing %s in pthread %lu\n", g_plugins[storeIndex].description, pthread_self());
	pthread_mutex_lock(&mtx);
	threadIds[(short)storeIndex] = 1;
        PluginItem *pi = getPluginItem(storeIndex);
	run_plugin(pi);
	if (timeScheduler) {
        	for (size_t i = 0; i < decCount; i++) {
                	if (scheduler[i].id == storeIndex) {
                        	scheduler[i].timestamp = g_plugins[storeIndex]->nextRun;
                        	//printf("Updated scheduler[%zu] for plugin_id %ld\n", i, storeIndex);
                        	break;
                	}
        	}
		rescheduleChecks();
	}

	//runPlugin(storeIndex, 0);
	thread_counter--;
	pthread_mutex_unlock(&mtx);
        threadIds[(short)storeIndex] = 0;
	pthread_exit(NULL);
	total_threads_run++;
}

void* gardenerExeThread(void* data) {
 	/*sigset_t sigset;
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGCHLD);
        pthread_sigmask(SIG_BLOCK, &sigset, NULL);*/
	pthread_detach(pthread_self());
	runGardener();
	pthread_mutex_lock(&mtx);
	thread_counter--;
	pthread_mutex_unlock(&mtx);
	pthread_exit(NULL);
	total_threads_run++;
}

void* clearDataCacheThread(void* data) {
	/*sigset_t sigset;
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGCHLD);
        pthread_sigmask(SIG_BLOCK, &sigset, NULL);*/
	pthread_detach(pthread_self());
	runClearDataCache();
	pthread_mutex_lock(&mtx);
	thread_counter--;
	pthread_mutex_unlock(&mtx);
	pthread_exit(NULL);
	total_threads_run++;
}

int countDeclarations(char *file_name) {
	FILE *fp = NULL;
	int i = 0;
        int ch;

	if (file_name == NULL || strlen(file_name) == 0) {
		writeLog("Filename is not initialized or is empty.", 2, 0);
		fprintf(stderr, "Filename is uninitialized or empty.\n");
	}
        fp = fopen(file_name, "r");
	if (fp == NULL)
        {
                perror("Error while opening the file[countDeclarations].\n");
		writeLog("Error opening and counting g_plugins file.", 2, 0);
                exit(EXIT_FAILURE);
        }
        while ((ch = fgetc(fp)) != EOF) {
		if (ch == '\n')
			i++;
	}
	fclose(fp);
	fp = NULL;
	return i-1;
}

/*int loadPluginDeclarations(const char *configFile, int reload) {
	FILE *fp = fopen(configFile, "r");
    	if (!fp) {
        	writeLog("Cannot open plugin g_plugins file", 2, 0);
        	return -1;
    	}

    	char *line = NULL;
    	size_t len = 0;
    	ssize_t read;
    	int count = 0;
    	int lineno = 0;

    	while ((read = getline(&line, &len, fp)) != -1) {
        	lineno++;
        	char *trimmed = trim_line(line);
        	if (*trimmed == '\0' || *trimmed == '#')
            		continue;  

        	if (count >= MAX_DECLS) {
            		writeLog("Too many g_plugins, skipping rest", LOG_LEVEL_WARN, 0);
            		break;
        	}

        	if (parseLine(trimmed, &g_plugins[count], count, lineno)) {
            		snprintf(infostr, sizeof(infostr),"Loaded declaration [%s] (id=%d)",g_plugins[count].name, count);
            		writeLog(infostr, 0, 0);
            		count++;
        	}
    	}

    	free(line);
    	fclose(fp);

	return count;
}*/

/*int loadPluginDeclarations(const char *pluginDeclarationsFile, int reload) {
    	int counter      = 0;
    	int i, index     = 0;
    	int ret          = 0;     // return code, 0 on success, <0 on error
    	char *line       = NULL;
    	char *linecopy   = NULL;
    	size_t len       = 0;
    	ssize_t read;
    	FILE *fp         = NULL;

    	fp = fopen(pluginDeclarationsFile, "r");
    	if (!fp) {
        	writeLog("Error opening plugin g_plugins file.", 2, 0);
        	ret = -1;
        	goto cleanup;
    	}

    	while ((read = getline(&line, &len, fp)) != -1) {
        	index++;
        	if (strchr(line, '#')) {
            		// comment or empty → skip
            		continue;
        	}

        	linecopy = strdup(line);
        	if (!linecopy) {
            		writeLog("Failed to duplicate line", 2, 0);
            		ret = -2;
            		goto cleanup;
        	}

        	{
           		char *token      = NULL;
            		char *name       = NULL;
            		char *saveptr    = NULL;
            		int   parsingErr = 0;

            		for (i = 1; ; i++) {
                		token = strtok_r(i == 1 ? linecopy : NULL, ";", &saveptr);
                		if (!token) 
                    			break;

                		switch (i) {
                  			case 1:
                    				name = strtok(token, " ");
                    				if (!name) {
                        				parsingErr = 1;
                        				break;
                    				}
                    				{
                        				char *desc = strtok(NULL, "?");
                        				if (!reload) {
                            					free(g_plugins[counter].name);
                            					g_plugins[counter].name = strdup(name);
                            					free(g_plugins[counter].description);
                            					g_plugins[counter].description = desc 
                                					? strdup(desc) 
                                					: NULL;
                        				} else {
                            					free(update_g_plugins[counter].name);
                            					update_g_plugins[counter].name = strdup(name);
                            					if (desc) {
                                					free(update_g_plugins[counter].description);
                                					update_g_plugins[counter].description = strdup(desc);
                            					} else {
                                					parsingErr = 1;
                            					}
                        				}
                    				}
                    				break;
                  			case 2:
                    				if (strlen(token) < 5) {
                        				parsingErr = 1;
                        				break;
                    				}
                    				if (!reload) {
                        				free(g_plugins[counter].command);
                        				g_plugins[counter].command = strdup(token);
                    				} else {
                        				free(update_g_plugins[counter].command);
                        				update_g_plugins[counter].command = strdup(token);
                    				}
                    				break;
                  			case 3:
                    				if (!reload)
                        				g_plugins[counter].active = atoi(token);
                    				else
                        				update_g_plugins[counter].active = atoi(token);
                    				break;
                  			case 4:
                    				if (!reload) {
                        				g_plugins[counter].interval = atoi(token);
                        				g_plugins[counter].id = index - 1;
                    				} else {
                        				update_g_plugins[counter].interval = atoi(token);
                        				update_g_plugins[counter].id = index - 1;
                    				}
                    				break;
                  			default:
                    				break;
                		}

                		if (parsingErr)
                    			break;
            		}  // end for‐token loop
            		free(linecopy);
            		linecopy = NULL;
            		if (parsingErr) {
                		continue;
            		}
        	}
        	if (!reload) {
            		g_plugins[counter].lastRunTimestamp[0]     = '\0';
            		g_plugins[counter].nextRunTimestamp[0]    = '\0';
            		g_plugins[counter].lastChangeTimestamp[0] = '\0';
            		g_plugins[counter].statusChanged[0]       = '\0';
        	} else {
            		update_g_plugins[counter].lastRunTimestamp[0]     = '\0';
            		update_g_plugins[counter].nextRunTimestamp[0]    = '\0';
            		update_g_plugins[counter].lastChangeTimestamp[0] = '\0';
            		update_g_plugins[counter].statusChanged[0]       = '\0';
        	}
        	snprintf(infostr, infostr_size,"Declaration with index %d is created.\n", counter);
        	writeLog(trim(infostr), 0, 0);
        	counter++;
    	}

	cleanup:
    		if (linecopy) free(linecopy);
    		if (line) free(line);
    		if (fp) {
        		fclose(fp);
        		fp = NULL;
    		}

   	return (ret == 0 ? counter : ret);
}*/

void copyPluginItem(PluginItem *dest, const PluginItem *src, int mode) {
    if (!dest || !src) return;  // Defensive check

    if (mode == 0) {
        if (src->name != NULL) {
            snprintf(dest->lastRunTimestamp, max_timestamp_size, "%s", src->lastRunTimestamp);
            snprintf(dest->nextRunTimestamp, max_timestamp_size, "%s", src->nextRunTimestamp);
            snprintf(dest->lastChangeTimestamp, max_timestamp_size, "%s", src->lastChangeTimestamp);
            snprintf(dest->statusChanged, 2, "%s", src->statusChanged);
            dest->active = src->active;
            dest->interval = src->interval;
            dest->nextRun = src->nextRun;
        } else {
            writeLog("copyPluginItem[src->name] is empty. Do not copy.", 0, 0);
        }
    } else if (mode == 2) {
        snprintf(dest->lastRunTimestamp, max_timestamp_size, "%s", src->lastRunTimestamp);
        snprintf(dest->nextRunTimestamp, max_timestamp_size, "%s", src->nextRunTimestamp);
        snprintf(dest->statusChanged, 2, "%s", src->statusChanged);
        dest->nextRun = src->nextRun;
    } else {
        snprintf(dest->name, pluginitemname_size + 1, "%s", src->name);
        snprintf(dest->description, pluginitemdesc_size + 1, "%s", src->description);
        snprintf(dest->command, pluginitemcmd_size + 1, "%s", src->command);
        snprintf(dest->lastRunTimestamp, max_timestamp_size, "%s", src->lastRunTimestamp);
        snprintf(dest->nextRunTimestamp, max_timestamp_size, "%s", src->nextRunTimestamp);
        snprintf(dest->lastChangeTimestamp, max_timestamp_size, "%s", src->lastChangeTimestamp);
        snprintf(dest->statusChanged, 2, "%s", src->statusChanged);
        dest->active = src->active;
        dest->interval = src->interval;
        dest->nextRun = src->nextRun;
    }
}

void plugin_output_init(PluginOutput *o) {
	if (!o) return;
    	o->retCode     = 0;
    	o->prevRetCode = 0;
    	o->retString   = NULL;
}

void plugin_output_destroy(PluginOutput *o) {
    	if (!o) return;
    	free(o->retString);
    	o->retString = NULL;
}

int plugin_output_set(PluginOutput *dest, const PluginOutput *src) {
	size_t len;
	char *dup;

    	if (!dest || !src) return EINVAL;

    	dest->retCode     = src->retCode;
    	dest->prevRetCode = src->prevRetCode;

    	plugin_output_destroy(dest);

    	if (!src->retString) {
        	return 0;
    	}

    	//dest->retString = strdup(src->retString);
    	/*if (!dest->retString) {
        	writeLog("[plugin_output_set] strdup failed", 1, 0);
        	return ENOMEM;
    	}*/
	len = strlen(src->retString);
	dup = malloc(len +1);
	if (!dup) {
		writeLog("[plugin_output_set] malloc failed", 1, 0);
        	return ENOMEM;
	}
	memcpy(dup, src->retString, len + 1);
	dest->retString = dup;

    	return 0;
}

void destroy_g_plugins(PluginItem *decls, size_t count) {
	if (!decls) return;
	for (size_t i = 0; i < count; i++) {
		free(decls[i].name);
		free(decls[i].description);
		free(decls[i].command);
	}
	free(decls);
	decls = NULL;
}

int redeclarePluginDeclarations(int mode, int count) {
	//int c;
	//int rows = 0;
	int check = 0;

	writeLog("Needs to redeclare g_plugins.", 0, 0);
	check = check_plugin_conf_file(pluginDeclarationFile);
	if (check > 0) {
		writeLog("Errors detected in plugin file. Can not reload.", 1, 0);
	       	return 2;	
	}
	else
		writeLog("Plugin conf file seems in good state. Will try to reload it now.", 0, 0);
	update_plugins();
	flushLog();

	return 0;
}

void checkRetVal(int val) {
	if (val > 1) {
		printf("Caught memory problem redeclaring plugin variables.\nQuiting...");
                writeLog("Memory allocation error redeclaring plugins.", 2, 0);
                writeLog("Check your configs if needed, then restart me.", 0, 0);
                flushLog();
                sig_handler(SIGSTOP);
        }
}

int hardReloadPlugins(int cnt) {
        int qsv = quick_start;

        if (quick_start != 1) quick_start = 1;

        /* free previous plugin items and their per-item allocations */
        if (g_plugins != NULL) {
                for (size_t i = 0; i < cnt; ++i) {
                        if (g_plugins[i]) {
                                free(g_plugins[i]->name);
                                free(g_plugins[i]->description);
                                free(g_plugins[i]->command);
                                free(g_plugins[i]->output.retString);
                                free(g_plugins[i]);
                        }
                }
                free(g_plugins);
                g_plugins = NULL;
        }

        /* allocate array of pointers */
        decCount = countDeclarations(pluginDeclarationFile);
        g_plugins = calloc((size_t)decCount, sizeof(PluginItem *));
        if (!g_plugins) {
                perror("Error allocating memory for g_plugins array");
                writeLog("Error allocating memory [g_plugins array]", 2, 0);
                abort();
                return 2;
        }

        declaration_size = (size_t)decCount;

        /* allocate each PluginItem and its per-item strings including embedded output */
        for (int i = 0; i < decCount; ++i) {
                g_plugins[i] = calloc(1, sizeof(PluginItem));
                if (!g_plugins[i]) {
                        fprintf(stderr, "Error allocating PluginItem %d\n", i);
                        writeLog("Error allocating PluginItem", 2, 0);
                        for (int j = 0; j < i; ++j) {
                                free(g_plugins[j]->name);
                                free(g_plugins[j]->description);
                                free(g_plugins[j]->command);
                                free(g_plugins[j]->output.retString);
                                free(g_plugins[j]);
                        }
                        free(g_plugins);
                        g_plugins = NULL;
                        abort();
                        return 2;
                }
                g_plugins[i]->name = calloc(pluginitemname_size + 1, 1);
                g_plugins[i]->description = calloc(pluginitemdesc_size + 1, 1);
                g_plugins[i]->command = calloc(pluginitemcmd_size + 1, 1);
                g_plugins[i]->output.retString = calloc(pluginoutput_size + 1, 1);
		if (!g_plugins[i]->name || !g_plugins[i]->description || !g_plugins[i]->command || !g_plugins[i]->output.retString) {
                        fprintf(stderr, "Error allocating memory while redeclaring plugins (item %d).\n", i);
                        writeLog("Error allocating memory [update_g_plugins::items].", 2, 0);

                        for (int j = 0; j <= i; ++j) {
                                if (g_plugins[j]) {
                                        free(g_plugins[j]->name);
                                        free(g_plugins[j]->description);
                                        free(g_plugins[j]->command);
                                        free(g_plugins[j]->output.retString);
                                        free(g_plugins[j]);
                                }
                        }
                        free(g_plugins);
                        g_plugins = NULL;
                        abort();
                        return 2;
                }
        }

        /* reload declarations */
        if (check_plugin_conf_file(pluginDeclarationFile) != 0) {
                writeLog("plugins.conf file seems to be corrupt. Program will shut down.", 2, 0);
                return 2;
        }
        if (threadIds != NULL) {
                free(threadIds);
                threadIds = NULL;
        }
        threadIds = (unsigned short*)malloc((size_t)MAX_PLUGINS * sizeof(unsigned short));
        memset(threadIds, 0, MAX_PLUGINS * sizeof(unsigned short));
        for (int i = 0; i < decCount; i++) {
                threadIds[i] = 0;
        }
        g_current_scheduler_cnt = decCount;
        checkPluginFileStat(pluginDeclarationFile, tPluginFile, 0);
        writeLog("No errors found in plugins.conf", 0, 0);
        if (init_plugins() != 0) {
                logError("Failed to initiate plugins", 2, 0);
                flushLog();
                return 2;
        }
        flushLog();
        // Remove schededuler?
        initScheduler(cnt, 1000, true);
        quick_start = qsv;
        return 0;
}

void apiReloadConfigHard() {
	if (check_plugin_conf_file(pluginDeclarationFile) != 0) {
		constructSocketMessage("reloadpluginshard", "failed");
        }
       	else {
		if (hardReloadPlugins(decCount) == 0)
			constructSocketMessage("reloadpluginshard", "success");
		else {
			constructSocketMessage("reloadpluginshard", "fatal");
			sig_handler(SIGSTOP);
		}
	}
}

int checkNewConfig(const char *file_name) {
	FILE *file = NULL;
	char line[512];
        int count = 0;
	char identifier[256];
	char identifiers[150][256] = {0};
	int identifierCount = 0;
	int copies;
	//char c;
	int ch;	

        file = fopen(file_name, "r");
        if (file == NULL)
        {
                perror("Error while opening the file.[checkNewConfig]\n");
                writeLog("Error opening and counting g_plugins file.", 2, 0);
		return -1;
        }

	while (fgets(line, sizeof(line), file)) {
		if (sscanf(line,"[%[^]]]", identifier) == 1) {
			strncpy(identifiers[identifierCount], identifier, sizeof(line));
            		identifierCount++;
        	}
    	}
  	for (int i = 0; i < identifierCount; ++i) {
		copies = 0;
		for (int j = 0; j < identifierCount; j++) {
                    if (strcmp(identifiers[i], identifiers[j]) == 0)
                            copies++;
            	}
		if (copies > 1) {
			writeLog("There are duplicates in plugins.conf. Will abort reloading.", 1, 0);
			writeLog("The plugin file contains duplicates.", 2, 0);
			return -1;
		}
	}
	rewind(file);
	/*for (c = getc(file); c != EOF; c = getc(file)){
                if (c == '\n')
                        count++;
        }*/
	while ((ch = fgetc(file)) != EOF) {
                if (ch == '\n')
                        count++;
        }
        fclose(file);
	file = NULL;
        return count-1;
}

void initNewPlugin(int index) {
	//char currTime[80];
	/*char currTime[TIME_BUF_LEN];
	snprintf(infostr, infostr_size, "Initiating new plugin: %s\n", update_g_plugins[index].name);
	writeLog(trim(infostr), 0, 0);
	printf("Initiating new plugin with id %d\n", index);
	if (update_g_plugins[index].active == 1) {
		snprintf(infostr, infostr_size, "%s is now active. Id %d\n", update_g_plugins[index].name, update_g_plugins[index].id-1);
		writeLog(trim(infostr), 0, 0);
		update_outputs[index].prevRetCode = -1;
		//strcpy(update_g_plugins[index].statusChanged, "0");
		snprintf(update_g_plugins[index].statusChanged, 2, "%s", "0");
		//runPlugin(index, 1);
		PluginItem *item = g_plugins[index];
        	if (item && item->active) {
            		run_plugin(item);
        	}
		if (timeScheduler)
			rescheduleChecks();
		size_t dest_size = 20;
                time_t t = time(NULL);
                struct tm tm = *localtime(&t);
                int plen = snprintf(currTime, dest_size, "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon +1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
		if (plen >= dest_size) {
			writeLog("Possible truncation of timestamp while init new plugin.", 1, 0);
		}
                strcpy(update_g_plugins[index].lastRunTimestamp, currTime);
                strcpy(update_g_plugins[index].lastChangeTimestamp, currTime);
                time_t nextTime = t + (update_g_plugins[index].interval *60);
                struct tm tNextTime;
                memset(&tNextTime, '\0', sizeof(struct tm));
                localtime_r(&nextTime, &tNextTime);
                int len = snprintf(update_g_plugins[index].nextRunTimestamp, dest_size, "%04d-%02d-%02d %02d:%02d:%02d", tNextTime.tm_year + 1900, tNextTime.tm_mon +1, tNextTime.tm_mday, tNextTime.tm_hour, tNextTime.tm_min, tNextTime.tm_sec);
		if (len >= dest_size) {
			writeLog("[initNewPlugin] possible truncation of timestamp.", 1, 0);
		}
                update_g_plugins[index].nextRun = nextTime;
		usleep(500);
	}
	else
        {
        	snprintf(infostr, infostr_size, "%s is not active. Id: %d\n", update_g_plugins[index].name, update_g_plugins[index].id);
        	writeLog(trim(infostr), 0, 0);
        }*/
        flushLog();
}

int initTimeScheduler(bool reinit) {
	if (decCount == 0) {
		scheduler = NULL;
		printf("Could not initiate a time scheduler of count %d.\n", decCount);
		return 1;
	}
	//scheduler = malloc((size_t)sizeof(Scheduler)*decCount);
	scheduler = calloc(decCount, sizeof(Scheduler));
	if (!scheduler) {
        	printf("Error allocating memory");
        	writeLog("Error allocating memory [initTimeScheduler]", 2, 0);
        	abort();
       		return 2;
        }
	if (reinit) {
		// Populate scheduler
                for (int i = 0; i < g_current_scheduler_cnt; i++) {
                        scheduler[i].id = g_plugins[i]->id;
                        scheduler[i].timestamp = g_plugins[i]->nextRun;
                }
        }
	return 0;
}

void initScheduler(int numOfP, int msSleep, bool reinit) {
	char currTime[TIME_BUF_LEN];
	time_t nextTime;
	float sleepTime = msSleep/1000;
	if (reinit)
                writeLog("Reinitate scheduler to run checks att given intervals", 0, 0);
        else
                logInfo("Initiating scheduler to run checks att given intervals.", 0, 0);
	if (timeScheduler) {
		if (!reinit) {
                        logInfo("Initiating a time scheduler.", 0, 0);
                        initTimeScheduler(false);
                }
                else {
                        writeLog("Reinitate time scheduler.", 0, 0);
                        initTimeScheduler(true);
                }
	}
	flushLog();
	for (int i = 0; i < numOfP; i++)
	{
		if (g_plugins[i]->active == 1)
		{
			snprintf(infostr, infostr_size, "%s is active. Id %d\n", g_plugins[i]->name, g_plugins[i]->id);
			writeLog(trim(infostr), 0, 0);
			//outputs[i].prevRetCode = -1;
			g_plugins[i]->output.prevRetCode = -1;
			snprintf(g_plugins[i]->statusChanged, 2, "%s", "0");
                        
			PluginItem *item = g_plugins[i];
        		if (item) {
            			run_plugin(item);
        		}
			//runPlugin(i, 0);
			size_t dest_size = 20;
			time_t t = time(NULL);
  			struct tm tm = *localtime(&t);
			int len = snprintf(currTime, dest_size, "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon +1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
			if (len >= dest_size) {
				writeLog("[InitScheduler] possible truncation of timestamp.", 1, 0);
			}
			strcpy(g_plugins[i]->lastRunTimestamp, currTime);
			strcpy(g_plugins[i]->lastChangeTimestamp, currTime);
			if (quick_start) {
				int add_time = (int)sleepTime;
				int time_to_add = add_time * i+1;
				nextTime = t + (g_plugins[i]->interval * 60) + time_to_add;
			}
			else {
				nextTime = t + (g_plugins[i]->interval *60);
			}
			struct tm tNextTime;
			memset(&tNextTime, '\0', sizeof(struct tm));
			localtime_r(&nextTime, &tNextTime);
			len = snprintf(g_plugins[i]->nextRunTimestamp, dest_size, "%04d-%02d-%02d %02d:%02d:%02d", tNextTime.tm_year + 1900, tNextTime.tm_mon +1, tNextTime.tm_mday, tNextTime.tm_hour, tNextTime.tm_min, tNextTime.tm_sec);
			if (len >= dest_size) {
				writeLog("[Init scheduler] Possible truncation at nextTimeRuntimestamp", 1, 0);
			}
			g_plugins[i]->nextRun = nextTime;
			if (timeScheduler) {
				scheduler[i].id = i;
				scheduler[i].timestamp = nextTime;
			}
			if (!quick_start)
				sleep(sleepTime);
		}
		else
		{
			snprintf(infostr, infostr_size, "%s is not active. Id: %d\n", g_plugins[i]->name, g_plugins[i]->id);
			writeLog(trim(infostr), 0, 0);
			if (timeScheduler) {
				scheduler[i].id = i;
				scheduler[i].timestamp = 0;
			}
		}
		flushLog();
	}
	if (!standalone) {
		switch (output_type) {
			case JSON_OUTPUT:
				collectJsonData(numOfP);
				break;
			case METRICS_OUTPUT:
				collectMetrics(numOfP, 0);
				break;
			case JSON_AND_METRICS_OUTPUT:
		       		collectJsonData(numOfP);
		       		collectMetrics(numOfP, 0);
		       		break;
			case PROMETHEUS_OUTPUT:
		       		collectMetrics(numOfP, 1);
		       		break;
			case JSON_AND_PROMETHEUS_OUTPUT:
		      		collectJsonData(numOfP);
		     		collectMetrics(numOfP, 1);
				break;
	        	default:
				collectJsonData(numOfP);
		}
	}	
        tnextGardener = time(0) + gardenerInterval;	
	tnextClearDataCache = time(0) + clearDataCacheInterval;
	if (local_api && !reinit) {
		if (use_ssl)
			 SSL_library_init();
		if (socket_is_ready == 1) {
			writeLog("Socket is already happy.", 0, 0);
			return;
		}
		if (initSocket() == SOCKET_READY) {
			startApiSocket();
		}
		else {
			writeLog("Continue without local api.", 0, 0);
		}
	}
	if (timeScheduler) {
		checkSchedulerCount();
		qsort(scheduler, decCount, sizeof(struct Scheduler), compare_timestamps);
	}
	if (runGardenerAtStart && !reinit) {
		writeLog("Running gardener cleanup job", 0, 0);
		runGardener();
	}
	if (reinit)
                writeLog("Scheduler reinitialized.", 0, 0);
        else
                logInfo("Scheduler initialized.", 0, 0);
    	flushLog();
}

void startPluginThread(int plugin_id) {
	int rc;
	pthread_t thread_id;
	intptr_t vpid = (intptr_t)plugin_id;

	vpid = plugin_id;

	rc = pthread_create(&thread_id, NULL, pluginExeThread, (void *)vpid);
	if(rc) {
		snprintf(infostr, infostr_size, "Error: return code from phtread_create is %d\n", rc);
		writeLog(trim(infostr), 2, 0);
	}
	else {
		snprintf(infostr, infostr_size, "Created new thread (%lu) for plugin %s\n", thread_id, g_plugins[plugin_id]->name);
		writeLog(trim(infostr), 0, 0);
		total_threads_run++;
		pthread_mutex_lock(&mtx);
		thread_counter++;
		pthread_mutex_unlock(&mtx);
		pthread_join(thread_id, NULL);
        }
}

void runPluginThreads(int loopVal){
	char currTime[TIME_BUF_LEN];
	pthread_t thread_id;
        int rc;
        int i;
	time_t t = time(NULL);
        struct tm tm = *localtime(&t);

	snprintf(currTime, sizeof(currTime), "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon +1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

	/*if (timeScheduler == 1) {
		i = 1;
		struct Scheduler do_run = scheduler[0];
		while(i > 0) {
			if ((t >= do_run.timestamp) && (g_plugins[do_run.id]->active == 1)) {
				//printf("DEBUG: startPluginThread id %d\n", do_run.id);
				startPluginThread(do_run.id);
				tspr++;
			}
			if (do_run.timestamp > t) {
				//printf("Exit..\n");
				break;
			}
			do_run = scheduler[0];
		}
		return;
	}*/
	if (timeScheduler) {
                time_t t = time(NULL);
                int currentId = -1;
                time_t currentTimestamp = 0;

                while (scheduler[0].timestamp <= t) {
                        struct Scheduler do_run = scheduler[0];

                        // Prevent infinite loop on same plugin and timestamp
                        if ((currentId == do_run.id) && (currentTimestamp == do_run.timestamp)) {
                                //printf("Loop protection triggered for id %d. Sleeping...\n", do_run.id);
				snprintf(infostr, infostr_size, "Loop protextion triggered for id %d. Sleeping...\n", do_run.id);
				writeLog(trim(infostr), 0, 0);
                                sleep(1);
                                break;
                        }

                        if (g_plugins[do_run.id]->active == 1) {
                                //printf("DEBUG: startPluginThread id %d\n", do_run.id);
                                startPluginThread(do_run.id);
                                tspr++;
                                currentId = do_run.id;
                                currentTimestamp = do_run.timestamp;
                                //printf("After reschedule: scheduler[0].id = %d, timestamp = %ld\n", scheduler[0].id, scheduler[0].timestamp);
                        }
                        // Loop continues as long as scheduler[0].timestamp <= t
                }
                return;
        }

        for (i = 0; i < loopVal; i++) {
           long j = i;
	   if (g_plugins[i]->active == 1) {
		if (t > g_plugins[i]->nextRun)
		{
			rc = pthread_create(&thread_id, NULL, pluginExeThread, (void *)j);
           		if(rc) {
                		snprintf(infostr, infostr_size, "Error: return code from phtread_create is %d\n", rc);
				writeLog(trim(infostr), 2, 0);
           		}
           		else {
                   		snprintf(infostr, infostr_size, "Created new thread (%lu) for plugin %s\n", thread_id, g_plugins[i]->name);
				writeLog(trim(infostr), 0, 0);
				total_threads_run++;
				pthread_mutex_lock(&mtx);
				thread_counter++;
				pthread_mutex_unlock(&mtx);
				//pthread_join(thread_id, NULL);
           		}
		}
            }
	}
        //pthread_exit(NULL);
}

void executeGardener() {
	pthread_t thread_id;
	int rc;

	rc = pthread_create(&thread_id, NULL, gardenerExeThread, "gardener 1");
	if(rc) {
		snprintf(infostr, infostr_size, "Error: return code from phtread_create is %d\n", rc);
               	writeLog(trim(infostr), 2, 0);
		return;
        }
	//pthread_setname_np(thread_id, "Gardener worker");
	pthread_setspecific(thread_id, "Gardener worker");
	snprintf(infostr, infostr_size, "Created new thread (%lu) truncating metrics logs (gardener) \n", thread_id);
        writeLog(trim(infostr), 0, 0);
	pthread_mutex_lock(&mtx);
	thread_counter++;
	pthread_mutex_unlock(&mtx);
}

void clearDataCache() {
	pthread_t thread_id;
	int rc;

	rc = pthread_create(&thread_id, NULL, clearDataCacheThread, "clearDataCache 1");
	      if(rc) {
                snprintf(infostr, infostr_size, "Error: return code from phtread_create is %d\n", rc);
                writeLog(trim(infostr), 2, 0);
        }
        else {
		//pthread_setname_np(thread_id, "DataClearCache");
		pthread_setspecific(thread_id, "DataClearCache");
                snprintf(infostr, infostr_size, "Created new thread (%lu) clearing old data files (clearDataCache) \n", thread_id);
                writeLog(trim(infostr), 0, 0);
		pthread_mutex_lock(&mtx);
		thread_counter++;
		pthread_mutex_unlock(&mtx);
		pthread_join(thread_id, NULL);
       }
}

void apiReloadConfigSoft() {
	if (check_plugin_conf_file(pluginDeclarationFile) != 0) {
                constructSocketMessage("softreloadplugins", "failed");
        }
        else {
                //updatePluginDeclarations();
		update_plugins();
                constructSocketMessage("softreloadplugins", "success");
        }
}

void scheduleChecks(){
	float sleepTime = schedulerSleep/1000;
	int i = 1;
	int repeate_write = 0;

	logInfo("Almond started succesfully. Ready to schedule checks.", 0, 0);
	if (timeScheduler) {
		writeLog("Start time based scheduler...", 0, 0);
	}
	else {
		writeLog("Start classic scheduler timer...", 0, 0);
		snprintf(infostr, infostr_size, "Sleep time is: %.3f\n", sleepTime);
		writeLog(trim(infostr), 0, 0);
	}
	flushLog();
	// Timer is an eternal loop :P
	while (i > 0) {
		if (is_stopping != 0) i--;
		if (!timeScheduler)
			writeLog("Check for command files.", 0, 0);
		else {
			if (repeate_write == 0) {
				writeLog("Check for command files.", 0, 0);
				repeate_write++;
			}
		}
		checkApiCmds();
		if (!external_scheduler) {
			runPluginThreads(decCount);
		}
		if (!timeScheduler) {
			snprintf(infostr, infostr_size, "Sleeping for %.3f seconds.\n", sleepTime);
                	writeLog(trim(infostr), 0, 0);
			sleep(sleepTime);
		}
		else {
			checkSchedulerCount();
			qsort(scheduler, decCount, sizeof(struct Scheduler), compare_timestamps);
			//writeLog("VERBOSE: Scheduler sorted. Sleeping for a second.", 0, 0);
			sleep(1);
		}
		if (!timeScheduler || tspr > 0) {
			tspr = 0;
			repeate_write = 0;
			switch (output_type) {
                		case JSON_OUTPUT:
                        		collectJsonData(decCount);
                        		break;
                		case METRICS_OUTPUT:
                        		collectMetrics(decCount, 0);
                        		break;
                		case JSON_AND_METRICS_OUTPUT:
                       			collectJsonData(decCount);
					collectMetrics(decCount, 0);
                       			break;
				case PROMETHEUS_OUTPUT:
					collectMetrics(decCount, 1);
					break;
				case JSON_AND_PROMETHEUS_OUTPUT:
					collectJsonData(decCount);
                                	collectMetrics(decCount, 1);
					break;
                		default:
                        		collectJsonData(decCount);
        		}
		}
		// Set this to timestamp
		if (checkPluginFileStat(pluginDeclarationFile, tPluginFile, 0)) {
			writeLog("Detected change of plugins file.", 0, 0);
			flushLog();
			//updatePluginDeclarations();
                        update_plugins();
                        printf("Plugins updated. Total live plugins: %d\n", g_plugin_count);
		}
		// Time to execute gardener?
		if (enableGardener) {
			time_t seconds = time(0);
			if (seconds > tnextGardener) {
				sleep(10);
				executeGardener();
				tnextGardener = seconds + gardenerInterval;
				sleep(1);
			}

		}
		if (enableClearDataCache) {
			time_t seconds = time(0);
			if (seconds > tnextClearDataCache) {
                                writeLog("ClearDataCash is ready", 0, 0);
				clearDataCache();
				tnextClearDataCache = seconds + clearDataCacheInterval;
				sleep(5);
			}
		}
		if (use_push || use_metrics_push) {
			push_interval_cnt++;
			int sleep_push_interval = push_interval;
			if (!timeScheduler)
				sleep_push_interval = push_interval / sleepTime;
			if (push_interval_cnt >= sleep_push_interval) {
				char url[1024];
				// Path added if future version would like such an extra param
				const char *path = "/receive";
				build_push_url(url, sizeof(url), push_url, push_port, path);
				if (url[0] == '\0') {
        				fprintf(stderr, "Failed to build URL\n");
					writeLog("Failed to build push url.", 1, 0);
    				}
				else {
					if (use_push) {
						char json_path[filename_size];
						snprintf(infostr, infostr_size, "Pushing data to url '%s'.", url);
                                        	writeLog(trim(infostr), 0, 0);
                                        	int written = snprintf(json_path, filename_size, "%s/%s", dataDir, jsonFileName);
                                        	if (written < 0) {
                                                	writeLog("Could not write to push json file", 2, 0);
                                        	}
                                        	if ((size_t)written >= filename_size) {
                                                	writeLog("Push file name truncated. Name is too long.", 1, 0);
                                        	}
						if (post_json_file_stream(url, json_path) == 0) {
							writeLog("Data pushed successfully.", 0, 0);
    						} 
						else {
							writeLog("Failed to push data.", 2, 0);
    						}
						flushLog();
					}
					if (use_metrics_push) {
						char metrics_path[storename_size];
    						snprintf(infostr, infostr_size, "Pushing metrics to url '%s'.", url);
                                                writeLog(trim(infostr), 0, 0);
						int written = snprintf(metrics_path, storename_size, "%s/%s", storeDir, metricsFileName);
						if (written < 0) {
							writeLog("Could not write to push metrics file", 2, 0);
						}
						if ((size_t)written >= filename_size) {
							writeLog("Push metrics file name truncated. Name is too long.", 1, 0);
						}
						if (post_metrics_file_stream(url, metrics_path) == 0) {
							writeLog("Merics pushed successfully.", 0, 0);
						}
						else {
							writeLog("Failed to push metrics.", 2, 0);
						}
						flushLog(); 
					}
				}
				push_interval_cnt = 0;
			}
		}
		else	
			flushLog();
		if (truncateLog) {
			if (trunc_time == 0) {
				check_file_truncation();
			}
			// Check truncation only every 10th cycle
			trunc_time++;
			if (trunc_time >= 10) {
				trunc_time = 0;
			}
		}
		else {
			//printf("TruncateLog not active.\n");
		}
		if (total_threads_run >= MAX_THREAD_COUNT) {
			writeLog("You are reaching the limit of max_thread_counter. It will be reset to 1.", 1, 0);
			writeLog("Reaching MAX_THREAD_COUNT is an indication the service has been alive too long without restart.", 0, 0);
			total_threads_run = 1;
			flushLog();
		}
	}
}

int isConstantsEnabled () {
	FILE *file = NULL;
	char line[10];
	char *searchString = "enable";

	file = fopen("/etc/almond/memalloc.conf", "r");
	if (file == NULL) {
		printf("No constants file will be used.\n");
		writeLog("No memalloc.conf file was found.", 1, 1);
		return 0;
	}
	while (fgets(line, sizeof(line), file)) {
		if (strstr(line, searchString)) {
			writeLog("Constants file is enabled.", 0, 1);
			fclose(file);
			return 1;
			break;
		}
	}
	fclose(file);
	return 0;
}

void initLogMessages() {
	for (int i = 0; i < 5; i++) {
		logmessage_id[i] = 0;
	}
}

void initialLogging() {
	char lfin[28] = "/var/log/almond/almond.log";

        fptr = fopen(lfin, "a");
	if (!fptr) {
        	perror("Failed to open log file");
        	exit(EXIT_FAILURE);
    	}
        fprintf(fptr, "\n");
        printf("Starting almond version %s.\n", VERSION);
        initConstants();
        writeLog("Almond constants initialized.", 0, 1);
        writeLog("Starting almond (0.9.28)...", 0, 1);
}

int closeFileHandler() {
	fclose(fptr);
	fptr = NULL;
	return EXIT_FAILURE;
}

void setupSignalHandlers() {
	struct sigaction sa;

    	memset(&sa, 0, sizeof(sa));
    	sa.sa_handler = sig_handler;
    	if (sigaction(SIGINT, &sa, NULL) == -1) {
        	logError("Failed to set SIGINT handler", 2, 1);
		printf("Failed to set SIGTERM handler: %s", strerror(errno));
		closeFileHandler();
        	return;
    	}

   	memset(&sa, 0, sizeof(sa));
    	sa.sa_handler = sig_handler;
    	if (sigaction(SIGTERM, &sa, NULL) == -1) {
        	logError("Failed to set SIGTERM handler", 2, 1);
		printf("Failed to set SIGTERM handler: %s", strerror(errno));
		closeFileHandler();
        	return;
    	}
}

int loadConfiguration() {
	int retVal = getConfigurationValues();
        if (retVal == 0) {
                logInfo("Configuration read ok.", 0, 1);
		if (useKafkaConfigFile) {
			if (kafkaConfigFile != NULL) {
				if (fileExists(kafkaConfigFile) == 0) {
					snprintf(infostr, infostr_size, "Setting Kafka config file to: %s.", kafkaConfigFile);
					logInfo(trim(infostr), 0, 1);
					setKafkaConfigFile(kafkaConfigFile);
				}
				else {
					snprintf(infostr, infostr_size, "File does not exist: %s", kafkaConfigFile);
					logInfo(trim(infostr), 2, 1);
					logInfo("Kafka will use default config file: /etc/almond/kafka.conf", 0, 1);
				}
			}
			if (loadKafkaConfig() == 0) {
				logInfo("Kafka configuration read ok.", 0, 1);
				if (init_kafka_producer() != 0) {
					logInfo("Error initiating Kafka producer.", 2, 1);
					return 1;
				}
				else {
					logInfo("Kafka producer initiated.", 0, 1);
				}
			}
		}
        }
        else {
                logError("Could not load configuration, due to corruption or memory allocation failure.", 1, 1);
                return 1;
        }
	return 0;
}

void initLoggerThread() {
	fclose(fptr);
        fptr = NULL;
        printf("Initiate logger\n");
        initLogger();
        logInfo("Initiate plugins.", 0, 0);
        fflush(fptr);
}

int loadPlugins() {
	decCount = countDeclarations(pluginDeclarationFile);
        //threadIds = (unsigned short*)malloc((size_t)decCount * sizeof(unsigned short));
        for (int i = 0; i < decCount; i++) {
                threadIds[i] = 0;
        }
        /*g_plugins = (PluginItem *)malloc((size_t)sizeof(PluginItem) * decCount);
        declaration_size = (size_t)decCount;
        if (!g_plugins) {
                perror ("Error allocating memory");
                writeLog("Error allocating memory - PluginItem.", 2, 0);
                abort();
        }
        printf("Declarations initiated.\n");
        for (int i = 0; i < decCount; i++) {
                g_plugins[i].name = malloc((size_t)pluginitemname_size + 1);
                if (g_plugins[i].name == NULL) {
                        logError("Failed to allocate g_plugins.", 2, 0);
                        exit(2);
                }
                else
                        g_plugins[i].name[0] = '\0';
                g_plugins[i].description = malloc((size_t)pluginitemdesc_size + 1);
                if (g_plugins[i].description == NULL){
                        logError("Failed to allocate g_plugins.", 2, 0);
                        exit(2);
                }
                else
                        g_plugins[i].description[0] = '\0';
                g_plugins[i].command = malloc((size_t)pluginitemcmd_size + 1);
                if (g_plugins[i].command == NULL) {
                        logError("Failed to allocate g_plugins.", 2, 0);
                        exit(2);
                }
                else
                        g_plugins[i].command[0] = '\0';
        }*/
	//init_plugins(pluginDeclarationFile, &declaration_size);
	init_plugins();
        logInfo("Declarations read.", 0, 0);
        /*outputs = malloc((size_t)sizeof(PluginOutput)*decCount);
        if (!outputs){
                perror("Error allocating memory");
                writeLog("Error allocating memory - PluginOutput.", 2, 0);
                abort();
        }*/
        for (size_t i = 0; i < decCount; ++i) {
    		//plugin_output_init(&outputs[i]);
	}
        /*for (int i = 0; i < decCount; i++) {
                outputs[i].retString = malloc((size_t)pluginoutput_size);
                if (outputs[i].retString == NULL) {
                        logError("Failed to allocate outputs.", 2, 0);
                        exit(2);
                }
                else
                        outputs[i].retString[0] = '\0';
        }*/
        output_size = (size_t)decCount;
        //int pluginDeclarationResult = loadPluginDeclarations(pluginDeclarationFile, 0);
	// This should be deprecated
	int pluginDeclarationResult = 9;
        time_t dummy = time(NULL);
        checkPluginFileStat(pluginDeclarationFile, dummy, 1);
        if (pluginDeclarationResult <= 0){
                logInfo("Problem reading from plugin declaration file.", 1, 0);
        }
        else {
                logInfo("Plugin g_plugins file loaded.", 0, 0);
        }
	//printf("DEBUG: pluginDeclarationResult = %d\n", pluginDeclarationResult);
	return 0;
}

void apiReload() {
	// Reinitiate all Almond vars, copy needed if failed?
	if (loadConfiguration() != 0) {
		constructSocketMessage("almond_reload", "failed");
	}
	else {
		constructSocketMessage("almond_reload", "true");
	}
}

void* zombieReaper(void* arg) {
	sigset_t set;
    	sigemptyset(&set);
    	sigaddset(&set, SIGCHLD);

    	int sig;

	while(!is_stopping) {
		if (sigwait(&set, &sig) == 0 && sig == SIGCHLD) {
			pid_t pid;
			int status;
			while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
				if (is_plugin_pid(pid)) {
					continue;
				}
				snprintf(infostr, infostr_size, "Reaper thread cleaned up orphan PID %d", pid);
				writeLog(trim(infostr), 0, 0);
			}
		}
	}
	writeLog("Reaper thread exiting gracefully", 0, 0);
	return NULL;
}

static int init_system(void) {
	install_signals();
        initialLogging();
        int configResult = loadConfiguration();
        if (configResult != 0) {
                logError("Failed to load configuration", 1, 1);
                return 1;
        }
        else
                printf("Configuration read.\n");

        if (strcmp(hostName, "None") == 0) {
                char *tempHost = getHostName();
                snprintf(hostName, 255, "%s", tempHost);
                free(tempHost);
        }
        writeLog("Initiate logger thread.", 0, 1);
        initLoggerThread();
        if (check_plugin_conf_file(pluginDeclarationFile) != 0) {
                logError("plugins.conf file seems to be corrupt. Program will shut down.", 2, 0);
                return 2;
        }
        threadIds = (unsigned short*)malloc((size_t)MAX_PLUGINS * sizeof(unsigned short));
        memset(threadIds, 0, MAX_PLUGINS * sizeof(unsigned short));
        checkPluginFileStat(pluginDeclarationFile, tPluginFile, 0);
        logInfo("No errors found in plugins.conf", 0, 0);
        decCount = countDeclarations(pluginDeclarationFile);
        for (int i = 0; i < decCount; i++) {
                threadIds[i] = 0;
        }
        g_current_scheduler_cnt = decCount;
        if (init_plugins() != 0) {
                logError("Failed to initiate plugins", 2, 0);
                flushLog();
                return 2;
        }
        flushLog();
        initScheduler(decCount, initSleep, false);
        return 0;
}

static void run_check_loop(void) {
    while (!is_stopping) {
        scheduleChecks();
    }
}

static void shutdown_system(void) {
	switch (shutdown_reason) {
                case SR_SIGINT:
                        writeLog("Caught SIGINT, exiting program.", 0, 0);
                        break;
                case SR_SIGKILL:
                        writeLog("Caught SIGKILL, exiting program.", 0, 0);
                        break;
                default:
                        writeLog("Normal program termination.", 0, 0);
                        break;
        }
        sig_exit_app();
	if (threadIds) {
        	free(threadIds);
        	threadIds = NULL;
    	}

    	flushLog();
}

int main(int argc, char* argv[]) {
	#if defined(_BSD_SOURCE) || defined(_SVID_SOURCE)
		#define HAS_BIRTHTIME 1
	#else
		#define HAS_BIRTHTIME 0
	#endif
        int rc = init_system();
	if (rc != 0) {
		return rc;
    	}

	run_check_loop();
	shutdown_system();

   	return 0;
}
