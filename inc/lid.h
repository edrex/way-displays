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

struct Lid *lid_create(void);

void lid_update(void);

bool lid_is_closed(char *name);

void destroy_lid(void);

#endif // LID_H

