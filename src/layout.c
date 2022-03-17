#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wayland-util.h>

#include "layout.h"

#include "calc.h"
#include "cfg.h"
#include "displ.h"
#include "head.h"
#include "info.h"
#include "lid.h"
#include "list.h"
#include "listeners.h"
#include "log.h"
#include "mode.h"
#include "process.h"
#include "server.h"
#include "types.h"
#include "wlr-output-management-unstable-v1.h"

wl_fixed_t scale_head(struct Head *head) {
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

void copy_current(void) {
	for (struct SList *i = heads; i; i = i->nex) {
		struct Head *head = (struct Head*)i->val;
		memcpy(&head->desired, &head->current, sizeof(struct HeadState));
	}
}

bool desire_arrange(void) {

	struct Head *head;
	struct SList *i, *j;

	slist_free(&heads_changing);
	copy_current();

	for (i = heads; i; i = i->nex) {
		head = (struct Head*)i->val;

		// ignore lid close when there is only the laptop display, for smoother sleeping
		head->desired.enabled = !lid_is_closed(head->name) || slist_length(heads) == 1;

		// explicitly disabled
		for (j = cfg->disabled_name_desc; j; j = j->nex) {
			if (head_name_desc_matches(head, j->val)) {
				head->desired.enabled = false;
			}
		}

		// find a mode
		if (head->desired.enabled) {
			head_desire_mode(head);
			if (!head->desired.mode) {
				head->desired.enabled = false;
			} else if (head->desired.mode != head->current.mode) {

				// single mode changes in their own operation
				slist_append(&heads_changing, head);
				head_changing_mode = head;
				return true;
			}
		}
	}

	// non-mode changes in one operation
	for (i = heads; i; i = i->nex) {
		head = (struct Head*)i->val;

		if (head->desired.enabled) {
			head->desired.scale = scale_head(head);
			calc_layout_dimensions(head);
		}
	}

	// head order, including disabled
	heads_changing = calc_head_order(cfg->order_name_desc, heads);

	// head position
	calc_head_positions(heads_changing);

	// scan for any needed change
	for (i = heads; i; i = i->nex) {
		if (!head_current_is_desired(i->val)) {
			return true;
		}
	}
	return false;
}

void apply_desired(void) {
	struct Head *head;
	struct SList *i;
	struct zwlr_output_configuration_v1 *zwlr_config;

	// passed into our configuration listener
	zwlr_config = zwlr_output_manager_v1_create_configuration(displ->output_manager, displ->serial);
	zwlr_output_configuration_v1_add_listener(zwlr_config, output_configuration_listener(), displ);

	for (i = heads_changing; i; i = i->nex) {
		head = (struct Head*)i->val;

		if (head->desired.enabled && head->desired.mode) {

			// Just a handle for subsequent calls; it's why we always enable instead of just on changes.
			head->zwlr_config_head = zwlr_output_configuration_v1_enable_head(zwlr_config, head->zwlr_head);

			if (head->current.mode != head->desired.mode) {
				zwlr_output_configuration_head_v1_set_mode(head->zwlr_config_head, head->desired.mode->zwlr_mode);
			}
			if (head->current.scale != head->desired.scale) {
				zwlr_output_configuration_head_v1_set_scale(head->zwlr_config_head, head->desired.scale);
			}
			if (head->current.x != head->desired.x || head->current.y != head->desired.y) {
				zwlr_output_configuration_head_v1_set_position(head->zwlr_config_head, head->desired.x, head->desired.y);
			}

		} else {
			zwlr_output_configuration_v1_disable_head(zwlr_config, head->zwlr_head);
		}
	}

	zwlr_output_configuration_v1_apply(zwlr_config);

	displ->config_state = OUTSTANDING;
}

void handle_failure(void) {

	if (head_changing_mode) {

		// mode setting failure, try again
		log_error("  %s:", head_changing_mode->name);
		print_mode(ERROR, head_changing_mode->desired.mode);
		slist_append(&head_changing_mode->modes_failed, head_changing_mode->desired.mode);

		// current mode may be misreported
		head_changing_mode->current.mode = NULL;

		head_changing_mode = NULL;
	} else {

		// any other failures are fatal
		exit_fail();
	}
}

void layout(void) {

	print_heads(INFO, ARRIVED, heads_arrived);
	slist_free(&heads_arrived);

	print_heads(INFO, DEPARTED, heads_departed);
	slist_free_vals(&heads_departed, free_head);

	switch (displ->config_state) {
		case SUCCEEDED:
			log_info("\nChanges successful");
			displ->config_state = IDLE;
			break;

		case OUTSTANDING:
			// wait
			return;

		case FAILED:
			log_error("\nChanges failed");
			handle_failure();
			displ->config_state = IDLE;
			break;

		case CANCELLED:
			log_info("\nChanges cancelled");
			// TODO retrying business
			displ->config_state = IDLE;
			break;

		case IDLE:
		default:
			break;
	}

	// TODO we can hard fail here if HDMI-A-1 departs after a mode set

	// TODO infinite loop when started: lid closed, eDP-1 enabled

	if (desire_arrange()) {
		print_heads(INFO, DELTA, heads_changing);
		apply_desired();
	}
}

