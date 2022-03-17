#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-client-protocol.h>
#include <wayland-util.h>

#include "list.h"

struct Mode {
	struct Head *head;

	struct zwlr_output_mode_v1 *zwlr_mode;

	int32_t width;
	int32_t height;
	int32_t refresh_mhz;
	bool preferred;
};

struct ModesResRefresh {
	int32_t width;
	int32_t height;
	int32_t refresh_hz;
	struct SList *modes;
};

struct HeadState {
	struct Mode *mode;
	wl_fixed_t scale;
	int enabled;
	// layout coords
	int32_t x;
	int32_t y;
};

struct Head {
	struct OutputManager *output_manager;

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

struct OutputManager {
	struct SList *heads;

	struct Head *head_changing_mode;

	struct SList *heads_arrived;
	struct SList *heads_departed;
	struct SList *heads_changing;
};

void free_mode(void *mode);
void free_head(void *head);
void free_output_manager(void *output_manager);
void displ_free(void *displ);
void free_modes_res_refresh(void *modes_res_refresh);

void head_free_mode(struct Head *head, struct Mode *mode);

#endif // TYPES_H

