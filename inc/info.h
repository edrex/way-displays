#ifndef INFO_H
#define INFO_H

#include "cfg.h"
#include "list.h"
#include "log.h"

enum event {
	ARRIVED,
	DEPARTED,
	DELTA,
	NONE,
};

void print_cfg(enum LogThreshold t, struct Cfg *cfg, bool del);

void print_heads(enum LogThreshold t, enum event event, struct SList *heads);

#endif // INFO_H

