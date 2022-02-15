#include <bits/types/struct_tm.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "log.h"

#define LS 2048

enum LogLevel log_level = LOG_LEVEL_INFO;

bool log_time = true;

bool capturing = false;

struct LogCap log_cap = { 0 };

char log_level_char[] = {
	'?',
	'D',
	'I',
	'W',
	'E',
};

void print_time(enum LogLevel level, FILE *__restrict __stream) {
	static struct timeval tv;
	static struct tm *tm;

	gettimeofday(&tv, NULL);
	tm = localtime(&tv.tv_sec);

	fprintf(__stream, "%c [%02d:%02d:%02d.%03ld] ", log_level_char[level], tm->tm_hour, tm->tm_min, tm->tm_sec, tv.tv_usec / 1000);
}

void capture_line(enum LogLevel level, char *l) {
	if (log_cap.num_lines < LOG_CAP_LINES) {
		struct LogCapLine *cap_line = calloc(1, sizeof(struct LogCapLine));
		cap_line->line = strdup(l);
		cap_line->log_level = level;
		log_cap.lines[log_cap.num_lines] = cap_line;
		log_cap.num_lines++;
	}
}

void print_line(enum LogLevel level, const char *prefix, int eno, FILE *__restrict __stream, const char *__restrict __format, va_list __args) {
	static char l[LS];
	static size_t n;

	if (log_time) {
		print_time(level, __stream);
	}

	n = 0;
	n += snprintf(l + n, LS - n, "%s", prefix);
	if (__format) {
		n += vsnprintf(l + n, LS - n, __format, __args);
	}
	if (eno) {
		n += snprintf(l + n, LS - n, ": %d %s", eno, strerror(eno));
	}

	if (n >= LS) {
		sprintf(l + LS - 4, "...");
	}

	if (capturing) {
		capture_line(level, l);
	}

	fprintf(__stream, "%s\n", l);
}

void print_lines(enum LogLevel level, const char *prefix, int eno, FILE *__restrict __stream, const char *__restrict __format, va_list __args) {
	static const char *format;

	format = __format;
	while (*format == '\n') {
		print_line(level, "", 0, __stream, NULL, __args);
		format++;
	}
	print_line(level, prefix, eno, __stream, format, __args);
}

void log_debug(const char *__restrict __format, ...) {
	if (log_level <= LOG_LEVEL_DEBUG) {
		va_list args;
		va_start(args, __format);
		print_lines(LOG_LEVEL_DEBUG, "", 0, stdout, __format, args);
		va_end(args);
	}
}

void log_info(const char *__restrict __format, ...) {
	if (log_level <= LOG_LEVEL_INFO) {
		va_list args;
		va_start(args, __format);
		print_lines(LOG_LEVEL_INFO, "", 0, stdout, __format, args);
		va_end(args);
	}
}

void log_warn(const char *__restrict __format, ...) {
	if (log_level <= LOG_LEVEL_WARNING) {
		va_list args;
		va_start(args, __format);
		print_lines(LOG_LEVEL_WARNING, "WARNING: ", 0, stderr, __format, args);
		va_end(args);
	}
}

void log_warn_errno(const char *__restrict __format, ...) {
	if (log_level <= LOG_LEVEL_WARNING) {
		va_list args;
		va_start(args, __format);
		print_lines(LOG_LEVEL_WARNING, "WARNING: ", errno, stderr, __format, args);
		va_end(args);
	}
}

void log_error(const char *__restrict __format, ...) {
	if (log_level <= LOG_LEVEL_ERROR) {
		va_list args;
		va_start(args, __format);
		print_lines(LOG_LEVEL_ERROR, "ERROR: ", 0, stderr, __format, args);
		va_end(args);
	}
}

void log_error_errno(const char *__restrict __format, ...) {
	if (log_level <= LOG_LEVEL_ERROR) {
		va_list args;
		va_start(args, __format);
		print_lines(LOG_LEVEL_ERROR, "ERROR: ", errno, stderr, __format, args);
		va_end(args);
	}
}

// TODO log level from client
void log_capture_start() {
	capturing = true;
}

void log_capture_end() {
	capturing = false;
}

void log_capture_reset() {
	for (size_t i = 0; i < LOG_CAP_LINES; i++) {
		struct LogCapLine *cap_line = log_cap.lines[i];
		if (cap_line) {
			if (cap_line->line) {
				free(cap_line->line);
			}
			free(cap_line);
			log_cap.lines[i] = NULL;
		}
	}
	capturing = false;
	log_cap.num_lines = 0;
}

