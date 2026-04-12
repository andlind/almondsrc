AUTOMAKE_OPTIONS = foreign

bin_PROGRAMS = almond

almond_SOURCES =
COMMON_SOURCES = plugins.c logger.c config.c jwt_validate.c

AM_CPPFLAGS = -I/usr/include
AM_LDFLAGS = -L/usr/lib64 -L/usr/local/lib -L/usr/lib

# Default flags
almond_CPPFLAGS = -I/usr/include/json_c
almond_CFLAGS = -g -Wall -std=c99 -O0
almond_LDFLAGS = -L/usr/lib/x86_64-linux-gnu -fstack-protector
almond_LDADD = -lpthread -ljson-c -lcurl -lssl -lcrypto -lz $(JWT_LIBS) 

if USE_MINIMAL
  almond_SOURCES += main_s.c api_s.c $(COMMON_SOURCES)
endif

if USE_KAFKA_ONLY
  almond_SOURCES += main.c api.c $(COMMON_SOURCES) mod_kafka.c
  almond_CPPFLAGS += -I/usr/include/librdkafka
  almond_LDADD += -lrdkafka
endif

if USE_AVRO
  almond_SOURCES += main.c api.c  $(COMMON_SOURCES) mod_avro.c
  almond_CPPFLAGS += -I/usr/include/librdkafka -I/usr/include/avro -I/usr/include/libserdes
  almond_LDADD += -lrdkafka -lserdes -lavro
endif

