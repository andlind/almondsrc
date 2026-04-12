#ifndef ALMOND_MODKAFKA_AVRO_H
#define ALMOND_MODKAFKA_AVRO_H

void setKafkaConfigFile(const char*);
void setKafkaTopic(const char*);
char* getKafkaTopic(void);
int loadKafkaConfig();
int init_kafka_producer(); 
int send_message_to_gkafka(const char*);
int send_avro_message_to_gkafka(const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, int);
int send_message_to_kafka(char*, char*, char*);
int send_ssl_message_to_kafka(char*, char*, char*, char*, char*, char*);
int send_avro_message_to_kafka(char*, char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, int);
int send_ssl_avro_message_to_kafka(char*, char*, char*, char*, char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, const char*, int);
void process_kafka_avro(ConfVal);
void free_kafka_memalloc();

#endif // ALMOND_MODKAFKA_AVRO_H 
