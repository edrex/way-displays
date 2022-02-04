#ifndef INFO_H
#define INFO_H

#include "list.h"
#include "types.h"

enum event {
	ARRIVED,
	DEPARTED,
	DELTA,
};

char *arrange_name(enum Arrange arrange);

char *align_name(enum Align align);

void print_cfg(struct Cfg *cfg);

void print_heads(enum event event, struct SList *heads);

#endif // INFO_H

