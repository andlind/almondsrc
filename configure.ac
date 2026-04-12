AC_INIT([almond], [0.9.28], [andreas.lindell@almondmonitor.com])
AM_INIT_AUTOMAKE([-Wall foreign])
AC_CONFIG_HEADERS([config.h])
AC_PROG_CC

CPPFLAGS="$CPPFLAGS -I/usr/include -I/usr/local/include"
LDFLAGS="$LDFLAGS -L/usr/lib64 -L/usr/local/lib -L/usr/lib"

AS_MKDIR_P(/etc/almond)
AS_MKDIR_P(/opt/almond/data)
# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h sys/socket.h sys/types.h sys/stat.h math.h signal.h pthread.h time.h netdb.h unistd.h ctype.h string.h errno.h dirent.h stdlib.h stdio.h jwt.h curl/curl.h json-c/json.h openssl/ssl.h openssl/x509.h])

# Check jwt module
AC_CHECK_LIB([jwt], [jwt_new], [
    JWT_LIBS="-ljwt"
], [
    AC_MSG_ERROR([libjwt not found. Install libjwt-devel.])
])
AC_SUBST(JWT_LIBS)
# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STAT
AC_CHECK_FUNCS([memmove])
AC_CHECK_FUNCS([strdup])

AC_ARG_ENABLE([avro],
  [AS_HELP_STRING([--enable-avro], [Enable Avro support])],
  [],
  [])

if test "x$enable_avro" = "x"; then
  enable_avro=no
fi

AC_ARG_ENABLE([kafka],
  [AS_HELP_STRING([--disable-kafka], [Disable Kafka support])],
  [],
  [])

if test "x$enable_kafka" = "x"; then
  enable_kafka=yes
fi

AM_CONDITIONAL([USE_MINIMAL], [test "x$enable_kafka" != "xyes"])
AM_CONDITIONAL([USE_KAFKA], [test "x$enable_kafka" = "xyes"])
AM_CONDITIONAL([USE_AVRO], [test "x$enable_avro" = "xyes"])
AM_CONDITIONAL([USE_KAFKA_ONLY], [test "x$enable_kafka" = "xyes" && test "x$enable_avro" != "xyes"])

if test "x$enable_avro" = "xyes" && test "x$enable_kafka" != "xyes"; then
  AC_MSG_ERROR([Avro support requires Kafka. Please enable Kafka or disable Avro.])
fi

if test "x$enable_kafka" = "xyes"; then
  AC_MSG_NOTICE([Kafka support ENABLED])
  AC_CHECK_HEADERS([librdkafka/rdkafka.h], [], [
    AC_MSG_ERROR([Kafka header not found. Install librdkafka-dev or disable Kafka support.])
  ])
  AC_CHECK_LIB([rdkafka], [rd_kafka_new], [], [
    AC_MSG_ERROR([Kafka library not found. Install librdkafka-dev or disable Kafka support.])
  ])
else
  AC_MSG_NOTICE([Kafka support DISABLED])
  AC_DEFINE([USE_MINIMAL], [1], [Disable KAFKA support])
fi

if test "x$enable_avro" = "xyes"; then
  CPPFLAGS="$CPPFLAGS -I/usr/include/libserdes"
  LIBS="$LIBS -lserdes -lavro"
  AC_MSG_NOTICE([Avro support ENABLED])
  AC_CHECK_HEADERS([avro.h], [], [
    AC_MSG_ERROR([Avro header not found. Install libavro-dev or disable Avro support.])
  ])

  AC_CHECK_HEADERS([libserdes/serdes-avro.h], [], [
    AC_MSG_ERROR([Serdes header not found. Try setting CPPFLAGS with the correct include path, install libserdes-dev or disable Avro support.])
  ])

  AC_CHECK_LIB([avro], [avro_schema_from_json], [], [
    AC_MSG_ERROR([Avro library not found. Install libavro-dev or disable Avro support.])
  ])
  
  AC_DEFINE([HAVE_SERDES], [1], [Define if Serdes is available])
  AC_DEFINE([USE_AVRO], [1], [Enable AVRO support])
else
  AC_MSG_NOTICE([Avro support DISABLED])
fi

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

