#ifndef LOGGING_H
#define LOGGING_H

void flushLog();
void logError(const char* message, int severity, int mode);
void logInfo(const char* message, int severity, int mode);

#endif // LOGGING_H