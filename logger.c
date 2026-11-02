#define _POSIX_C_SOURCE 200112L  /* Enable POSIX features */
#define _GNU_SOURCE         /* Enable GNU extensions */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "logger.h"
#include "main.h"

//static FILE* fptr = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static int dockerLog = 0;

void makeTimestamp(char *buffer, size_t size) {
    time_t t = time(NULL);
    struct tm tm_data;
    localtime_r(&t, &tm_data);  // Thread-safe variant
    snprintf(buffer, size, "%d-%02d-%02d %02d:%02d:%02d",
             tm_data.tm_year + 1900,
             tm_data.tm_mon  + 1,
             tm_data.tm_mday,
             tm_data.tm_hour,
             tm_data.tm_min,
             tm_data.tm_sec);
}

const char* levelString(LogLevel level) {
    switch (level) {
        case LOG_INFO:    return "[INFO]\t";
        case LOG_WARNING: return "[WARNING]\t";
        case LOG_ERROR:   return "[ERROR]\t";
        default:          return "[DEBUG]\t";
    }
}

void initLogger() {
        char ch = '/';
        
        /*if (!logfile) {
		fprintf(stderr, "logfile is NULL in initLogger.\n");
    		return;
	}
	if (!logDir) {
    		fprintf(stderr, "logDir is NULL in initLogger.\n");
    		return;
	}*/
        if (!logfile || !logDir || shutdown_phase >= 2) {
        	return;
    	}		
        snprintf(logfile, logfile_size, "%s%c%s", logDir, ch, "almond.log");
        pthread_mutex_lock(&log_mutex);
        fptr = fopen(logfile, "a");
        if (fptr == NULL) {
                printf("Could not open '%s'\n", logfile);
                perror("Error opening logfile");
                exit(EXIT_FAILURE);
        }
        is_file_open = 1;
        pthread_cond_broadcast(&file_opened);
        pthread_mutex_unlock(&log_mutex);
        //writeLog("Logger is initiated.", 0, 0);
}

void cleanupLogger(void) {
    if (fptr) {
        fclose(fptr);
        fptr = NULL;
    }
}

void writeLog(const char *message, LogLevel level, int startup) {
    if (!message || shutdown_phase >= 2)
        return;

    char timeStamp[TIMESTAMP_SIZE];
    makeTimestamp(timeStamp, sizeof(timeStamp));

    /* Both the formatting buffer and FILE* are shared among threads.
     * Protect the entire operation, including startup log messages. */
    pthread_mutex_lock(&log_mutex);
    if (shutdown_phase >= 2 || !logmessage) {
        pthread_mutex_unlock(&log_mutex);
        return;
    }

    int bytesWritten = snprintf(logmessage, logmessage_size, "%s | %s%s",
                                timeStamp, levelString(level), message);
    if (bytesWritten < 0 || (size_t)bytesWritten >= logmessage_size) {
        pthread_mutex_unlock(&log_mutex);
        fprintf(stderr, "Log message truncated or encoding error occurred.\n");
        return;
    }

    if (startup < 1) {
        int attempts = 0;
        while (!is_file_open && !is_stopping && attempts < 3) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 2;
            pthread_cond_timedwait(&file_opened, &log_mutex, &ts);
            ++attempts;
        }
    }

    if (fptr != NULL) {
        fprintf(fptr, "%s\n", logmessage);
        if (dockerLog)
            printf("%s\n", logmessage);
    } else if (!is_stopping) {
        fprintf(stderr, "Log file is not open; skipping message.\n");
    }
    pthread_mutex_unlock(&log_mutex);
}

void flushLog() {
        pthread_mutex_lock(&log_mutex);
        if (fptr != NULL) {
                fclose(fptr);
                fptr = NULL;
                //fflush(fptr);
                is_file_open = 0;
        }
        pthread_mutex_unlock(&log_mutex);
        initLogger();
}

void closeLog() {
        pthread_mutex_lock(&log_mutex);
        if (fptr != NULL) {
                fclose(fptr);
                fptr = NULL;
                is_file_open = 0;
        }
        pthread_mutex_unlock(&log_mutex);
}

void destroy_log_mutex() {
	pthread_mutex_destroy(&log_mutex);
}
