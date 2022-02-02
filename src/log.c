#include <bits/types/struct_tm.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "log.h"

#define TIME_FORMAT "[%02d:%02d:%02d.%03ld]"

// we are single threaded
enum LogLevel log_threshold = LOG_LEVEL_DEBUG;
struct timeval tv;

void log_print(const char *prefix, const char *suffix, int eno, FILE *__restrict __stream, const char *__restrict __format, va_list __args) {
	gettimeofday(&tv, NULL);
	struct tm *tm = localtime(&tv.tv_sec);

	const char *format_stripped = &__format[0];
	while (format_stripped && format_stripped[0] == '\n') {
		fprintf(__stream, "%s "TIME_FORMAT"\n", prefix, tm->tm_hour, tm->tm_min, tm->tm_sec, tv.tv_usec / 1000);
		format_stripped = &format_stripped[1];
	}

	fprintf(__stream, "%s "TIME_FORMAT" %s", prefix, tm->tm_hour, tm->tm_min, tm->tm_sec, tv.tv_usec / 1000, suffix);
	vfprintf(__stream, format_stripped, __args);
	if (eno) {
		fprintf(__stream, ": %d %s", eno, strerror(eno));
	}
	fprintf(__stream, "\n");
}

void log_debug(const char *__restrict __format, ...) {
	if (log_threshold <= LOG_LEVEL_DEBUG) {
		va_list args;
		va_start(args, __format);
		log_print("D", "", 0, stdout, __format, args);
		va_end(args);
	}
}

void log_info(const char *__restrict __format, ...) {
	if (log_threshold <= LOG_LEVEL_INFO) {
		va_list args;
		va_start(args, __format);
		log_print("I", "", 0, stdout, __format, args);
		va_end(args);
	}
}

void log_warn(const char *__restrict __format, ...) {
	if (log_threshold <= LOG_LEVEL_WARNING) {
		va_list args;
		va_start(args, __format);
		log_print("W", "WARNING: ", 0, stderr, __format, args);
		va_end(args);
	}
}

void log_warn_errno(const char *__restrict __format, ...) {
	if (log_threshold <= LOG_LEVEL_WARNING) {
		va_list args;
		va_start(args, __format);
		log_print("W", "WARNING: ", errno, stderr, __format, args);
		va_end(args);
	}
}

void log_error(const char *__restrict __format, ...) {
	if (log_threshold <= LOG_LEVEL_ERROR) {
		va_list args;
		va_start(args, __format);
		log_print("E", "ERROR: ", 0, stderr, __format, args);
		va_end(args);
	}
}

void log_error_errno(const char *__restrict __format, ...) {
	if (log_threshold <= LOG_LEVEL_ERROR) {
		va_list args;
		va_start(args, __format);
		log_print("E", "ERROR: ", errno, stderr, __format, args);
		va_end(args);
	}
}

