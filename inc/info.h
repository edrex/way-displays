#ifndef INFO_H
#define INFO_H

#include "list.h"
#include "types.h"

enum event {
	ARRIVED,
	DEPARTED,
	DELTA,
};

void print_cfg(struct Cfg *cfg);

void print_cfg_deltas(struct Cfg *cfg_set, struct Cfg *cfg_del);

void print_heads(enum event event, struct SList *heads);

#endif // INFO_H

