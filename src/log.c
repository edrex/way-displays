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

enum LogThreshold LOG_THRESHOLD_DEFAULT = INFO;

struct active {
	enum LogThreshold threshold;
	bool times;
	bool capturing;
};
struct active active = {
	.threshold = INFO,
	.times = true,
	.capturing = false,
};

struct LogCap log_cap = { 0 };

char threshold_char[] = {
	'?',
	'D',
	'I',
	'W',
	'E',
};

char *threshold_prefix[] = {
	"",
	"",
	"",
	"WARNING: ",
	"ERROR: ",
};

void print_time(enum LogThreshold threshold, FILE *__restrict __stream) {
	static struct timeval tv;
	static struct tm *tm;

	gettimeofday(&tv, NULL);
	tm = localtime(&tv.tv_sec);

	fprintf(__stream, "%c [%02d:%02d:%02d.%03ld] ", threshold_char[threshold], tm->tm_hour, tm->tm_min, tm->tm_sec, tv.tv_usec / 1000);
}

void capture_line(enum LogThreshold threshold, char *l) {
	if (log_cap.num_lines < LOG_CAP_LINES) {
		struct LogCapLine *cap_line = calloc(1, sizeof(struct LogCapLine));
		cap_line->line = strdup(l);
		cap_line->threshold = threshold;
		log_cap.lines[log_cap.num_lines] = cap_line;
		log_cap.num_lines++;
	}
}

void print_line(enum LogThreshold threshold, const char *prefix, int eno, FILE *__restrict __stream, const char *__restrict __format, va_list __args) {
	static char l[LS];
	static size_t n;

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

	if (active.capturing) {
		capture_line(threshold, l);
	}

	if (threshold >= active.threshold) {
		if (active.times) {
			print_time(threshold, __stream);
		}
		fprintf(__stream, "%s\n", l);
	}
}

void print_log(enum LogThreshold threshold, int eno, FILE *__restrict __stream, const char *__restrict __format, va_list __args) {
	static const char *format;

	format = __format;
	while (*format == '\n') {
		print_line(threshold, "", 0, __stream, NULL, __args);
		format++;
	}
	print_line(threshold, threshold_prefix[threshold], eno, __stream, format, __args);
}

void log_set_threshold(enum LogThreshold threshold) {
	active.threshold = threshold;
}

void log_set_times(bool times) {
	active.times = times;
}

void log_debug(const char *__restrict __format, ...) {
	va_list args;
	va_start(args, __format);
	print_log(DEBUG, 0, stdout, __format, args);
	va_end(args);
}

void log_info(const char *__restrict __format, ...) {
	va_list args;
	va_start(args, __format);
	print_log(INFO, 0, stdout, __format, args);
	va_end(args);
}

void log_warn(const char *__restrict __format, ...) {
	va_list args;
	va_start(args, __format);
	print_log(WARNING, 0, stderr, __format, args);
	va_end(args);
}

void log_warn_errno(const char *__restrict __format, ...) {
	va_list args;
	va_start(args, __format);
	print_log(WARNING, errno, stderr, __format, args);
	va_end(args);
}

void log_error(const char *__restrict __format, ...) {
	va_list args;
	va_start(args, __format);
	print_log(ERROR, 0, stderr, __format, args);
	va_end(args);
}

void log_error_errno(const char *__restrict __format, ...) {
	va_list args;
	va_start(args, __format);
	print_log(ERROR, errno, stderr, __format, args);
	va_end(args);
}

// TODO log threshold from client
void log_capture_start() {
	active.capturing = true;
}

void log_capture_end() {
	active.capturing = false;
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
	active.capturing = false;
	log_cap.num_lines = 0;
}

