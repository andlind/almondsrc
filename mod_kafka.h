#ifndef ALMOND_MODKAFKA_H
#define ALMOND_MODKAFKA_H

#include <json-c/json.h>
#include "configuration.h"

typedef struct {
        const char *name;
        const char *id;
        const char *tag;
        const char *lastChange;   // Double check: NO COMMA HERE
        const char *lastRun;
        const char *dataName;
        const char *nextRun;
        const char *pluginName;   // Double check: NO COMMA HERE
        const char *pluginOutput;
        const char *pluginStatus;
        const char *pluginStatusChanged;
        int pluginStatusCode;
        struct json_object *labels;
        struct json_object *metrics;
} GKafkaMessage;

extern bool kafkaAvro;
void setKafkaConfigFile(const char*);
void setKafkaTopic(const char*);
char* getKafkaTopic(void);
int loadKafkaConfig();
int init_kafka_producer(); 
int send_message_to_gkafka(const char*);
//int send_avro_message_to_gkafka(const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, int);
int send_avro_message_to_gkafka(char*, char*, const GKafkaMessage *msg);
int send_message_to_kafka(char*, char*, char*);
int send_ssl_message_to_kafka(char*, char*, char*, char*, char*, char*);
int send_avro_message_to_kafka(char*, char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, int);
//int send_avro_message_to_kafka(char*, char*, const GKafkaMessage *msg);
int send_ssl_avro_message_to_kafka(char*, char*, char*, char*, char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, int);
void process_kafka_avro(ConfVal);
void free_kafka_memalloc();

#endif // ALMOND_MODKAFKA_H 
