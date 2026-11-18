AC_INIT([almond], [26.3.1], [andreas.lindell@almondmonitor.com])
AM_INIT_AUTOMAKE([-Wall foreign])
AC_CONFIG_HEADERS([config.h])
AC_PROG_CC

# Static metadata already supplies the complete ordered link flags.
# Dynamic checks retain AC_CHECK_LIB's normal behavior.
AC_DEFUN([ALMOND_CHECK_LIB], [
  AS_IF([test "x$enable_static" = "xyes"], [
    AC_MSG_CHECKING([for $2 with static dependencies])
    AC_LINK_IFELSE([AC_LANG_CALL([], [$2])],
      [AC_MSG_RESULT([yes])
       $3],
      [AC_MSG_RESULT([no])
       $4])
  ], [AC_CHECK_LIB([$1], [$2], [$3], [$4])])
])

CPPFLAGS="$CPPFLAGS -I/usr/include -I/usr/local/include"
LDFLAGS="$LDFLAGS -L/usr/lib64 -L/usr/local/lib -L/usr/lib"

AS_MKDIR_P(/etc/almond)
AS_MKDIR_P(/opt/almond/data)

# Select features before resolving static dependency metadata.
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

AC_ARG_ENABLE([static],
  [AS_HELP_STRING([--enable-static], [Link a fully static Almond executable (default: no)])],
  [], [enable_static=no])
AS_CASE([$enable_static], [yes|no], [],
  [AC_MSG_ERROR([--enable-static accepts only yes or no])])
AM_CONDITIONAL([STATIC_BUILD], [test "x$enable_static" = "xyes"])
AC_ARG_VAR([STATIC_LIBS], [Complete static link flags, including transitive dependencies; overrides pkg-config])
AC_ARG_VAR([STATIC_PACKAGES], [Override pkg-config module list for the selected static build])
AC_ARG_VAR([STATIC_EXTRA_LIBS], [Additional libraries missing from static pkg-config metadata])
AC_ARG_VAR([PKG_CONFIG], [Path to pkg-config (or a target-specific wrapper)])

AS_IF([test "x$enable_static" = "xyes"], [
  LDFLAGS="$LDFLAGS -static"
  AS_IF([test -z "$STATIC_LIBS"], [
    AC_PATH_TOOL([PKG_CONFIG], [pkg-config])
    AS_IF([test -z "$PKG_CONFIG"], [
      AC_MSG_ERROR([Static builds require pkg-config or an explicit STATIC_LIBS value.])
    ])
    AS_IF([test -z "$STATIC_PACKAGES"], [
      STATIC_PACKAGES="libjwt json-c libcurl openssl zlib"
      AS_IF([test "x$enable_kafka" = "xyes"], [
        AS_IF([$PKG_CONFIG --exists rdkafka-static],
          [STATIC_PACKAGES="rdkafka-static $STATIC_PACKAGES"],
          [STATIC_PACKAGES="rdkafka $STATIC_PACKAGES"])
      ])
      AS_IF([test "x$enable_avro" = "xyes"], [
        STATIC_PACKAGES="serdes avro-c $STATIC_PACKAGES"
      ])
    ])
    AC_MSG_CHECKING([for static dependency metadata: $STATIC_PACKAGES])
    AS_IF([$PKG_CONFIG --exists --static $STATIC_PACKAGES], [
      AC_MSG_RESULT([yes])
    ], [
      AC_MSG_RESULT([no])
      $PKG_CONFIG --print-errors --exists --static $STATIC_PACKAGES
      AC_MSG_ERROR([Install the missing pkg-config files, set PKG_CONFIG_PATH or STATIC_PACKAGES, or supply complete STATIC_LIBS.])
    ])
    STATIC_CPPFLAGS=`$PKG_CONFIG --cflags --static $STATIC_PACKAGES` ||
      AC_MSG_ERROR([Could not obtain static dependency compiler flags.])
    STATIC_LIBS=`$PKG_CONFIG --libs --static $STATIC_PACKAGES` ||
      AC_MSG_ERROR([Could not obtain static dependency linker flags.])
  ])
  STATIC_LIBS="$STATIC_LIBS $STATIC_EXTRA_LIBS -pthread -lm"
  CPPFLAGS="$CPPFLAGS $STATIC_CPPFLAGS"
  LIBS="$STATIC_LIBS $LIBS"
  AC_MSG_CHECKING([whether the compiler can link a static executable])
  AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])],
    [AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])
     AC_MSG_ERROR([Static link failed. Install static archives for all dependencies and the C runtime. See config.log; use STATIC_EXTRA_LIBS or STATIC_LIBS for missing dependencies.])])
  AC_MSG_CHECKING([for core APIs with static dependencies])
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <curl/curl.h>
#include <json-c/json.h>
#include <openssl/ssl.h>
#include <zlib.h>
  ]], [[
    CURL *curl = curl_easy_init();
    struct json_object *object = json_object_new_object();
    SSL_CTX *context = SSL_CTX_new(SSLv23_method());
    const char *version = zlibVersion();
    curl_easy_cleanup(curl);
    json_object_put(object);
    SSL_CTX_free(context);
    return version == 0;
  ]])], [AC_MSG_RESULT([yes])], [
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([Static core library link failed. See config.log for missing headers, archives or transitive dependencies.])
  ])
  AC_MSG_NOTICE([Fully static linking ENABLED])
])
AC_SUBST([STATIC_LIBS])

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h sys/socket.h sys/types.h sys/stat.h math.h signal.h pthread.h time.h netdb.h unistd.h ctype.h string.h errno.h dirent.h stdlib.h stdio.h jwt.h curl/curl.h json-c/json.h openssl/ssl.h openssl/x509.h])

