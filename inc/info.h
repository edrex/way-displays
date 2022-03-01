#ifndef INFO_H
#define INFO_H

#include "cfg.h"
#include "list.h"

enum event {
	ARRIVED,
	DEPARTED,
	DELTA,
	NONE,
};

void print_cfg(enum LogThreshold t, struct Cfg *cfg);

void print_heads(enum LogThreshold t, enum event event, struct SList *heads);

#endif // INFO_H

