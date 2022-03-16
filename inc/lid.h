#ifndef LID_H
#define LID_H

#include <stdbool.h>

#include "types.h"

struct Lid {
	bool closed;

	char *device_path;
	struct libinput *libinput_monitor;
	int libinput_fd;
};

struct Lid *lid_create();

void lid_update(struct Lid *lid);

bool lid_is_closed(struct Displ *displ, char *name);

void destroy_lid(struct Lid *lid);

void free_lid(struct Lid *lid);

#endif // LID_H

