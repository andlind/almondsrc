#ifndef ALMOND_LOGGER_H
#define ALMOND_LOGGER_H

typedef enum LogLevel {
    LOG_INFO = 0,
    LOG_WARNING = 1,
    LOG_ERROR = 2,
    LOG_DEBUG = 3
} LogLevel;

extern void initLogger();
extern void flushLog();
extern void closeLog();
extern void destroy_log_mutex();
void cleanupLogger(void);
extern void writeLog(const char *message, LogLevel level, int startup);

#endif //ALMOND_ LOGGER_H
