#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wayland-util.h>

#include "layout.h"

#include "calc.h"
#include "cfg.h"
#include "head.h"
#include "info.h"
#include "list.h"
#include "listeners.h"
#include "log.h"
#include "mode.h"
#include "process.h"
#include "types.h"
#include "wlr-output-management-unstable-v1.h"

wl_fixed_t scale_head(struct Head *head, struct Cfg *cfg) {
	struct UserScale *user_scale;

	for (struct SList *i = cfg->user_scales; i; i = i->nex) {
		user_scale = (struct UserScale*)i->val;
		if (head_name_desc_matches(head, user_scale->name_desc)) {
			return wl_fixed_from_double(user_scale->scale);
		}
	}

	if (cfg->auto_scale == ON) {
		return calc_auto_scale(head);
	} else {
		return wl_fixed_from_int(1);
	}
}

void reset(struct OutputManager *om) {
	if (!om)
		return;

	slist_free(&om->heads_changing);

	for (struct SList *i = om->heads; i; i = i->nex) {
		struct Head *head = (struct Head*)i->val;

		memcpy(&head->desired, &head->current, sizeof(struct HeadState));

		if (!head->desired.scale || head->desired.scale <= 0) {
			head->desired.scale = wl_fixed_from_double(1);
		}

		if (head->desired.x < 0 || head->desired.y < 0) {
			head->desired.x = 0;
			head->desired.y = 0;
		}
	}
}

void desire_arrange(struct Displ *displ) {
	if (!displ || !displ->output_manager || !displ->cfg)
		return;

	struct OutputManager *om = displ->output_manager;
	struct Cfg *cfg = displ->cfg;

	struct Head *head;
	struct SList *i, *j;

	reset(om);

	for (i = om->heads; i; i = i->nex) {
		head = (struct Head*)i->val;

		// ignore lid close when there is only the laptop display, for smoother sleeping
		if (slist_length(om->heads) == 1 && head->lid_closed) {
			head->desired.enabled = 1;
		} else {
			head->desired.enabled = !head->lid_closed;
		}

		// explicitly disabled
		for (j = cfg->disabled_name_desc; j; j = j->nex) {
			if (head_name_desc_matches(head, j->val)) {
				head->desired.enabled = false;
			}
		}

		// find a mode
		if (head->desired.enabled) {
			head_desire_mode(head, cfg, displ->user_delta);
			if (!head->desired.mode) {
				head->desired.enabled = false;
			} else if (head->desired.mode != head->current.mode) {

				// mode changes in their own operation
				slist_append(&om->heads_changing, head);
				om->head_changing_mode = head;
				return;
			}
		}
	}

	// non-mode changes in one operation
	for (i = om->heads; i; i = i->nex) {
		head = (struct Head*)i->val;

		if (head->desired.enabled) {
			head->desired.scale = scale_head(head, cfg);
			calc_layout_dimensions(head);
		}
	}

	// head order, including disabled
	om->heads_changing = calc_head_order(cfg->order_name_desc, om->heads);

	// head position
	calc_head_positions(om->heads_changing, cfg);
}

void apply_desired(struct OutputManager *om) {
	struct Head *head;
	struct SList *i;
	struct zwlr_output_configuration_v1 *zwlr_config;

	// passed into our configuration listener
	zwlr_config = zwlr_output_manager_v1_create_configuration(om->zwlr_output_manager, om->serial);
	zwlr_output_configuration_v1_add_listener(zwlr_config, output_configuration_listener(), om);

	for (i = om->heads_changing; i; i = i->nex) {
		head = (struct Head*)i->val;

		if (head->desired.enabled && head->desired.mode) {

			// Just a handle for subsequent calls; it's why we always enable instead of just on changes.
			head->zwlr_config_head = zwlr_output_configuration_v1_enable_head(zwlr_config, head->zwlr_head);

			// set all as disabled heads do not report these attributes
			zwlr_output_configuration_head_v1_set_mode(head->zwlr_config_head, head->desired.mode->zwlr_mode);
			zwlr_output_configuration_head_v1_set_scale(head->zwlr_config_head, head->desired.scale);
			zwlr_output_configuration_head_v1_set_position(head->zwlr_config_head, head->desired.x, head->desired.y);

		} else {
			zwlr_output_configuration_v1_disable_head(zwlr_config, head->zwlr_head);
		}
	}

	zwlr_output_configuration_v1_apply(zwlr_config);

	om->config_state = OUTSTANDING;
}

void handle_failure(struct OutputManager *om) {
	struct Head *head_changing_mode = om->head_changing_mode;

	if (head_changing_mode) {

		// mode setting failure, try again
		log_error("  %s:", head_changing_mode->name);
		print_mode(ERROR, head_changing_mode->desired.mode);
		slist_append(&head_changing_mode->modes_failed, head_changing_mode->desired.mode);

		// current mode may be misreported
		om->head_changing_mode->current.mode = NULL;

		om->head_changing_mode = NULL;
	} else {

		// any other failures are fatal
		exit_fail();
	}
}

enum ConfigState layout(struct Displ *displ) {
	if (!displ || !displ->output_manager)
		return IDLE;

	struct OutputManager *om = displ->output_manager;

	print_heads(INFO, ARRIVED, om->heads_arrived);
	slist_free(&om->heads_arrived);

	print_heads(INFO, DEPARTED, om->heads_departed);
	slist_free_vals(&om->heads_departed, free_head);

	switch (om->config_state) {
		case SUCCEEDED:
			log_info("\nChanges successful");
			om->config_state = IDLE;
			return om->config_state;

		case OUTSTANDING:
			// wait
			return om->config_state;

		case FAILED:
			log_error("\nChanges failed");
			handle_failure(om);
			om->config_state = IDLE;
			break;

		case CANCELLED:
			log_info("\nChanges cancelled");
			// TODO retrying business
			om->config_state = IDLE;
			break;

		case IDLE:
		default:
			break;
	}

	// TODO we can hard fail here if HDMI-A-1 departs after a mode set

	desire_arrange(displ);
	if (changes_needed_output_manager(om)) {
		print_heads(INFO, DELTA, om->heads);
		apply_desired(om);
	} else {
		log_info("\nNo changes needed");
	}

	return om->config_state;
}

