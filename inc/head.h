#ifndef HEAD_H
#define HEAD_H

#include <wayland-client-protocol.h>

#include "cfg.h"

extern struct SList *heads;
extern struct SList *heads_arrived;
extern struct SList *heads_departed;

struct HeadState {
	struct Mode *mode;
	wl_fixed_t scale;
	int enabled;
	// layout coords
	int32_t x;
	int32_t y;
};

struct Head {

	struct zwlr_output_head_v1 *zwlr_head;

	struct zwlr_output_configuration_head_v1 *zwlr_config_head;

	struct SList *modes;

	char *name;
	char *description;
	int32_t width_mm;
	int32_t height_mm;
	struct Mode *preferred_mode;
	enum wl_output_transform transform;
	char *make;
	char *model;
	char *serial_number;

	struct HeadState current;
	struct HeadState desired;

	struct SList *modes_failed;

	struct {
		int32_t width;
		int32_t height;
	} calculated;
};

bool head_name_desc_matches(struct Head *head, const char *s);

void head_desire_mode(struct Head *head);

bool head_current_is_desired(struct Head *head);

void head_release_mode(struct Head *head, struct Mode *mode);

void head_free(void *head);

void heads_release_head(struct Head *head);

void heads_destroy(void);

#endif // HEAD_H

