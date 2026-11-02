#define _POSIX_C_SOURCE 200809L
#define SERDES_SCHEMA_AVRO 1
#define SERDES_SCHEMA_PROTOBUF 2
#include <sys/types.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <avro.h>
#include <libserdes/serdes-avro.h>
#include <librdkafka/rdkafka.h>
#include "main.h"
#include "configuration.h"
#include "logger.h"
#include "mod_avro.h"
#include "structures.h"
#include "kafkaapi.h"

#ifndef RD_KAFKA_RESP_ERR__UNKNOWN
#define RD_KAFKA_RESP_ERR__UNKNOWN RD_KAFKA_RESP_ERR_UNKNOWN
#endif
#define MAX_STRING_SIZE 50
#define INFO_STR_SIZE 810 
#define DEFAULT_AVRO_DECLARATION_FILE "/opt/almond/plugin_status.avsc"

extern char *schemaRegistryUrl;
extern char schemaName[100];

char* configFile = NULL;
char* brokers = NULL;
char* topic = NULL;
char* kafka_client_id = NULL;
char* kafka_partitioner = NULL;
char* transactional_id = NULL;
char* kafka_compression = NULL;
char* kafkaCALocation = NULL;
char* kafkaSSLCertificate = NULL;
char* kafka_SSLKey = NULL;
char* kafka_sasl_mechanisms = NULL;
char* kafka_sasl_username = NULL;
char* kafka_sasl_password = NULL;
char* kafka_security_protocol = NULL;
char* avro_declaration_file = NULL;
char* kafka_key_type = NULL;
char* kafka_index_key = NULL;
bool use_transactions = false;
bool enable_idempotence = false;
bool kafka_dr_cb = true;
bool kafka_log_connection_close = false;
int metadata_request_timeout = 60000;
int acks = -2;
int retries = 5;
int max_in_flight_requests = 5;
int linger_ms = 10;
int batch_num_messages = 10000;
int queue_buffering_max_messages = 100000;
int queue_buffering_max_kbytes = 1048576;
int message_max = 1000000;
int message_copy_max = 65535;
int request_timeout = 30000;
int message_timeout_ms = 300000;
int retry_backoff = 100;
int statistics_interval = 60000;
int kafka_socket_timeout = 60000;
int kafka_socket_blocking = 1000;
int timeout_ms = 30000;
rd_kafka_t *global_producer = NULL;
char info[INFO_STR_SIZE];
int config_k_memalloc_fails = 0;
int requirements_met = 0;

static void process_kafka_brokers(ConfVal);
static void process_kafka_topic(ConfVal);
void process_kafka_client_id(ConfVal);
void process_metadata_request_timeout(ConfVal);
void process_kafka_acks(ConfVal);
void process_kafka_idempotence(ConfVal);
void process_kafka_transactional_id(ConfVal);
void process_kafka_retries(ConfVal);
void process_kafka_in_flight_requests(ConfVal);
void process_kafka_linger(ConfVal);
void process_kafka_batch_num_messages(ConfVal);
void process_queue_buffering_messages(ConfVal);
void process_queue_buffering_kbytes(ConfVal);
void process_kafka_compression(ConfVal);
void process_kafka_partitioner(ConfVal);
void process_kafka_message_max(ConfVal);
void process_message_copy_max(ConfVal);
void process_kafka_request_timeout(ConfVal);
void process_kafka_message_timeout(ConfVal);
void process_kafka_retry_backoff(ConfVal);
void process_kafka_dr_cb(ConfVal);
void process_kafka_statistics_interval(ConfVal);
void process_kafka_log_connection_close(ConfVal);
void process_kafka_ssl_ca_location(ConfVal);
void process_kafka_certificate(ConfVal);
void process_kafka_key(ConfVal);
void process_kafka_security_protocol(ConfVal);
void process_kafka_sasl_mechanisms(ConfVal);
void process_kafka_sasl_password(ConfVal);
void process_kafka_sasl_username(ConfVal);
void process_kafka_socket_timeout(ConfVal);
void process_kafka_socket_blocking(ConfVal);
void process_kafka_transaction_timeout(ConfVal);
void process_scheduler_avro_declaration(ConfVal);

ConfigEntry kafka_entries[] = {
    {"kafka.bootstrap.servers", process_kafka_brokers},
    {"kafka.topic", process_kafka_topic},
    {"kafka.client.id", process_kafka_client_id},
    {"kafka.metadata.max.age.ms", process_metadata_request_timeout},
    {"kafka.acks", process_kafka_acks},
    {"kafka.enable.idempotence", process_kafka_idempotence},
    {"kafka.transactional.id", process_kafka_transactional_id},
    {"kafka.retries", process_kafka_retries},
    {"kafka.max.in.flight.requests.per.connection", process_kafka_in_flight_requests},
    {"kafka.linger.ms", process_kafka_linger},
    {"kafka.batch.num.messages", process_kafka_batch_num_messages},
    {"kafka.queue.buffering.max.messages", process_queue_buffering_messages},
    {"kafka.queue.buffering.max.kbytes", process_queue_buffering_kbytes},
    {"kafka.compression.codec", process_kafka_compression},
    {"kafka.message.max.bytes", process_kafka_message_max},
    {"kafka.message.copy.max.bytes", process_message_copy_max},
    {"kafka.request.timeout.ms", process_kafka_request_timeout},
    {"kafka.delivery.timeout.ms", process_kafka_message_timeout},
    {"kafka.retry.backoff.ms", process_kafka_retry_backoff},
    {"kafka.dr_cb", process_kafka_dr_cb},
    {"kafka.statistics.interval.ms", process_kafka_statistics_interval},
    {"kafka.log.connection.close", process_kafka_log_connection_close},
    {"kafka.ssl.ca.location", process_kafka_ssl_ca_location},
    {"kafka.ssl.certificate.location", process_kafka_certificate},
    {"kafka.ssl.key.location", process_kafka_key},
    {"kafka.security.protocol", process_kafka_security_protocol},
    {"kafka.sasl.mechanisms", process_kafka_sasl_mechanisms},
    {"kafka.sasl.username", process_kafka_sasl_username},
    {"kafka.sasl.password", process_kafka_sasl_password},
    {"kafka.socket.timeout.ms", process_kafka_socket_timeout},
    {"kafka.socket.blocking.max.ms", process_kafka_socket_blocking},
    {"kafka.transaction.timeout.ms", process_kafka_transaction_timeout},
    {"kafka.partitioner", process_kafka_partitioner},
    {"kafka.key.type", process_kafka_key_type},
    {"scheduler.kafkaAvroDeclarationFile", process_scheduler_avro_declaration}
};


char *triminfo(char *s) {
	char *ptr;
    	if (!s)
        	return NULL;   // NULL string
    	if (!*s)
        	return s;      // empty string
    	for (ptr = s + strlen(s) - 1; (ptr >= s) && isspace(*ptr); --ptr);
    	ptr[1] = '\0';
	return s;
}

bool is_numeric_string(const char *s) {
        if (!s || !*s) return false;
        for (; *s; s++) {
                if (!isdigit(*s)) return false;
        }
        return true;
}

void to_uppercase(char *str) {
        for (int i = 0; str[i]; i++) {
                str[i] = toupper((unsigned char)str[i]);
        }
}

char *load_file(const char *filename, size_t *len_out) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) return NULL;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return NULL; }
    long len = ftell(fp);
    if (len < 0 || fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return NULL; }
    char *buf = malloc((size_t)len + 1);
    if (!buf) { fclose(fp); return NULL; }
    size_t nr = fread(buf, 1, (size_t)len, fp);
    int read_error = ferror(fp);
    fclose(fp);
    if (read_error) { free(buf); return NULL; }
    buf[nr] = '\0';
    if (len_out) *len_out = nr;
    return buf;
}

