#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-client-protocol.h>
#include <wayland-util.h>

struct Mode {
	struct Head *head;

	struct zwlr_output_mode_v1 *zwlr_mode;

	int32_t width;
	int32_t height;
	int32_t refresh_mHz;
	bool preferred;
};

struct HeadState {
	struct Mode *mode;
	wl_fixed_t scale;
	int enabled;
	// layout coords
	int32_t x;
	int32_t y;
	// one shot, for use by desired
	bool set;
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
	bool lid_closed;
	bool max_preferred_refresh;

	struct HeadState current;
	struct HeadState desired;

	struct {
		int32_t width;
		int32_t height;
	} calculated;
};

enum ConfigState {
	IDLE = 0,
	SUCCEEDED,
	OUTSTANDING,
	CANCELLED,
	FAILED,
};

struct OutputManager {
	struct Displ *displ;

	struct zwlr_output_manager_v1 *zwlr_output_manager;

	struct SList *heads;

	enum ConfigState config_state;

	uint32_t serial;
	char *interface;
	struct SList *heads_arrived;
	struct SList *heads_departed;

	struct {
		struct SList *heads_ordered;
	} desired;
};

struct Displ {
	struct wl_display *display;

	struct wl_registry *registry;

	struct OutputManager *output_manager;
	struct Cfg *cfg;
	struct Lid *lid;

	uint32_t name;
};

void free_mode(void *mode);
void free_head(void *head);
void free_output_manager(void *output_manager);
void free_displ(void *displ);

void head_free_mode(struct Head *head, struct Mode *mode);

bool changes_needed_output_manager(struct OutputManager *output_manager);
bool changes_needed_head(struct Head *head);

#endif // TYPES_H

