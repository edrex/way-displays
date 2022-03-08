#ifndef INFO_H
#define INFO_H

#include "cfg.h"
#include "list.h"
#include "log.h"
#include "types.h"

enum event {
	ARRIVED,
	DEPARTED,
	DELTA,
	NONE,
};

void print_cfg(enum LogThreshold t, struct Cfg *cfg, bool del);

void print_head(enum LogThreshold t, enum event event, struct Head *head);

void print_heads(enum LogThreshold t, enum event event, struct SList *heads);

void print_mode(enum LogThreshold t, struct Mode *mode);

#endif // INFO_H