static int serialize_and_send(char *dummy_topic, const GKafkaMessage *msg) {
    char errstr[512];
    //char info[812];
    void *buffer = NULL;
    size_t length = 0;
    avro_value_t v, data, record;
    rd_kafka_resp_err_t err = RD_KAFKA_RESP_ERR_NO_ERROR;

    if (schemaRegistryUrl == NULL) {
        writeLog("No url to schema registry found in config.", 2, 0);
        return -1;
    }

    // --- SETUP SERDES (Schema Registry SSL) ---
    serdes_conf_t *sconf = serdes_conf_new(errstr, sizeof(errstr), "schema.registry.url", schemaRegistryUrl, NULL);
    if (!sconf) return -1;

    // If your registry is HTTPS and requires the same certs as Kafka:
    if (kafkaCALocation) serdes_conf_set(sconf, "ssl.ca.location", kafkaCALocation, NULL, 0);
    if (kafkaSSLCertificate) serdes_conf_set(sconf, "ssl.certificate.location", kafkaSSLCertificate, NULL, 0);
    if (kafka_SSLKey) serdes_conf_set(sconf, "ssl.key.location", kafka_SSLKey, NULL, 0);

    serdes_t *serdes = serdes_new(sconf, NULL, 0);
    if (!serdes) return -1; /* sconf ownership belongs to libserdes */
    size_t schema_len;
    const char *filepath = (avro_declaration_file != NULL) 
                       ? avro_declaration_file 
                       : DEFAULT_AVRO_DECLARATION_FILE;
    //char *schema_buf = load_file("plugin_status.avsc", &schema_len); // Adjust path if needed
    /*if (avro_declaration_file != NULL)
    	schema_buf = load_file(avro_declaration_file, &schema_len);
    else
        schema_buf = load_file(DEFAULT_AVRO_DECLARATION_FILE, &schema_len);*/
    char *schema_buf = load_file(filepath, &schema_len);
    if (!schema_buf) {
        snprintf(info, INFO_STR_SIZE, "[mod_avro] Failed to load Avro schema file: %s", filepath);
        writeLog(triminfo(info), 2, 0);
        serdes_destroy(serdes);
        return -1;
    }

    serdes_schema_t *schema = serdes_schema_add(serdes, schemaName, -1,
                                               schema_buf, schema_len,
                                               errstr, sizeof(errstr));
    free(schema_buf);
    schema_buf = NULL;
    if (!schema) {
	snprintf(info, INFO_STR_SIZE, "[mod_kafka] Failed to register schema '%s': %s", schemaName, errstr);
	writeLog(triminfo(info), 2, 0);
        /*fprintf(stderr, "Failed to register schema: %s\n", errstr);
        free(schema_buf);*/
        serdes_destroy(serdes);
        return -1;
    }

    avro_schema_t avro_schema = serdes_schema_avro(schema);
    avro_value_iface_t *iface = avro_generic_class_from_schema(avro_schema);
    if (!iface) {
        serdes_schema_destroy(schema);
        serdes_destroy(serdes);
        return -1;
    }
    if (avro_generic_value_new(iface, &record) != 0) {
        avro_value_iface_decref(iface);
        serdes_schema_destroy(schema);
        serdes_destroy(serdes);
        return -1;
    }

    // --- SET FIXED FIELDS ---
    // Top-level fields
    avro_value_get_by_name(&record, "name", &v, NULL); avro_value_set_string(&v, msg->name);
    avro_value_get_by_name(&record, "id", &v, NULL);   avro_value_set_string(&v, msg->id);
    avro_value_get_by_name(&record, "tag", &v, NULL);  avro_value_set_string(&v, msg->tag);

    // Nested "data" record fields
    avro_value_get_by_name(&record, "data", &data, NULL);
    avro_value_get_by_name(&data, "lastChange", &v, NULL);          avro_value_set_string(&v, msg->lastChange);
    avro_value_get_by_name(&data, "lastRun", &v, NULL);             avro_value_set_string(&v, msg->lastRun);
    avro_value_get_by_name(&data, "name", &v, NULL);                avro_value_set_string(&v, msg->dataName);
    avro_value_get_by_name(&data, "nextRun", &v, NULL);             avro_value_set_string(&v, msg->nextRun);
    avro_value_get_by_name(&data, "pluginName", &v, NULL);          avro_value_set_string(&v, msg->pluginName);
    avro_value_get_by_name(&data, "pluginOutput", &v, NULL);        avro_value_set_string(&v, msg->pluginOutput);
    avro_value_get_by_name(&data, "pluginStatus", &v, NULL);        avro_value_set_string(&v, msg->pluginStatus);
    avro_value_get_by_name(&data, "pluginStatusChanged", &v, NULL); avro_value_set_string(&v, msg->pluginStatusChanged);
    avro_value_get_by_name(&data, "pluginStatusCode", &v, NULL);    avro_value_set_int(&v, msg->pluginStatusCode);

    // --- SET DYNAMIC LABELS ---
    /*if (msg->labels && avro_value_get_by_name(&data, "labels", &labels_map, NULL) == 0) {
        json_object_object_foreach(msg->labels, key, val) {
            avro_value_t element;
            const char *val_str = json_object_get_string(val);
            if (avro_value_add(&labels_map, key, &element, NULL, NULL) == 0) {
                avro_value_set_string(&element, val_str);
            }
        }
    }*/

    // --- SET DYNAMIC METRICS ---
    //avro_value_t metrics_map;
    /*if (msg->metrics && avro_value_get_by_name(&data, "metrics", &metrics_map, NULL) == 0) {
        json_object_object_foreach(msg->metrics, key, val) {
            avro_value_t element;
            const char *val_str = json_object_get_string(val);
            if (avro_value_add(&metrics_map, key, &element, NULL, NULL) == 0) {
                avro_value_set_string(&element, val_str);
            }
        }
    }*/

    // --- SET DYNAMIC LABELS (Now at TOP LEVEL) ---
    if (msg->labels != NULL && json_object_get_type(msg->labels) == json_type_object) {
        avro_value_t labels_map;
        // We now look in 'record' (top level) instead of 'data'
        if (avro_value_get_by_name(&record, "labels", &labels_map, NULL) == 0) {
            json_object_object_foreach(msg->labels, key, val) {
                avro_value_t element;
                const char *val_str = json_object_get_string(val);
                if (val_str && avro_value_add(&labels_map, key, &element, NULL, NULL) == 0) {
                    avro_value_set_string(&element, val_str);
                }
            }
        }
    }

    // --- SET DYNAMIC METRICS (Now at TOP LEVEL) ---
    /*if (msg->metrics != NULL && json_object_get_type(msg->metrics) == json_type_object) {
        avro_value_t metrics_map;
        // We now look in 'record' (top level) instead of 'data'
        if (avro_value_get_by_name(&record, "metrics", &metrics_map, NULL) == 0) {
            json_object_object_foreach(msg->metrics, key, val) {
                avro_value_t element;
                const char *val_str = json_object_get_string(val);
                if (val_str && avro_value_add(&metrics_map, key, &element, NULL, NULL) == 0) {
                    avro_value_set_string(&element, val_str);
                }
            }
        }
    }*/

   // --- SET DYNAMIC METRICS (Map of Maps) ---
   if (msg->metrics != NULL && json_object_get_type(msg->metrics) == json_type_object) {
        avro_value_t metrics_map;

        if (avro_value_get_by_name(&record, "metrics", &metrics_map, NULL) == 0) {
                json_object_object_foreach(msg->metrics, key, val) {
                        // We expect 'val' to be an object: {"value": "1.2", "unit": "percent"}
                        if (json_object_get_type(val) == json_type_object) {
                                avro_value_t inner_map;
                                // 1. Add the Metric Name (e.g., CpuIdle) as a new Map inside metrics
                                if (avro_value_add(&metrics_map, key, &inner_map, NULL, NULL) == 0) {
                                // 2. Loop through the inner JSON object
                                        json_object_object_foreach(val, inner_key, inner_val) {
                                                avro_value_t element;
                                                const char *inner_str = json_object_get_string(inner_val);
                                                // 3. Add the attributes (value, unit, etc.) to the inner map
                                                if (inner_str && avro_value_add(&inner_map, inner_key, &element, NULL, NULL) == 0) {
                                                        avro_value_set_string(&element, inner_str);
                                                }
                                        }
                                }
                        }
                }
        }
    }

    // --- SERIALIZE & PRODUCE ---
    // DEBUG
    char debug_msg[1024]; // Buffer for debug logs

    // 1. Check if the producer even exists
    if (global_producer == NULL) {
        writeLog("[DEBUG] CRITICAL: global_producer is NULL in serialize_and_send! Check extern linkage.", 1, 0);
        // Cleanup and return early to prevent further issues
        avro_value_decref(&record);
        avro_value_iface_decref(iface);
        if (schema_buf) {
		free(schema_buf);
		schema_buf = NULL;
	}
        serdes_schema_destroy(schema);
        serdes_destroy(serdes);
        return -1;
    }

    serdes_err_t serr = serdes_schema_serialize_avro(schema, &record, &buffer, &length, errstr, sizeof(errstr));
    if (serr != SERDES_ERR_OK) {
        fprintf(stderr, "serialize_avro failed: %s\n", serdes_err2str(serr));
        snprintf(debug_msg, sizeof(debug_msg), "[DEBUG] Avro Serialization FAILED: %s", serdes_err2str(serr));
        writeLog(debug_msg, 1, 0);
    } else {
        snprintf(debug_msg, sizeof(debug_msg), "[DEBUG] Serialization SUCCESS. Buffer size: %zu bytes. Topic: %s", length, topic);
        writeLog(debug_msg, 0, 0);
        bool transaction_started = false;
        if (use_transactions) {
            rd_kafka_error_t *kerr = rd_kafka_begin_transaction(global_producer);
            if (kerr) {
                writeLog(rd_kafka_error_string(kerr), 1, 0);
                rd_kafka_error_destroy(kerr);
                err = RD_KAFKA_RESP_ERR__UNKNOWN;
                goto serialized_cleanup;
            }
            transaction_started = true;
        }
        int retries_left = 5;
        retry:
		if (kafka_key_type != NULL && kafka_index_key != NULL) {
                	size_t index_len = strlen(kafka_index_key);
        		err = rd_kafka_producev(global_producer,
                                RD_KAFKA_V_TOPIC(topic),
				RD_KAFKA_V_PARTITION(RD_KAFKA_PARTITION_UA),
                                RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
                                RD_KAFKA_V_VALUE(buffer, length),
				RD_KAFKA_V_KEY((void *)kafka_index_key, index_len),
                                RD_KAFKA_V_END);
		}
		else {
			err = rd_kafka_producev(global_producer,
                                RD_KAFKA_V_TOPIC(topic),
                                RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
                                RD_KAFKA_V_VALUE(buffer, length),
                                RD_KAFKA_V_END);
		}

        if (err) {
                snprintf(debug_msg, sizeof(debug_msg), "[DEBUG] rd_kafka_producev FAILED: %s", rd_kafka_err2str(err));
                writeLog(debug_msg, 1, 0);
        }
        if (err == RD_KAFKA_RESP_ERR__QUEUE_FULL && --retries_left > 0) {
            rd_kafka_poll(global_producer, 1000);
            goto retry;
        }

        if (transaction_started) {
            rd_kafka_error_t *kerr = err ?
                rd_kafka_abort_transaction(global_producer, timeout_ms) :
                rd_kafka_commit_transaction(global_producer, timeout_ms);
            if (kerr) {
                writeLog(rd_kafka_error_string(kerr), 1, 0);
                rd_kafka_error_destroy(kerr);
                err = RD_KAFKA_RESP_ERR__UNKNOWN;
            }
        }
        else {
                //rd_kafka_poll(global_producer, 0);
                int events = rd_kafka_poll(global_producer, 100);
                snprintf(debug_msg, sizeof(debug_msg), "[DEBUG] Poll triggered, processed %d events", events);
                writeLog(debug_msg, 0, 0);
        }
    }

serialized_cleanup:
    // --- CLEANUP ---
    avro_value_decref(&record);
    avro_value_iface_decref(iface);
    if (buffer) free(buffer);
    if (schema_buf) {
	free(schema_buf);
	schema_buf = NULL;
    }
    serdes_schema_destroy(schema);
    serdes_destroy(serdes);
    return (serr == SERDES_ERR_OK && !err) ? 0 : -1;
}


