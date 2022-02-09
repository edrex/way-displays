#include <stdio.h>
#include <string.h>
#include <wayland-util.h>

#include "info.h"

#include "calc.h"
#include "list.h"
#include "log.h"
#include "types.h"

struct NameVal {
	unsigned int val;
	char *name;
};

static struct NameVal arranges[] = {
	{ .val = ROW, .name = "ROW",    },
	{ .val = COL, .name = "COLUMN", },
	{ .val = 0,   .name = NULL,     },
};

static struct NameVal aligns[] = {
	{ .val = TOP,    .name = "TOP",    },
	{ .val = MIDDLE, .name = "MIDDLE", },
	{ .val = BOTTOM, .name = "BOTTOM", },
	{ .val = LEFT,   .name = "LEFT",   },
	{ .val = RIGHT,  .name = "RIGHT",  },
	{ .val = 0,      .name = NULL,     },
};

static struct NameVal auto_scales[] = {
	{ .val = ON,  .name = "ON",    },
	{ .val = OFF, .name = "OFF",   },
	{ .val = ON,  .name = "TRUE",  },
	{ .val = OFF, .name = "FALSE", },
	{ .val = ON,  .name = "YES",   },
	{ .val = OFF, .name = "NO",    },
	{ .val = 0,   .name = NULL,    },
};

static struct NameVal cfg_elements[] = {
	{ .val = ARRANGE,               .name = "ARRANGE",               },
	{ .val = ALIGN,                 .name = "ALIGN",                 },
	{ .val = ORDER,                 .name = "ORDER",                 },
	{ .val = AUTO_SCALE,            .name = "AUTO_SCALE",            },
	{ .val = SCALE,                 .name = "SCALE",                 },
	{ .val = LAPTOP_DISPLAY_PREFIX, .name = "LAPTOP_DISPLAY_PREFIX", },
	{ .val = MAX_PREFERRED_REFRESH, .name = "MAX_PREFERRED_REFRESH", },
	{ .val = LOG_THRESHOLD,         .name = "LOG_THRESHOLD",         },
	{ .val = DISABLED,              .name = "DISABLED",              },
	{ .val = 0,                     .name = NULL,                    },
};

unsigned int val_exact(struct NameVal *name_vals, const char *name) {
	if (!name_vals || !name) {
		return 0;
	}
	for (int i = 0; name_vals[i].name; i++) {
		if (strcasecmp(name_vals[i].name, name) == 0) {
			return name_vals[i].val;
		}
	}
	return 0;
}

unsigned int val_start(struct NameVal *name_vals, const char *name) {
	if (!name_vals || !name) {
		return 0;
	}
	for (int i = 0; name_vals[i].name; i++) {
		if (strcasestr(name_vals[i].name, name) == name_vals[i].name) {
			return name_vals[i].val;
		}
	}
	return 0;
}

char *name(struct NameVal *name_vals, unsigned int val) {
	if (!name_vals) {
		return NULL;
	}
	for (int i = 0; name_vals[i].name; i++) {
		if (val == name_vals[i].val) {
			return name_vals[i].name;
		}
	}
	return NULL;
}

enum Arrange arrange_val(const char *name) {
	if (!name || (strlen(name) < 1)) {
		return 0;
	}
	return val_start(arranges, name);
}

char *arrange_name(enum Arrange arrange) {
	return name(arranges, arrange);
}

enum Align align_val(const char *name) {
	if (!name || (strlen(name) < 1)) {
		return 0;
	}
	return val_start(aligns, name);
}

char *align_name(enum Align align) {
	return name(aligns, align);
}

enum AutoScale auto_scale_val(const char *name) {
	if (name && name[0] == 'O') {
		return val_exact(auto_scales, name);
	} else {
		return val_start(auto_scales, name);
	}
}

char *auto_scale_name(enum AutoScale auto_scale) {
	return name(auto_scales, auto_scale);
}

enum CfgElement cfg_element_val(const char *name) {
	return val_exact(cfg_elements, name);
}

char *cfg_element_name(enum CfgElement cfg_element) {
	return name(cfg_elements, cfg_element);
}

