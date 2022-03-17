#ifndef DISPL_H
#define DISPL_H

#include "types.h"

struct Displ {
	struct wl_display *display;

	struct wl_registry *registry;

	struct OutputManager *output_manager;

	uint32_t name;
};

void displ_init(void);

void displ_destroy(void);

#endif // DISPL_H