void processIntConfigInfo(char* name, int value) {
        snprintf(info, INFO_STR_SIZE, "[mod_kafka] %s is set to %d.", name, value);
        writeLog(triminfo(info), 0, 1);
}

int mem_k_alloc(void* ptr, char* name) {
        if(ptr == NULL) {
                fprintf(stderr, "[mod_kafka] Failed to allocate memory for %s.\n", name);
                snprintf(info, INFO_STR_SIZE, "[mod_kafka] Failed to allocate memory for '%s'.", name);
                writeLog(triminfo(info), 2, 1);
                config_k_memalloc_fails++;
                return 1;
        }
        return 0;
}

static char *safe_strdup(const char *src, const char *name) {
        if (!src || !*src) return NULL;
        size_t n = strlen(src) + 1;
        char *dup = malloc(n);
        if (!dup) {
                /* mem_k_alloc logs the failure and increments the counter */
                mem_k_alloc(dup, (char *)name);
                return NULL;
        }
        memcpy(dup, src, n);
        return dup;
}

static void free_and_null(char **p) {
        if (p && *p) {
                free(*p);
                *p = NULL;
        }
}

static bool str_to_bool(const char *s, bool default_val) {
        if (!s) return default_val;
        char tmp[64];
        strncpy(tmp, s, sizeof(tmp));
        tmp[sizeof(tmp)-1] = '\0';
        triminfo(tmp);
        to_uppercase(tmp);
        if (strcmp(tmp, "TRUE") == 0) return true;
        if (strcmp(tmp, "FALSE") == 0) return false;
        return default_val;
}

static int parse_int_with_default(const char *s, int def) {
        if (!s) return def;
        char *endptr = NULL;
        long v = strtol(s, &endptr, 0);
        if (endptr == s) return def;
        return (int)v;
}

static void process_kafka_brokers(ConfVal value) {
        if (value.strval && strlen(value.strval) > 4) {
                char *dup = safe_strdup(value.strval, "kafka.bootstrap.servers");
                if (!dup) return;
                free_and_null(&brokers);
                brokers = dup;
                snprintf(info, INFO_STR_SIZE, "[mod_kafka] Kafka export brokers is set to '%s'.", brokers);
                writeLog(triminfo(info), 0, 1);
                requirements_met++;
        } else {
                writeLog("[mod_kafka] Almond brokers is set to NULL.", 2, 1);
        }
}

static void process_kafka_topic(ConfVal value) {
        if (value.strval && strlen(value.strval) > 1) {
                char *dup = safe_strdup(value.strval, "kafka.topic");
                if (!dup) return;
                free_and_null(&topic);
                topic = dup;
                snprintf(info, INFO_STR_SIZE, "[mod_kafka] Kafka topic is set to '%s'.", topic);
                writeLog(triminfo(info), 0, 1);
                requirements_met++;
        } else {
                writeLog("[mod_kafka] Almond topic is set to NULL.", 2, 1);
        }
}

void process_kafka_client_id(ConfVal value) {
        if (value.strval && strlen(value.strval) > 1) {
                char *dup = safe_strdup(value.strval, "kafka.client.id");
                if (!dup) return;
                free_and_null(&kafka_client_id);
                kafka_client_id = dup;
                snprintf(info, INFO_STR_SIZE, "[mod_kafka] Kafka client id is set to '%s'.", kafka_client_id);
                writeLog(triminfo(info), 0, 1);
        } else {
                writeLog("[mod_kafka] kafka.client.id is set to NULL.", 0, 1);
        }
}

void process_metadata_request_timeout(ConfVal value) {
        int i = parse_int_with_default(value.strval, 0);
        if (i == 0) {
                writeLog("[mod_kafka] Could not interpret 'kafka.metadata.max.age.ms' value from config file.", 1, 1);
                writeLog("[mod_kafka] 'kafka.metadata.max.age.ms' is set to 60000.", 0, 1);
                return;
        }
        metadata_request_timeout = i;
        snprintf(info, INFO_STR_SIZE, "[mod_kafka] kafka.metadata.max.age.ms is set to %i.", metadata_request_timeout);
        writeLog(triminfo(info), 0, 1);
}

void process_kafka_acks(ConfVal value) {
        char tmp[32] = "";
        if (value.strval && strlen(value.strval) > 0) {
                strncpy(tmp, value.strval, sizeof(tmp));
                tmp[sizeof(tmp)-1] = '\0';
                triminfo(tmp);
        }

        if (tmp[0] && !is_numeric_string(tmp)) {
                if (strcmp(tmp, "all") == 0) {
                        acks = -1; // sentinel for 'all'
                        writeLog("[mod_kafka] Kafka.acks is set to 'all'.", 0, 1);
                } else {
                        snprintf(info, INFO_STR_SIZE, "[mod_kafka] Invalid string value for Kafka.acks: %s.", tmp);
                        writeLog(triminfo(info), 1, 1);
                        writeLog("[mod_kafka] Kafka.acks will be set to 1.", 0, 1);
                        acks = 1;
                }
        } else if (value.intval == 0 || value.intval == 1) {
                acks = value.intval;
                snprintf(info, INFO_STR_SIZE, "[mod_kafka] Kafka.acks is set to %d.", value.intval);
                writeLog(triminfo(info), 0, 1);
        } else {
                snprintf(info, INFO_STR_SIZE, "[mod_kafka] Invalid integer value for Kafka.acks: %d", value.intval);
                writeLog(triminfo(info), 1, 1);
                writeLog("[mod_kafka] Kafka.acks will be set to 1.", 0, 1);
                acks = 1;
        }
}

void process_kafka_idempotence(ConfVal value) {
        enable_idempotence = str_to_bool(value.strval, false);
        snprintf(info, INFO_STR_SIZE, "[mod_kafka] Kafka.enable.idempotence is set to %s.", enable_idempotence ? "true" : "false");
        writeLog(triminfo(info), 0, 1);
}

void process_kafka_transactional_id(ConfVal value) {
        if (!value.strval || strlen(triminfo(value.strval)) == 0) {
                writeLog("[mod_kafka] kafka.transactional.id can not be an empty string.", 1, 1);
                return;
        }
        /* Reject numeric-only values */
        if (value.intval && value.intval != 0) {
                writeLog("[mod_kafka] kafka.transactional.id should not be an integer value, but a string.", 1, 1);
                return;
        }
        char *dup = safe_strdup(value.strval, "kafka.transactional.id");
        if (!dup) return;
        free_and_null(&transactional_id);
        transactional_id = dup;
        snprintf(info, INFO_STR_SIZE, "[mod_kafka] kafka.transactional.id is set to '%s'.", transactional_id);
        writeLog(triminfo(info), 0, 1);
}

void process_kafka_retries(ConfVal value) {
        int i = parse_int_with_default(value.strval, retries);
        if (i < 0) {
                writeLog("[mod_kafka] Can not evaluate negative numbers in kafka.retries.", 1, 1);
                writeLog("[mod_kafka] Kafka.retries is set to 5.", 0, 1);
                return;
        }
        retries = i;
        processIntConfigInfo("kafka.retries", retries);
}

void process_kafka_in_flight_requests(ConfVal value) {
        int i = parse_int_with_default(value.strval, max_in_flight_requests);
        if (i > 5) {
                writeLog("[mod_kafka] kafka.max.in.flight.requests.per.connection should not be above 5 if enabling kafka idempotence.", 0, 1);
        }
        max_in_flight_requests = i;
        processIntConfigInfo("kafka.max.in.flight.requests.per.connection", max_in_flight_requests);
}

void process_kafka_linger(ConfVal value) {
        int i = parse_int_with_default(value.strval, 0);
        if (i > 900000) {
                writeLog("[mod_kafka] kafka.linger.ms value is above maximum value, will reduce it to 900000.", 1, 1);
                writeLog("[mod_kafka] Note that kafka.linger.ms is at its hard limit (15 minutes).", 1, 1);
                i = 900000;
        }
        if (i < 0) i = 0;
        linger_ms = i;
        processIntConfigInfo("kafka.linger.ms", linger_ms);
}

void process_kafka_batch_num_messages (ConfVal value) {
        int i = value.intval;
        if (i < 0) i = 10000;
        batch_num_messages = i;
        processIntConfigInfo("kafka.batch_num_messages", batch_num_messages);
        writeLog("[mod_kafka] Use queue.buffering.max.messages instead of batch.num.messages.", 1, 1);
}

void process_queue_buffering_messages(ConfVal value) {
        int i = value.intval;
        if (i < 0) i = 100000;
        queue_buffering_max_messages = i;
        processIntConfigInfo("kafka.queue.buffering.max.messages", queue_buffering_max_messages);
}

