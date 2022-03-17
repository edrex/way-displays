#ifndef DISPL_H
#define DISPL_H

#include "types.h"

struct Displ {
	struct wl_display *display;

	struct wl_registry *registry;

	struct OutputManager *output_manager;

	uint32_t name;
};

void init_displ(void);

void destroy_displ(void);

#endif // DISPL_H
