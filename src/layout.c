#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <wayland-util.h>

#include "layout.h"

#include "calc.h"
#include "cfg.h"
#include "info.h"
#include "list.h"
#include "listeners.h"
#include "types.h"
#include "wlr-output-management-unstable-v1.h"

wl_fixed_t scale_head(struct Head *head, struct Cfg *cfg) {
	struct UserScale *user_scale;

	for (struct SList *i = cfg->user_scales; i; i = i->nex) {
		user_scale = (struct UserScale*)i->val;
		if (user_scale->name_desc &&
				(strcasecmp(user_scale->name_desc, head->name) == 0 ||
				 strcasestr(head->description, user_scale->name_desc))) {
			return wl_fixed_from_double(user_scale->scale);
		}
	}

	if (cfg->auto_scale == ON) {
		return auto_scale(head);
	} else {
		return wl_fixed_from_int(1);
	}
}

void desire_arrange(struct Displ *displ) {
	struct Head *head;
	struct SList *i, *j;

	if (!displ || !displ->output_manager || !displ->cfg)
		return;

	long num_heads = slist_length(displ->output_manager->heads);

	// head specific
	for (i = displ->output_manager->heads; i; i = i->nex) {
		head = (struct Head*)i->val;

		head->desired.set = true;

		// ignore lid close when there is only the laptop display, for smoother sleeping
		if (num_heads == 1 && head->lid_closed) {
			head->desired.enabled = 1;
		} else {
			head->desired.enabled = !head->lid_closed;
		}

		// explicitly disabled
		for (j = displ->cfg->disabled_name_desc; j; j = j->nex) {
			if ((head->name && strcasecmp(j->val, head->name) == 0) ||
					(head->description && strcasestr(head->description, j->val))) {
				head->desired.enabled = false;
			}
		}

		if (head->desired.enabled) {
			head->desired.mode = optimal_mode(head->modes, head->max_preferred_refresh);
			head->desired.scale = scale_head(head, displ->cfg);
			calc_layout_dimensions(head);
		}
	}

	// head order, including disabled
	slist_free(&displ->output_manager->desired.heads_ordered);
	displ->output_manager->desired.heads_ordered = order_heads(displ->cfg->order_name_desc, displ->output_manager->heads);

	// head position
	position_heads(displ->output_manager->desired.heads_ordered, displ->cfg);
}

void apply_desired(struct Displ *displ) {
	struct Head *head;
	struct SList *i;
	struct zwlr_output_configuration_v1 *zwlr_config;

	if (!displ || !displ->output_manager || displ->output_manager->config_state != IDLE)
		return;

	// passed into our configuration listener
	zwlr_config = zwlr_output_manager_v1_create_configuration(displ->output_manager->zwlr_output_manager, displ->output_manager->serial);
	zwlr_output_configuration_v1_add_listener(zwlr_config, output_configuration_listener(), displ->output_manager);

	for (i = displ->output_manager->desired.heads_ordered; i; i = i->nex) {
		head = (struct Head*)i->val;

		if (head->desired.enabled) {

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

	displ->output_manager->config_state = OUTSTANDING;
}

enum ConfigState layout(struct Displ *displ) {
	if (!displ)
		return IDLE;

	log_debug("\nlayout START");
	print_heads(DEBUG, NONE, displ->output_manager->heads);

	print_heads(INFO, ARRIVED, displ->output_manager->heads_arrived);
	slist_free(&displ->output_manager->heads_arrived);

	print_heads(INFO, DEPARTED, displ->output_manager->heads_departed);
	slist_free_vals(&displ->output_manager->heads_departed, free_head);

	switch (displ->output_manager->config_state) {
		case OUTSTANDING:
			log_debug("\nlayout OUTSTANDING");
			// no action required
			break;
		case FAILED:
			// TODO implement failed, same as cancelled
			log_debug("\nlayout FAILED");
			break;
		case CANCELLED:
			log_debug("\nlayout CANCELLED");
			break;
		case SUCCEEDED:
			log_debug("\nlayout SUCCEEDED -> IDLE");
			log_info("\nChanges successful");
			displ->output_manager->config_state = IDLE;
			break;
		case IDLE:
		default:
			log_debug("\nlayout IDLE");
			desire_arrange(displ);
			if (changes_needed_output_manager(displ->output_manager)) {
				print_heads(INFO, DELTA, displ->output_manager->heads);
				apply_desired(displ);
			} else {
				log_info("\nNo changes needed");
			}
			break;
	}

	log_debug("\nlayout END");

	return displ->output_manager->config_state;
}