void process_queue_buffering_kbytes(ConfVal value) {
        int i = value.intval;
        if (i < 0) i = 1048576;
        queue_buffering_max_kbytes = i;
        processIntConfigInfo("kafka.queue.buffering.max,kbytes", queue_buffering_max_kbytes);
}

void process_kafka_compression(ConfVal value) {
        if (!value.strval || !*value.strval) {
                writeLog("[mod_kafka] kafka.compression.codec is set to NULL.", 0, 1);
                return;
        }
        char *dup = safe_strdup(value.strval, "kafka.compression.codec");
        if (!dup) return;
        free_and_null(&kafka_compression);
        kafka_compression = dup;
        char *t = triminfo(kafka_compression);
        if (strcmp(t, "gzip") == 0 || strcmp(t, "snappy") == 0 || strcmp(t, "lz4") == 0 || strcmp(t, "zstd") == 0 || strcmp(t, "none") == 0) {
                snprintf(info, INFO_STR_SIZE, "[mod_kafka] kafka.compression.codec is set to '%s'.", t);
                writeLog(triminfo(info), 0, 1);
        } else {
                snprintf(info, INFO_STR_SIZE, "[mod_kafka] %s is not a valid kafka.compression.codec value", kafka_compression);
                writeLog(triminfo(info), 1, 1);
                writeLog("[mod_kafka] kafka.compression.codec will be set to 'lz4'.", 0, 1);
                free_and_null(&kafka_compression);
                kafka_compression = safe_strdup("lz4", "kafka.compression.codec");
        }
}

void process_kafka_message_max(ConfVal value) {
        int i = value.intval;
        if (i <= 0) i = 1000000;
        message_max = i;
        processIntConfigInfo("kafka.message.max.bytes", message_max);
}

void process_message_copy_max(ConfVal value) {
        int i = value.intval;
        if (i <= 0) i = 65535;
        message_copy_max = i;
        processIntConfigInfo("kafka.message.copy.max.bytes", message_copy_max);
}

void process_kafka_request_timeout(ConfVal value) {
        int i = value.intval;
        if (i <= 0) i = 30000;
        request_timeout = i;
        processIntConfigInfo("kafka.request.timeout.ms", request_timeout);
}

void process_kafka_message_timeout(ConfVal value) {
        int i = value.intval;
        if (i <= 0) i = 300000;
        message_timeout_ms = i;
        processIntConfigInfo("kafka.delivery.timeout.ms", message_timeout_ms);
}

void process_kafka_retry_backoff(ConfVal value) {
        int i = value.intval;
        if (i <= 0) i = 100;
        retry_backoff = i;
        processIntConfigInfo("kafka.retry.backoff.ms", retry_backoff);
}

void process_kafka_dr_cb(ConfVal value) {
        kafka_dr_cb = str_to_bool(value.strval, true);
        snprintf(info, INFO_STR_SIZE, "[mod_kafka] kafka.dr_cb is set to %s.", kafka_dr_cb ? "true" : "false");
        writeLog(triminfo(info), 0, 1);
}

void process_kafka_statistics_interval(ConfVal value) {
        int i = strtol(value.strval, NULL, 0);
        if (i <= 100) i = 60000;
        statistics_interval = i;
        processIntConfigInfo("kafka.statistics.interval.ms", statistics_interval);
}

void process_kafka_log_connection_close(ConfVal value) {
        kafka_log_connection_close = str_to_bool(value.strval, false);
        snprintf(info, INFO_STR_SIZE, "[mod_kafka] kafka.log.connection.close is set to  %s.", kafka_log_connection_close ? "true" : "false");
        writeLog(triminfo(info), 0, 1);
}

void process_kafka_ssl_ca_location(ConfVal value) {
        if (value.strval && strlen(value.strval) > 4) {
                char *dup = safe_strdup(value.strval, "kafka.ssl.ca.location");
                if (!dup) return;
                free_and_null(&kafkaCALocation);
                kafkaCALocation = dup;
                snprintf(info, INFO_STR_SIZE, "[mod_kafka] kafka.ssl.ca.location is set to '%s'.", kafkaCALocation);
                writeLog(triminfo(info), 0, 1);
        } else {
                writeLog("[mod_kafka] kafka.ssl.ca.location is set to NULL.", 0, 1);
        }
}

void process_kafka_certificate(ConfVal value) {
        if (value.strval && strlen(value.strval) > 4) {
                char *dup = safe_strdup(value.strval, "kafka.ssl.certificate.location");
                if (!dup) return;
                free_and_null(&kafkaSSLCertificate);
                kafkaSSLCertificate = dup;
                snprintf(info, INFO_STR_SIZE, "[mod_kafka] kafka.ssl.certificate.location is set to '%s'.", kafkaSSLCertificate);
                writeLog(triminfo(info), 0, 1);
        } else {
                writeLog("[mod_kafka] kafka.ssl.certificate.location is set to NULL.", 0, 1);
        }
}

void process_kafka_key(ConfVal value) {
        if (value.strval && strlen(value.strval) > 4) {
                char *dup = safe_strdup(value.strval, "kafka.ssl.key.location");
                if (!dup) return;
                free_and_null(&kafka_SSLKey);
                kafka_SSLKey = dup;
                snprintf(info, INFO_STR_SIZE, "[mod_kafka] kafka.ssl.key.location is set to '%s'.", kafka_SSLKey);
                writeLog(triminfo(info), 0, 1);
        } else {
                writeLog("[mod_kafka] kafka.ssl.key.location is set to NULL.", 0, 1);
        }
}

void process_kafka_security_protocol(ConfVal value) {
        if (!value.strval || strlen(value.strval) <= 2) {
                writeLog("[mod_kafka] kafka.security.protocol is set to NULL.", 0, 1);
                return;
        }
        char *dup = safe_strdup(value.strval, "kafka.security.protocol");
        if (!dup) return;
        free_and_null(&kafka_security_protocol);
        kafka_security_protocol = dup;
        to_uppercase(kafka_security_protocol);
        char *trimmed = triminfo(kafka_security_protocol);
        if ((strcmp(trimmed, "PLAINTEXT") == 0) ||
                (strcmp(trimmed, "SSL") == 0) ||
                (strcmp(trimmed, "SASL_PLAINTEXT") == 0) ||
                (strcmp(trimmed, "SASL_SSL") == 0) ||
                (strcmp(trimmed, "SASL") == 0)) {
                snprintf(info, INFO_STR_SIZE, "[mod_kafka] kafka.security.protocol is set to '%s'.", kafka_security_protocol);
                writeLog(triminfo(info), 0, 1);
        } else {
                snprintf(info, INFO_STR_SIZE, "[mod_kafka] %s is not a valid entry for kafka.security.protocol.", value.strval);
                writeLog(triminfo(info), 1, 1);
                free_and_null(&kafka_security_protocol);
                writeLog("[mod_kafka] kafka.security.protocol is set to NULL.", 0, 1);
        }
}

void process_kafka_sasl_mechanisms(ConfVal value) {
        if (!value.strval || strlen(value.strval) == 0) {
                writeLog("[mod_kafka] kafka.sasl.mechanisms is set to NULL.", 0, 1);
                return;
        }
        char *dup = safe_strdup(value.strval, "kafka.sasl.mechanisms");
        if (!dup) return;
        free_and_null(&kafka_sasl_mechanisms);
        kafka_sasl_mechanisms = dup;
        to_uppercase(kafka_sasl_mechanisms);
        char *trimmed = triminfo(kafka_sasl_mechanisms);
        if (strcmp(trimmed, "PLAIN") == 0) {
                writeLog("[mod_kafka] Kafka security mechanism is set to 'PLAIN'.", 0, 1);
        } else if (strcmp(trimmed, "SCRAM-SHA-256") == 0) {
                writeLog("[mod_kafka] Kafka security mechanism is set to SCRAM-SHA-256.", 0, 1);
        } else if (strcmp(trimmed, "SCRAM-SHA-512") == 0) {
                writeLog("[mod_kafka] Kafka security mechanism is set to SCRAM-SHA-512.", 0, 1);
        } else if (strcmp(trimmed, "GSSAPI") == 0) {
                writeLog("[mod_kafka] Kafka security mechanism is set to GSSAPI.", 0, 1);
        } else if (strcmp(trimmed, "OAUTHBEARER") == 0) {
                writeLog("[mod_kafka] Kafka security mechanism is set to OAUTHBEARER.", 0, 1);
        } else if (strcmp(trimmed, "AWS_MSK_IAM") == 0) {
                writeLog("[mod_kafka] Kafka security mechanism is set to AWS_MSK_IAM.", 0, 1);
        } else {
                snprintf(info, INFO_STR_SIZE, "[mod_kafka] '%s' is not a valid entry for kafka.sasl.mechanims.", kafka_sasl_mechanisms);
                writeLog(triminfo(info), 1, 1);
                free_and_null(&kafka_sasl_mechanisms);
                writeLog("[mod_kafka] kafka.sasl.mechanism is set to NULL.", 0, 1);
        }
}

void process_kafka_sasl_username(ConfVal value) {
    if (!value.strval || !*value.strval) {
        free_and_null(&kafka_sasl_username);
        return;
    }
    char *dup = safe_strdup(value.strval, "kafka.sasl.username");
    if (!dup) return;
    free_and_null(&kafka_sasl_username);
    kafka_sasl_username = dup;
    writeLog("[mod_avro] kafka.sasl.username is set.", 0, 1);
}

void process_kafka_sasl_password(ConfVal value) {
        if (value.strval && strlen(value.strval) > 1) {
                char *dup = safe_strdup(value.strval, "kafka.sasl.password");
                if (!dup) return;
                free_and_null(&kafka_sasl_password);
                kafka_sasl_password = dup;
                writeLog("[mod_kafka] Kafka.sasl.password is set.", 0, 1);
        } else {
                writeLog("[mod_kafka] kafka.sasl.password is set to NULL.", 0, 1);
        }
}

