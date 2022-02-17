#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include "log.h"

#include "calc.h"
#include "types.h"

// we are single threaded
enum LogLevel log_threshold = LOG_LEVEL_INFO;
struct timeval tv;

void log_print(enum LogLevel level, const char *prefix, const char *suffix, FILE *__restrict __stream, const char *__restrict __format, va_list __args) {
	if (log_threshold > level) {
		return;
	}

	gettimeofday(&tv, NULL);
	struct tm *tm = localtime(&tv.tv_sec);

	fprintf(__stream, "%s [%02d:%02d:%02d.%03ld] %s", prefix, tm->tm_hour, tm->tm_min, tm->tm_sec, tv.tv_usec / 1000, suffix);
	vfprintf(__stream, __format, __args);
	fprintf(__stream, "\n");
}

void log_debug(const char *__restrict __format, ...) {
	va_list args;
	va_start(args, __format);
	log_print(LOG_LEVEL_DEBUG, "D", "", stdout, __format, args);
	va_end(args);
}

void log_info(const char *__restrict __format, ...) {
	va_list args;
	va_start(args, __format);
	log_print(LOG_LEVEL_INFO, "I", "", stdout, __format, args);
	va_end(args);
}

void log_warn(const char *__restrict __format, ...) {
	va_list args;
	va_start(args, __format);
	log_print(LOG_LEVEL_WARNING, "W", "WARNING: ", stderr, __format, args);
	va_end(args);
}

void log_error(const char *__restrict __format, ...) {
	va_list args;
	va_start(args, __format);
	log_print(LOG_LEVEL_ERROR, "E", "ERROR: ", stderr, __format, args);
	va_end(args);
}

