#ifndef LOG_H
#define LOG_H

#include <stdbool.h>

enum LogThreshold {
	DEBUG = 1,
	INFO,
	WARNING,
	ERROR,
};
extern enum LogThreshold LOG_THRESHOLD_DEFAULT;

struct LogCapLine {
	char *line;
	enum LogThreshold threshold;
};
extern struct SList *log_cap_lines;

void log_set_threshold(enum LogThreshold threshold);

void log_set_times(bool times);

void log_(enum LogThreshold threshold, const char *__restrict __format, ...);

void log_debug(const char *__restrict __format, ...);

void log_info(const char *__restrict __format, ...);

void log_warn(const char *__restrict __format, ...);

void log_warn_errno(const char *__restrict __format, ...);

void log_error(const char *__restrict __format, ...);

void log_error_errno(const char *__restrict __format, ...);

void log_capture_start();

void log_capture_end();

void log_capture_reset();

#endif // LOG_H