void process_kafka_socket_timeout(ConfVal value) {
        int i = value.intval;
        if (i <= 0) i = 30000;
        kafka_socket_timeout = i;
        processIntConfigInfo("kafka.socket.timeout.ms", kafka_socket_timeout);
}

void process_kafka_socket_blocking(ConfVal value) {
        int i = value.intval;
        if (i <= 0) i = 1000;
        kafka_socket_blocking = i;
        processIntConfigInfo("kafka.socket.blocking.max.ms", kafka_socket_blocking);
}

void process_kafka_transaction_timeout(ConfVal value)  {
        int i = value.intval;
        if (i <= 0) i = 30000;
        timeout_ms = i;
        processIntConfigInfo("Kafka transaction timeout", timeout_ms);
}

void process_kafka_partitioner(ConfVal value) {
        if (!value.strval || strlen(value.strval) == 0) {
                writeLog("[mod_kafka] kafka.partitioner is set to NULL.", 0, 1);
                return;
        }
        char *dup = safe_strdup(value.strval, "kafka.partitioner");
        if (!dup) return;
        free_and_null(&kafka_partitioner);
        kafka_partitioner = dup;
        char *t = triminfo(kafka_partitioner);
        if (strcmp(t, "consistent_random") == 0) {
                writeLog("[mod_kafka] Kafka partitioner is set to 'consistent_random'.", 0, 1);
        } else if (strcmp(t, "random") == 0) {
                writeLog("[mod_kafka] Kafka partitioner is set to 'random'.", 0, 1);
        } else if (strcmp(t, "consistent") == 0) {
                writeLog("[mod_kafka] Kafka partitioner is set to 'consistent'.", 0, 1);
        } else if (strcmp(t, "murmur2") == 0) {
                writeLog("[mod_kafka] Kafka partitioner is set to 'murmur2'.", 0, 1);
        } else if (strcmp(t, "murmur2_random") == 0) {
                writeLog("[mod_kafka] Kafka partitioner is set to 'murmur2_random'.", 0, 1);
        } else if (strcmp(t, "fnvla") == 0) {
                writeLog("[mod_kafka] Kafka partitioner is set to 'fnvla'.", 0, 1);
        } else if (strcmp(t, "fnvla2_random") == 0) {
                writeLog("[mod_kafka] Kafka partitioner is set to 'fnvla2_random'.", 0, 1);
        } else {
                snprintf(info, INFO_STR_SIZE, "[mod_kafka] '%s' is not a valid value for kafka.partitioner.", kafka_partitioner);
                writeLog(triminfo(info), 1, 1);
                free_and_null(&kafka_partitioner);
                kafka_partitioner = safe_strdup("consistent_random", "kafka.partitioner");
                writeLog("[mod_kafka] Kafka partitioner will be set to 'consistent_random'.", 0, 1);
        }
}

void process_kafka_avro(ConfVal value) {
        if ((strcmp(value.strval, "true") == 0) || (value.intval > 0)) {
                writeLog("Kafka avro scheme enabled.", 0, 1);
                writeLog("Using avro is an optional add on and you might need to recompile Almond. Make sure you know what to do.", 1, 1);
                kafkaAvro = true;
        }
        else {
                kafkaAvro = false;
		writeLog("Kafka is producing JSON, avro is not enabled.", 0, 1);
	}
}

void process_kafka_key_type(ConfVal value)
{
        if (value.strval == NULL || strlen(value.strval) <= 1) {
                return;
        }
        size_t vlen = strlen(value.strval) + 1;
        char *new_key_type = malloc(vlen);

        if (new_key_type == NULL) {
                writeLog("[mod_kafka] Failed to allocate memory for Kafka key type.", 1, 1);
                return;
        }
        snprintf(new_key_type, vlen, "%s", value.strval);
        free(kafka_key_type);
        kafka_key_type = new_key_type;
        snprintf(infostr, INFO_STR_SIZE,"[mod_kafka] Kafka key is set to: %s",kafka_key_type);
        writeLog(trim(infostr), 0, 1);
        if (strcmp(kafka_key_type, "host") == 0) {
                if (hostName == NULL) {
                        writeLog("[mod_kafka] Hostname is NULL; cannot set Kafka index key.", 1, 1);
                        return;
                }

                char *new_index_key = strdup(hostName);

                if (new_index_key == NULL) {
                        writeLog("[mod_kafka] Failed to allocate memory for Kafka index key.", 1, 1);
                        return;
                }
                free(kafka_index_key);
                kafka_index_key = new_index_key;
                writeLog("[mod_kafka] Hostname will be used as key by Kafka producer.", 0, 1);
        }
        else if (strcmp(kafka_key_type, "id") == 0) {
                writeLog("[mod_kafka] Kafka will use id as index, which is not yet implemented.", 1, 1);
        }
        else {
                free(kafka_key_type);
                kafka_key_type = NULL;
        }
}

void process_scheduler_avro_declaration(ConfVal value) {
        const char *path = (value.strval && strlen(value.strval) > 0) 
                           ? value.strval 
                           : DEFAULT_AVRO_DECLARATION_FILE;

        char *dup = safe_strdup(path, "scheduler.kafkaAvroDeclarationFile");
        if (!dup) return;

        free_and_null(&avro_declaration_file);
        avro_declaration_file = dup;

        if (!value.strval || strlen(value.strval) == 0) {
                snprintf(info, INFO_STR_SIZE, 
                         "[mod_kafka] scheduler.kafkaAvroDeclarationFile is not configured. Falling back to default: '%s'.", 
                         avro_declaration_file);
        } else {
                snprintf(info, INFO_STR_SIZE, 
                         "[mod_kafka] scheduler.kafkaAvroDeclarationFile is set to '%s'.", 
                         avro_declaration_file);
        }
        writeLog(triminfo(info), 0, 1);
}

static bool parse_conf_line(char *line, char *name_out, size_t name_len, char *val_out, size_t val_len) {
    if (!line) return false;
    triminfo(line);
    /* skip empty lines and comments */
    char *p = line;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '\0' || *p == '#' || *p == ';') return false;
    char *eq = strchr(p, '=');
    if (!eq) return false;
    *eq = '\0';
    char *name = p;
    char *value = eq + 1;
    strncpy(name_out, name, name_len);
    name_out[name_len - 1] = '\0';
    triminfo(name_out);
    strncpy(val_out, value, val_len);
    val_out[val_len - 1] = '\0';
    triminfo(val_out);
    return true;
}

static int getKafkaConfiguration() {
        char* line = NULL;
        size_t len = 0;
        ssize_t read;
        FILE *fp = NULL;
        fp = fopen(configFile, "r");
        char confName[MAX_STRING_SIZE] = "";
        char confValue[MAX_STRING_SIZE] = "";

        if (fp == NULL)
        {
                perror("Error while opening the Kafka configuration file.\n");
                writeLog("Error opening Kafka configuration file.", 2, 1);
                exit(EXIT_FAILURE);
        }

        while ((read = getline(&line, &len, fp)) != -1) {
                if (!parse_conf_line(line, confName, sizeof(confName), confValue, sizeof(confValue))) continue;
                ConfVal cvu;
                cvu.intval = strtol(triminfo(confValue), NULL, 0);
                cvu.strval = triminfo(confValue);
                for (int i = 0; i < sizeof(kafka_entries)/sizeof(ConfigEntry);i++) {
                        if (strcmp(confName, kafka_entries[i].name) == 0) {
                                kafka_entries[i].process(cvu);
                                break;
                        }
                }
        }
        fclose(fp);
        fp = NULL;
        if (line){
                free(line);
                line = NULL;
        }
        if (config_k_memalloc_fails > 0) {
                config_k_memalloc_fails = 0;
                return 2;
        }
        return 0;
}

static void initConfigFile() {
        if (configFile == NULL) {
                char *dup = safe_strdup("/etc/almond/kafka.conf", "configFile");
                if (dup != NULL) {
                        configFile = dup;
                } else {
                        fprintf(stderr, "[mod_kafka] Memory allocation failed for Kafka config file.");
                        writeLog("[mod_kafka] Memory allocation failed for Kafka config file.", 2, 0);
                }
        }
}

void setKafkaConfigFile(const char* configPath) {
        if (configPath == NULL) return;
        char *dup = safe_strdup(configPath, "configFile");
        if (dup == NULL) {
                fprintf(stderr, "[mod_kafka] Failed to allocate memory for config file\n");
                writeLog("[mod_kafka] Memory allocation failed in 'setConfigFile'.", 2, 0);
                writeLog("[mod_kafka] Could not change config file value.", 1, 0);
                return;
        }
        if (configFile != NULL) free_and_null(&configFile);
        configFile = dup;
}

void setKafkaTopic(const char* topicName) {
        if (topicName == NULL) return;
        char *dup = safe_strdup(topicName, "kafka.topic");
        if (dup == NULL) {
                fprintf(stderr, "[mod_kafka] Failed to allocate memory for topic name.\n");     
                writeLog("[mod_kafka] Memory allocation failed in 'setKafkaTopic'.", 2, 0);
                writeLog("[mod_kafka] Could not change topic name.", 1, 0);
                return;
        }
        if (topic != NULL) free_and_null(&topic);
        topic = dup;
}

char* getKafkaTopic(void) {
        return topic;
}

int loadKafkaConfig() {
        initConfigFile();
        if (configFile == NULL) return 2;
        if (getKafkaConfiguration() == 0) {
                if (requirements_met == 2) {
                        return 0;
                }
        }
        return 1;
}

