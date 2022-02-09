#ifndef LOG_H
#define LOG_H

#include <stdbool.h>

enum LogLevel {
	LOG_LEVEL_DEBUG = 1,
	LOG_LEVEL_INFO,
	LOG_LEVEL_WARNING,
	LOG_LEVEL_ERROR
};
extern enum LogLevel log_level;

extern bool log_time;

struct LogCapLine {
	char *line;
	enum LogLevel log_level;
};

struct LogCap {
	struct LogCapLine **lines;
	int num_lines;
};

extern struct LogCap log_cap;

void log_debug(const char *__restrict __format, ...);

void log_info(const char *__restrict __format, ...);

void log_warn(const char *__restrict __format, ...);

void log_warn_errno(const char *__restrict __format, ...);

void log_error(const char *__restrict __format, ...);

void log_error_errno(const char *__restrict __format, ...);

void log_capture_start(enum LogLevel threshold);

void log_capture_end();

#endif // LOG_H