# Check jwt module
ALMOND_CHECK_LIB([jwt], [jwt_new], [
    JWT_LIBS="-ljwt"
], [
    AC_MSG_ERROR([libjwt link failed. Install its development files and, for static builds, all static dependencies; see config.log.])
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

if test "x$enable_kafka" = "xyes"; then
  AC_MSG_NOTICE([Kafka support ENABLED])
  AC_CHECK_HEADERS([librdkafka/rdkafka.h], [], [
    AC_MSG_ERROR([Kafka header not found. Install librdkafka-dev or disable Kafka support.])
  ])
  ALMOND_CHECK_LIB([rdkafka], [rd_kafka_new], [], [
    AC_MSG_ERROR([Kafka library link failed. Install librdkafka development files and, for static builds, all static dependencies; see config.log.])
  ])
else
  AC_MSG_NOTICE([Kafka support DISABLED])
  AC_DEFINE([USE_MINIMAL], [1], [Disable KAFKA support])
fi

if test "x$enable_avro" = "xyes"; then
  CPPFLAGS="$CPPFLAGS -I/usr/include/libserdes"
  AS_IF([test "x$enable_static" != "xyes"], [LIBS="-lserdes -lavro $LIBS"])
  AC_MSG_NOTICE([Avro support ENABLED])
  AC_CHECK_HEADERS([avro.h], [], [
    AC_MSG_ERROR([Avro header not found. Install libavro-dev or disable Avro support.])
  ])

  AC_CHECK_HEADERS([libserdes/serdes-avro.h], [], [
    AC_MSG_ERROR([Serdes header not found. Try setting CPPFLAGS with the correct include path, install libserdes-dev or disable Avro support.])
  ])

  ALMOND_CHECK_LIB([avro], [avro_schema_from_json], [], [
    AC_MSG_ERROR([Avro library link failed. Install Avro development files and, for static builds, all static dependencies; see config.log.])
  ])
  
  AC_DEFINE([HAVE_SERDES], [1], [Define if Serdes is available])
  AC_DEFINE([USE_AVRO], [1], [Enable AVRO support])
else
  AC_MSG_NOTICE([Avro support DISABLED])
fi

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