static void dr_msg_cb(rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque) {
	char info[810];
	if (rkmessage->err) {
                fprintf(stderr, "%% Message delivery failed: %s\n",
                        rd_kafka_err2str(rkmessage->err));
		snprintf(info, 810, "%% Message delivery failed: %s", rd_kafka_err2str(rkmessage->err));
		writeLog(triminfo(info), 1, 0);
	}
        else {
		snprintf(info, 810, "%% Message delivered (%zu bytes, "
                        "partition %" PRId32 ")",
			rkmessage->len, rkmessage->partition);
		writeLog(triminfo(info), 0, 0);
	}
        /* The rkmessage is destroyed automatically by librdkafka */
}

static int set_kafka_conf(rd_kafka_conf_t *conf, const char *key, const char *value) {
        char errstr[512];
        if (rd_kafka_conf_set(conf, key, value, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
		printf("[mod_kafka] %s\n", errstr);
                writeLog(errstr, 1, 0);
                return 1;
        }
        return 0;
}

int init_kafka_producer() {
        char acks_str[8];
        char value_str[32];
        char errstr[512];

        if (acks == -1) {
                strcpy(acks_str, "all");
        }
        else {
                snprintf(acks_str, sizeof(acks_str), "%d", acks);
        }

        rd_kafka_conf_t *conf = rd_kafka_conf_new();
        if (!conf) return 1;

        if (rd_kafka_conf_set(conf, "bootstrap.servers", brokers, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                writeLog(errstr, 1, 0);
                goto fail_conf;
        }
        if (set_kafka_conf(conf, "bootstrap.servers", brokers)) goto fail_conf;
        if (set_kafka_conf(conf, "client.id", kafka_client_id)) goto fail_conf;
        snprintf(value_str, sizeof(value_str), "%d", metadata_request_timeout);
        if (set_kafka_conf(conf, "metadata.max.age.ms", value_str)) goto fail_conf;
        snprintf(value_str, sizeof(value_str), "%d", linger_ms);
        if (set_kafka_conf(conf, "linger.ms", value_str)) goto fail_conf;
        snprintf(value_str, sizeof(value_str), "%d", queue_buffering_max_messages);
        if (set_kafka_conf(conf, "queue.buffering.max.messages", value_str)) goto fail_conf;
        if (set_kafka_conf(conf, "compression.codec", kafka_compression)) goto fail_conf;
        snprintf(value_str, sizeof(value_str), "%d", message_max);
        if (set_kafka_conf(conf, "message.max.bytes", value_str)) goto fail_conf;
        snprintf(value_str, sizeof(value_str), "%d", message_copy_max);
        if (set_kafka_conf(conf, "message.copy.max.bytes", value_str)) goto fail_conf;
        snprintf(value_str, sizeof(value_str), "%d", request_timeout);
        if (set_kafka_conf(conf, "request.timeout.ms", value_str)) goto fail_conf;
        snprintf(value_str, sizeof(value_str), "%d", message_timeout_ms);
        if (set_kafka_conf(conf, "delivery.timeout.ms", value_str)) goto fail_conf;
        snprintf(value_str, sizeof(value_str), "%d", retry_backoff);
        if (set_kafka_conf(conf, "retry.backoff.ms", value_str)) goto fail_conf;
        snprintf(value_str, sizeof(value_str), "%d", statistics_interval);
        if (set_kafka_conf(conf, "statistics.interval.ms", value_str)) goto fail_conf;
        const char *log_close_str = kafka_log_connection_close ? "true" : "false";
        if (set_kafka_conf(conf, "log.connection.close", log_close_str)) goto fail_conf;
        if (set_kafka_conf(conf, "partitioner", kafka_partitioner)) goto fail_conf;
        if (enable_idempotence) { // and transactions!?
                const char *enable_idempotence_str = enable_idempotence ? "true" : "false";
                if (set_kafka_conf(conf, "enable.idempotence", enable_idempotence_str)) goto fail_conf;
                if (acks != -1) {
                        writeLog("[mod_kafka] Setting kafka.acks to 'all' since you enabled idempotency.", 1, 0);
                        snprintf(acks_str, sizeof(acks_str), "%s", "all");
                }
                if (set_kafka_conf(conf, "acks", acks_str)) goto fail_conf;
                if (retries > 0) {
                        snprintf(value_str, sizeof(value_str), "%d", retries);
                        if (set_kafka_conf(conf, "retries", value_str)) goto fail_conf;
                }
                else {
                        writeLog("[mod_kafka] Setting kafka.retries to 5 since you enabled idempotency.", 1, 0);
                        if (set_kafka_conf(conf, "retries", "5")) goto fail_conf;
                }
                if (max_in_flight_requests <= 5) {
                        snprintf(value_str, sizeof(value_str), "%d", max_in_flight_requests);
                        if (set_kafka_conf(conf, "max.in.flight.requests.per.connection", value_str)) goto fail_conf;
                }
                else {
                        writeLog("[mod_kafka] Setting kafka.max.in.flight.requests.per.connection to 5 since idempotency is enabled.", 1, 0);
                        if (set_kafka_conf(conf, "max.in.flight.requests.per.connection", "5")) goto fail_conf;
                }
                if (transactional_id != NULL) {
                        if (set_kafka_conf(conf, "transactional.id", transactional_id)) goto fail_conf;
			snprintf(value_str, sizeof(value_str), "%d", timeout_ms);
                        if (set_kafka_conf(conf, "transaction.timeout.ms", value_str)) goto fail_conf;
                        use_transactions = true;
                }
        }
 	else {
                if (set_kafka_conf(conf, "acks", acks_str)) goto fail_conf;
                snprintf(value_str, sizeof(value_str), "%d", retries);
                if (set_kafka_conf(conf, "retries", value_str)) goto fail_conf;
                snprintf(value_str, sizeof(value_str), "%d", max_in_flight_requests);
                if (set_kafka_conf(conf, "max.in.flight.requests.per.connection", value_str)) goto fail_conf;
        }
        if (kafka_security_protocol != NULL) {
                if (set_kafka_conf(conf, "security.protocol", kafka_security_protocol)) goto fail_conf;
                if (kafka_sasl_mechanisms != NULL) {
                        if ((strcmp(kafka_security_protocol, "SASL_PLAINTEXT") == 0) || (strcmp(kafka_security_protocol,  "SASL_SSL") == 0)) {
                                writeLog("[mod_kafka] kafka.sasl.mechanisms requiere SASL_SSL or SASL_PLAINTEXT which is not set.", 1, 1);
                                snprintf(info, INFO_STR_SIZE, "[mod_kafka] You need to change kafka.security.protocol currently set as '%s'.", kafka_security_protocol);
                                writeLog(triminfo(info), 1, 0);
                                writeLog("[mod_kafka] Almond will not initiate kafka.sasl.mechanisms.", 0, 0);
                        }
                        else {
                                if (set_kafka_conf(conf, "sasl.mechanisms", kafka_sasl_mechanisms)) goto fail_conf;
                                if (set_kafka_conf(conf, "sasl.username", kafka_sasl_username)) goto fail_conf;
                                if (set_kafka_conf(conf, "sasl.password", kafka_sasl_password)) goto fail_conf;
                        }
                }
                if (kafkaCALocation != NULL) {
                        if (set_kafka_conf(conf, "ssl.ca.location", kafkaCALocation)) goto fail_conf;
                }
        }
        if (kafkaSSLCertificate != NULL) {
                if (set_kafka_conf(conf, "ssl.certificate.location", kafkaSSLCertificate)) goto fail_conf;
                if (kafka_SSLKey != NULL) {
                        if (set_kafka_conf(conf, "ssl.key.location", kafka_SSLKey)) goto fail_conf;
                }
        }
	snprintf(value_str, sizeof(value_str), "%d", kafka_socket_timeout);
        if (set_kafka_conf(conf, "socket.timeout.ms", value_str)) goto fail_conf;
        /* Remove deprecated, use request.timeout.ms, delivery.timeout.ms and retries instead
        snprintf(value_str, sizeof(value_str), "%d", kafka_socket_blocking);
        if (set_kafka_conf(conf, "socket.blocking.max.ms", value_str)) goto fail_conf; */

        rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);

        global_producer = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
        conf = NULL;
        if (!global_producer) {
                writeLog(errstr, 2, 0);
                goto fail_conf;
        }
        if (use_transactions) {
                rd_kafka_error_t *init_error = rd_kafka_init_transactions(global_producer, timeout_ms);
        if (init_error) {
            writeLog(rd_kafka_error_string(init_error), 2, 0);
            rd_kafka_error_destroy(init_error);
            goto fail_conf;
        }
        }
        return 0;

fail_conf:
        /* rd_kafka_new consumes conf, even when it fails. */
        if (conf) rd_kafka_conf_destroy(conf);
        if (global_producer) {
            rd_kafka_destroy(global_producer);
            global_producer = NULL;
        }
        return 1;
}

int send_message_to_gkafka(const char *payload) {
    size_t plen = strlen(payload);
    rd_kafka_resp_err_t err = RD_KAFKA_RESP_ERR_NO_ERROR;
    int aborted = 0;
    rd_kafka_error_t *kerr;

    if (use_transactions) {
        kerr = rd_kafka_begin_transaction(global_producer);
        if (kerr) {
            writeLog(rd_kafka_error_string(kerr), 1, 0);
            rd_kafka_error_destroy(kerr);
            return -1;
        }
    }

    int attempts = 0;
    const int max_attempts = 5;

    do {
	if (kafka_key_type != NULL && kafka_index_key != NULL) {
        	size_t index_len = strlen(kafka_index_key);
                err = rd_kafka_producev(
			global_producer,
                         RD_KAFKA_V_TOPIC(topic),
                         RD_KAFKA_V_PARTITION(RD_KAFKA_PARTITION_UA),
                         RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
                         RD_KAFKA_V_VALUE((void *)payload, plen),
                         RD_KAFKA_V_KEY((void *)kafka_index_key, index_len),
                         RD_KAFKA_V_OPAQUE(NULL),
                         RD_KAFKA_V_END);
        }
        else {
        	err = rd_kafka_producev(
			global_producer,
                	RD_KAFKA_V_TOPIC(topic),
                	RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
                	RD_KAFKA_V_VALUE((void *)payload, plen),
                	RD_KAFKA_V_OPAQUE(NULL),
               	 	RD_KAFKA_V_END);
	}

        if (err == RD_KAFKA_RESP_ERR__QUEUE_FULL) {
            if (use_transactions) {
                rd_kafka_abort_transaction(global_producer, timeout_ms);
                aborted = 1;
            }
            rd_kafka_poll(global_producer, 1000);
            attempts++;
        }
        else if (err) {
            writeLog(rd_kafka_err2str(err), 1, 0);
            break;
        }
        else {
            writeLog("Message enqueued", 0, 0);
            break;
        }

    } while (attempts < max_attempts);

    if (use_transactions) {
        if (!aborted) {
            kerr = rd_kafka_commit_transaction(global_producer, timeout_ms);
            if (kerr) {
                writeLog(rd_kafka_error_string(kerr), 1, 0);
                rd_kafka_error_destroy(kerr);
                return -1;
            }
        }
    } else {
        rd_kafka_poll(global_producer, 0);
        rd_kafka_flush(global_producer, timeout_ms);
    }

    return err;
}

int send_avro_message_to_gkafka(char *brokers, char *topic, const GKafkaMessage *msg) {
    // brokers is ignored here because global_producer is already connected to them
    return serialize_and_send(topic, msg);
}

int send_ssl_avro_message_to_gkafka(char *brokers, char *ca, char *cert, char *key, char *topic, const GKafkaMessage *msg) {
    // ca, cert, key are already in globals used by serialize_and_send
    return serialize_and_send(topic, msg);
}

int send_message_to_kafka(char *brokers, char *topic, char *payload) {
        char errstr[512];
        char info[812];
        rd_kafka_t *producer;        /* Producer instance handle */
        rd_kafka_conf_t *conf; /* Temporary configuration object */

        conf = rd_kafka_conf_new();
        if (rd_kafka_conf_set(conf, "bootstrap.servers", brokers, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                fprintf(stderr,"%s\n", errstr);
                writeLog(errstr, 1, 0);
        }

        rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);
        producer = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
        if (!producer) {
                fprintf(stderr, "Failed to create producer: %s\n", errstr);
                snprintf(info, 810, "Failed to create producer: %s", errstr);
                writeLog(triminfo(info), 2, 0);
                return 1;
        }

        if (rd_kafka_brokers_add(producer, brokers) == 0) {
                fprintf(stderr, "Failed to add brokers: %s\n", rd_kafka_err2str(rd_kafka_last_error()));
                snprintf(info, 810, "Failed to add brokers: %s", rd_kafka_err2str(rd_kafka_last_error()));
                writeLog(triminfo(info), 2, 0);
                rd_kafka_destroy(producer);
                return 1;
        }

        size_t plen = strlen(payload);
        (void)plen;
        int attempts = 0; const int max_attempts = 5;
        rd_kafka_resp_err_t err = RD_KAFKA_RESP_ERR__UNKNOWN;
        do {
		if (kafka_key_type != NULL && kafka_index_key != NULL) {
                 	size_t index_len = strlen(kafka_index_key);
               		err = rd_kafka_producev(producer,
                         	RD_KAFKA_V_TOPIC(topic),
                         	RD_KAFKA_V_PARTITION(RD_KAFKA_PARTITION_UA),
                         	RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
                         	RD_KAFKA_V_VALUE((void *)payload, plen),
                         	RD_KAFKA_V_KEY((void *)kafka_index_key, index_len),
                         	RD_KAFKA_V_OPAQUE(NULL),
                         	RD_KAFKA_V_END);
         	}
         	else {
                	err = rd_kafka_producev(producer,
                         	RD_KAFKA_V_TOPIC(topic),
                         	RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
                         	RD_KAFKA_V_VALUE((void *)payload, plen),
                         	RD_KAFKA_V_OPAQUE(NULL),
                         	RD_KAFKA_V_END);
         	}
                if (err) {
                        snprintf(info, 810, "%% Failed to produce topic %s: %s", topic, rd_kafka_err2str(err));
                        writeLog(triminfo(info), 1, 0);
                        if (err == RD_KAFKA_RESP_ERR__QUEUE_FULL) {
                                rd_kafka_poll(producer, 1000);
                                attempts++;
                                if (attempts < max_attempts) continue;
                                else break;
                        }
                }
                else {
                        snprintf(info, 810, "%% Enqueued message (%zu bytes) "
                                       "for topic %s",
                                       plen, topic);
                        writeLog(triminfo(info), 0, 0);
                }
                rd_kafka_poll(producer, 0);
        } while (attempts < max_attempts);
        writeLog("Flushing final Kafka messages...", 0, 0);
        rd_kafka_flush(producer, 10 * 1000 /* wait for max 10 seconds */);
        if (rd_kafka_outq_len(producer) > 0) {
                snprintf(info, 810, "%% %d message(s) were not delivered", rd_kafka_outq_len(producer));
                writeLog(triminfo(info), 1, 0);
        }
        /* Destroy the producer instance */
        rd_kafka_destroy(producer);
        return 0;
}

int send_ssl_message_to_kafka(char *brokers, char *cacertificate, char *certificate, char *key, char *topic,char *payload) {
        char errstr[512];
        char info[812];
        rd_kafka_t *producer;        /* Producer instance handle */
        rd_kafka_conf_t *conf; /* Temporary configuration object */

        conf = rd_kafka_conf_new();
        if (rd_kafka_conf_set(conf, "bootstrap.servers", brokers, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                fprintf(stderr,"%s\n", errstr);
                writeLog(errstr, 1, 0);
        }
        rd_kafka_conf_set(conf, "enable.ssl.certificate.verification", "false", errstr, sizeof(errstr));
        rd_kafka_conf_set(conf, "security.protocol", "SSL", errstr, sizeof(errstr));
        if (rd_kafka_conf_set(conf, "ssl.ca.location", cacertificate, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                fprintf(stderr,"%s\n", errstr);
                writeLog(errstr, 1, 0);
        }
        if (rd_kafka_conf_set(conf, "ssl.certificate.location", certificate, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                fprintf(stderr,"%s\n", errstr);
                writeLog(errstr, 1, 0);
        }
        if (rd_kafka_conf_set(conf, "ssl.key.location", key, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                fprintf(stderr,"%s\n", errstr);
                writeLog(errstr, 1, 0);
        }

        rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);
        producer = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
        if (!producer) {
                fprintf(stderr, "Failed to create producer: %s\n", errstr);
                snprintf(info, 810, "Failed to create producer: %s", errstr);
                writeLog(triminfo(info), 2, 0);
                return 1;
        }

        if (rd_kafka_brokers_add(producer, brokers) == 0) {
                fprintf(stderr, "Failed to add brokers: %s\n", rd_kafka_err2str(rd_kafka_last_error()));
                snprintf(info, 810, "Failed to add brokers: %s", rd_kafka_err2str(rd_kafka_last_error()));
                writeLog(triminfo(info), 2, 0);
                rd_kafka_destroy(producer);
                return 1;
        }
        size_t plen = strlen(payload);
        (void)plen;
        int attempts = 0; const int max_attempts = 5;
        rd_kafka_resp_err_t err = RD_KAFKA_RESP_ERR__UNKNOWN;
        do {
		if (kafka_key_type != NULL && kafka_index_key != NULL) {
			size_t index_len = strlen(kafka_index_key);
                	err = rd_kafka_producev(producer,
                         	RD_KAFKA_V_TOPIC(topic),
                         	RD_KAFKA_V_PARTITION(RD_KAFKA_PARTITION_UA),
                         	RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
                         	RD_KAFKA_V_VALUE((void *)payload, plen),
                         	RD_KAFKA_V_KEY((void *)kafka_index_key, index_len),
                         	RD_KAFKA_V_OPAQUE(NULL),
                         	RD_KAFKA_V_END);
         	}
         	else {
                 	err = rd_kafka_producev(producer,
                         	RD_KAFKA_V_TOPIC(topic),
                         	RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
                         	RD_KAFKA_V_VALUE((void *)payload, plen),
                         	RD_KAFKA_V_OPAQUE(NULL),
                         	RD_KAFKA_V_END);
         	}

                if (err) {
                        snprintf(info, 810, "%% Failed to produce topic %s: %s", topic, rd_kafka_err2str(err));
                        writeLog(triminfo(info), 1, 0);
                        if (err == RD_KAFKA_RESP_ERR__QUEUE_FULL) {
                                rd_kafka_poll(producer, 1000);
                                attempts++;
                                if (attempts < max_attempts) continue;
                                else break;
                        }
                }
                else {
                        snprintf(info, 810, "%% Enqueued message (%zu bytes) "
                                       "for topic %s",
                                       plen, topic);
                        writeLog(triminfo(info), 0, 0);
                }
        } while (attempts < max_attempts);
                rd_kafka_poll(producer, 0);
        writeLog("Flushing final Kafka messages...", 0, 0);
        rd_kafka_flush(producer, 10 * 1000 /* wait for max 10 seconds */);
        if (rd_kafka_outq_len(producer) > 0) {
                snprintf(info, 810, "%% %d message(s) were not delivered", rd_kafka_outq_len(producer));
                writeLog(triminfo(info), 1, 0);
        }
        /* Destroy the producer instance */
        rd_kafka_destroy(producer);
        return 0;
}

/* Shared legacy sender, used by the non-global producer entry points.
   Do not pass borrowed data pointers to cleanup functions. */
static int send_legacy_avro(char *bootstrap, const char *schema_registration_name,
                            const char *ca, const char *cert,
                            const char *keyfile, char *destination,
                            const char *name, const char *id, const char *tag,
                            const char *lastChange, const char *lastRun,
                            const char *dataName, const char *nextRun,
                            const char *pluginName, const char *pluginOutput,
                            const char *pluginStatus, const char *pluginStatusChanged,
                            int pluginStatusCode) {
    char errstr[512] = {0};
    int result = -1;
    serdes_t *serdes = NULL;
    serdes_schema_t *schema = NULL;
    char *schema_buf = NULL;
    void *buffer = NULL;
    size_t schema_len = 0, length = 0;
    rd_kafka_conf_t *conf = NULL;
    rd_kafka_t *producer = NULL;
    avro_value_iface_t *iface = NULL;
    avro_value_t record, data, v;
    int record_ready = 0;

    if (!schemaRegistryUrl || !bootstrap || !destination) return -1;
    serdes_conf_t *sconf = serdes_conf_new(errstr, sizeof(errstr),
                                          "schema.registry.url", schemaRegistryUrl, NULL);
    if (!sconf) return -1;
    serdes = serdes_new(sconf, NULL, 0);
    if (!serdes) return -1; /* libserdes owns sconf */

    const char *filepath = avro_declaration_file ? avro_declaration_file : DEFAULT_AVRO_DECLARATION_FILE;
    schema_buf = load_file(filepath, &schema_len);
    if (!schema_buf) goto cleanup;
    schema = serdes_schema_add(serdes, schema_registration_name, -1,
                                                schema_buf, schema_len,
                                                errstr, sizeof(errstr));
    if (!schema) {
        writeLog(errstr, 2, 0);
        goto cleanup;
    }
    avro_schema_t avro_schema = serdes_schema_avro(schema);
    iface = avro_generic_class_from_schema(avro_schema);
    if (!iface || avro_generic_value_new(iface, &record) != 0) goto cleanup;
    record_ready = 1;

    avro_value_get_by_name(&record, "name", &v, NULL); avro_value_set_string(&v, name);
    avro_value_get_by_name(&record, "id", &v, NULL); avro_value_set_string(&v, id);
    avro_value_get_by_name(&record, "tag", &v, NULL); avro_value_set_string(&v, tag);
    avro_value_get_by_name(&record, "data", &data, NULL);
    avro_value_get_by_name(&data, "lastChange", &v, NULL); avro_value_set_string(&v, lastChange);
    avro_value_get_by_name(&data, "lastRun", &v, NULL); avro_value_set_string(&v, lastRun);
    avro_value_get_by_name(&data, "name", &v, NULL); avro_value_set_string(&v, dataName);
    avro_value_get_by_name(&data, "nextRun", &v, NULL); avro_value_set_string(&v, nextRun);
    avro_value_get_by_name(&data, "pluginName", &v, NULL); avro_value_set_string(&v, pluginName);
    avro_value_get_by_name(&data, "pluginOutput", &v, NULL); avro_value_set_string(&v, pluginOutput);
    avro_value_get_by_name(&data, "pluginStatus", &v, NULL); avro_value_set_string(&v, pluginStatus);
    avro_value_get_by_name(&data, "pluginStatusChanged", &v, NULL); avro_value_set_string(&v, pluginStatusChanged);
    avro_value_get_by_name(&data, "pluginStatusCode", &v, NULL); avro_value_set_int(&v, pluginStatusCode);

    if (serdes_schema_serialize_avro(schema, &record, &buffer, &length,
                                     errstr, sizeof(errstr)) != SERDES_ERR_OK) {
        writeLog(errstr, 2, 0);
        goto cleanup;
    }
    conf = rd_kafka_conf_new();
    if (!conf) goto cleanup;
    if (rd_kafka_conf_set(conf, "bootstrap.servers", bootstrap, errstr,
                          sizeof(errstr)) != RD_KAFKA_CONF_OK) goto cleanup;
    if (ca || cert || keyfile) {
        if (rd_kafka_conf_set(conf, "security.protocol", "SSL", errstr,
                              sizeof(errstr)) != RD_KAFKA_CONF_OK) goto cleanup;
        /* Preserve legacy SSL verification setting. Review separately before production. */
        rd_kafka_conf_set(conf, "enable.ssl.certificate.verification", "false", errstr, sizeof(errstr));
        if (ca && rd_kafka_conf_set(conf, "ssl.ca.location", ca, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) goto cleanup;
        if (cert && rd_kafka_conf_set(conf, "ssl.certificate.location", cert, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) goto cleanup;
        if (keyfile && rd_kafka_conf_set(conf, "ssl.key.location", keyfile, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) goto cleanup;
    }
    rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);
    producer = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    conf = NULL; /* rd_kafka_new consumes the configuration, success or failure */
    if (!producer) goto cleanup;
    if (rd_kafka_brokers_add(producer, bootstrap) == 0) goto cleanup;

    rd_kafka_resp_err_t err = RD_KAFKA_RESP_ERR__UNKNOWN;
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (kafka_key_type && kafka_index_key) {
            err = rd_kafka_producev(producer,
                    RD_KAFKA_V_TOPIC(destination), RD_KAFKA_V_PARTITION(RD_KAFKA_PARTITION_UA),
                    RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY), RD_KAFKA_V_VALUE(buffer, length),
                    RD_KAFKA_V_KEY(kafka_index_key, strlen(kafka_index_key)), RD_KAFKA_V_END);
        } else {
            err = rd_kafka_producev(producer, RD_KAFKA_V_TOPIC(destination),
                    RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY), RD_KAFKA_V_VALUE(buffer, length),
                    RD_KAFKA_V_END);
        }
        if (err != RD_KAFKA_RESP_ERR__QUEUE_FULL) break;
        rd_kafka_poll(producer, 1000);
    }
    if (err == RD_KAFKA_RESP_ERR_NO_ERROR) {
        rd_kafka_poll(producer, 0);
        result = rd_kafka_flush(producer, 10000) == RD_KAFKA_RESP_ERR_NO_ERROR ? 0 : -1;
    } else {
        writeLog(rd_kafka_err2str(err), 1, 0);
    }

cleanup:
    if (conf) rd_kafka_conf_destroy(conf);
    if (producer) rd_kafka_destroy(producer);
    if (record_ready) avro_value_decref(&record);
    if (iface) avro_value_iface_decref(iface);
    free(buffer);
    free(schema_buf);
    if (schema) serdes_schema_destroy(schema);
    if (serdes) serdes_destroy(serdes);
    return result;
}

int send_avro_message_to_kafka(char *brokers, char *topic, const char *name,
        const char *id, const char *tag, const char *lastChange, const char *lastRun,
        const char *dataName, const char *nextRun, const char *pluginName,
        const char *pluginOutput, const char *pluginStatus,
        const char *pluginStatusChanged, int pluginStatusCode) {
    return send_legacy_avro(brokers, schemaName, NULL, NULL, NULL, topic, name, id, tag,
                            lastChange, lastRun, dataName, nextRun, pluginName,
                            pluginOutput, pluginStatus, pluginStatusChanged, pluginStatusCode);
}

int send_ssl_avro_message_to_kafka(char *brokers, char *cacertificate,
        char *certificate, char *key, char *topic, const char *name,
        const char *id, const char *tag, const char *lastChange, const char *lastRun,
        const char *dataName, const char *nextRun, const char *pluginName,
        const char *pluginOutput, const char *pluginStatus,
        const char *pluginStatusChanged, int pluginStatusCode) {
    return send_legacy_avro(brokers, "PluginStatusMessage", cacertificate, certificate, key, topic,
                            name, id, tag, lastChange, lastRun, dataName, nextRun,
                            pluginName, pluginOutput, pluginStatus,
                            pluginStatusChanged, pluginStatusCode);
}

static void shutdown_kafka_producer() {
	if (global_producer != NULL) {
		writeLog("Flushing final Kafka messages...", 0, 0);
        	rd_kafka_flush(global_producer, 10000);
		if (rd_kafka_outq_len(global_producer) > 0) {
                	snprintf(info, INFO_STR_SIZE, "%% %d message(s) were not delivered", rd_kafka_outq_len(global_producer));
                	writeLog(triminfo(info), 1, 0);
        	}
        	rd_kafka_destroy(global_producer);
		global_producer = NULL;
	}
}

void free_kafka_memalloc() {
        shutdown_kafka_producer();
	free_and_null(&brokers);
        free_and_null(&topic);
        free_and_null(&kafka_client_id);
        free_and_null(&kafka_partitioner);
        free_and_null(&transactional_id);
        free_and_null(&kafka_compression);
        free_and_null(&kafkaCALocation);
        free_and_null(&kafkaSSLCertificate);
        free_and_null(&kafka_SSLKey);
        free_and_null(&kafka_sasl_mechanisms);
        free_and_null(&kafka_sasl_username);
        free_and_null(&kafka_sasl_password);
        free_and_null(&kafka_security_protocol);
        free_and_null(&kafka_key_type);
        free_and_null(&kafka_index_key);
        free_and_null(&avro_declaration_file);
        free_and_null(&configFile);
}
