#include <string.h>
#include <wayland-util.h>

#include "info.h"

#include "calc.h"
#include "cfg.h"
#include "convert.h"
#include "list.h"
#include "log.h"
#include "types.h"

void print_cfg(enum LogThreshold t, struct Cfg *cfg) {
	if (!cfg)
		return;

	struct UserScale *user_scale;
	struct SList *i;

	if (cfg->arrange && cfg->align) {
		log_(t, "  Arrange in a %s aligned at the %s", arrange_name(cfg->arrange), align_name(cfg->align));
	} else if (cfg->arrange) {
		log_(t, "  Arrange in a %s", arrange_name(cfg->arrange));
	} else if (cfg->align) {
		log_(t, "  Align at the %s", align_name(cfg->align));
	}

	if (cfg->order_name_desc) {
		log_(t, "  Order:");
		for (i = cfg->order_name_desc; i; i = i->nex) {
			log_(t, "    %s", (char*)i->val);
		}
	}

	if (cfg->auto_scale) {
		log_(t, "  Auto scale: %s", auto_scale_name(cfg->auto_scale));
	}

	if (cfg->user_scales) {
		log_(t, "  Scale:");
		for (i = cfg->user_scales; i; i = i->nex) {
			user_scale = (struct UserScale*)i->val;
			log_(t, "    %s: %.3f", user_scale->name_desc, user_scale->scale);
		}
	}

	if (cfg->max_preferred_refresh_name_desc) {
		log_(t, "  Max preferred refresh:");
		for (i = cfg->max_preferred_refresh_name_desc; i; i = i->nex) {
			log_(t, "    %s", (char*)i->val);
		}
	}

	if (cfg->disabled_name_desc) {
		log_(t, "  Disabled:");
		for (i = cfg->disabled_name_desc; i; i = i->nex) {
			log_(t, "    %s", (char*)i->val);
		}
	}

	if (cfg->laptop_display_prefix && strcmp(cfg->laptop_display_prefix, LAPTOP_DISPLAY_PREFIX_DEFAULT) != 0) {
		log_(t, "  Laptop display prefix: %s", cfg->laptop_display_prefix);
	}
}

void print_mode(enum LogThreshold t, struct Mode *mode) {
	if (!mode)
		return;

	log_(t, "    mode:     %dx%d@%ldHz %s",
			mode->width,
			mode->height,
			(long)(((double)mode->refresh_mHz / 1000 + 0.5)),
			mode->preferred ? "(preferred)" : "           "
		  );
}

void print_head_current(enum LogThreshold t, struct Head *head) {
	if (!head)
		return;

	if (head->current.enabled) {
		log_(t, "    scale:    %.3f", wl_fixed_to_double(head->current.scale));
		log_(t, "    position: %d,%d", head->current.x, head->current.y);
		if (head->current.mode) {
			print_mode(t, head->current.mode);
		} else {
			log_(t, "    (no mode)");
		}
	} else {
		log_(t, "    (disabled)");
	}

	if (head->lid_closed) {
		log_(t, "    (lid closed)");
	}
}

void print_head_desired(enum LogThreshold t, struct Head *head) {
	if (!head)
		return;

	if (head->desired.enabled) {
		if (!head->current.enabled || head->current.scale != head->desired.scale) {
			log_(t, "    scale:    %.3f%s",
					wl_fixed_to_double(head->desired.scale),
					(!head->width_mm || !head->height_mm) ? " (default, size not specified)" : ""
				  );
		}
		if (!head->current.enabled || head->current.x != head->desired.x || head->current.y != head->desired.y) {
			log_(t, "    position: %d,%d",
					head->desired.x,
					head->desired.y
				  );
		}
		if (!head->current.enabled || (head->current.mode && head->desired.mode)) {
			print_mode(t, head->desired.mode);
		}
		if (!head->current.enabled) {
			log_(t, "    (enabled)");
		}
	} else {
		log_(t, "    (disabled)");
	}
}

void print_heads(enum LogThreshold t, enum event event, struct SList *heads) {
	struct Head *head;
	struct SList *i, *j;

	for (i = heads; i; i = i->nex) {
		head = i->val;
		if (!head)
			continue;

		switch (event) {
			case ARRIVED:
			case NONE:
				log_(t, "\n%s%s:", head->name, event == ARRIVED ? " Arrived" : "");
				log_(t, "  info:");
				log_(t, "    name:     '%s'", head->name);
				log_(t, "    desc:     '%s'", head->description);
				if (head->width_mm && head->height_mm) {
					log_(t, "    width:    %dmm", head->width_mm);
					log_(t, "    height:   %dmm", head->height_mm);
					if (head->preferred_mode) {
						log_(t, "    dpi:      %.2f @ %dx%d", calc_dpi(head->preferred_mode), head->preferred_mode->width, head->preferred_mode->height);
					}
				} else {
					log_(t, "    width:    (not specified)");
					log_(t, "    height:   (not specified)");
				}
				for (j = head->modes; j; j = j->nex) {
					print_mode(DEBUG, j->val);
				}
				log_(t, "  current:");
				print_head_current(t, head);
				break;
			case DEPARTED:
				log_(t, "\n%s Departed:", head->name);
				log_(t, "    name:     '%s'", head->name);
				log_(t, "    desc:     '%s'", head->description);
				break;
			case DELTA:
				if (changes_needed_head(head)) {
					log_(t, "\n%s Changing:", head->name);
					log_(t, "  from:");
					print_head_current(t, head);
					log_(t, "  to:");
					print_head_desired(t, head);
				}
				break;
			default:
				break;
		}
	}
}

