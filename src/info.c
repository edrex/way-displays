#include <stdio.h>
#include <string.h>

#include "info.h"

#include "calc.h"
#include "cfg.h"
#include "convert.h"
#include "list.h"
#include "log.h"
#include "mode.h"
#include "types.h"

void print_user_mode(enum LogThreshold t, struct UserMode *user_mode, bool del) {
	if (!user_mode)
		return;

	if (del) {
		log_(t, "     %s", user_mode->name_desc);
	} else if (user_mode->max) {
		log_(t, "     %s: MAX", user_mode->name_desc);
	} else if (user_mode->refresh_hz != -1) {
		log_(t, "     %s: %dx%d@%dHz",
				user_mode->name_desc,
				user_mode->width,
				user_mode->height,
				user_mode->refresh_hz
			);
	} else {
		log_(t, "     %s: %dx%d",
				user_mode->name_desc,
				user_mode->width,
				user_mode->height
			);
	}
}

void print_mode(enum LogThreshold t, struct Mode *mode) {
	if (!mode)
		return;

	log_(t, "    mode:     %dx%d@%dHz %d,%03d mHz  %s",
			mode->width,
			mode->height,
			mhz_to_hz(mode->refresh_mhz),
			mode->refresh_mhz / 1000,
			mode->refresh_mhz % 1000,
			mode->preferred ? "(preferred)" : ""
		);
}

void print_modes_res_refresh(enum LogThreshold t, struct SList *modes) {
	static char buf[2048];
	char *bp;

	struct SList *mrrs = modes_res_refresh(modes);

	struct ModesResRefresh *mrr = NULL;
	struct Mode *mode = NULL;

	for (struct SList *i = mrrs; i; i = i->nex) {
		mrr = i->val;

		bp = buf;
		bp += snprintf(bp, sizeof(buf) - (bp - buf), "    mode:    %5d x%5d @%4d Hz ", mrr->width, mrr->height, mrr->refresh_hz);

		for (struct SList *j = mrr->modes; j; j = j->nex) {
			mode = j->val;
			bp += snprintf(bp, sizeof(buf) - (bp - buf), "%4d,%03d mHz", mode->refresh_mhz / 1000, mode->refresh_mhz % 1000);
		}
		log_info("%s", buf);
	}

	slist_free_vals(&mrrs, free_modes_res_refresh);
}

void print_cfg(enum LogThreshold t, struct Cfg *cfg, bool del) {
	if (!cfg)
		return;

	struct UserScale *user_scale;
	struct UserMode *user_mode;
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
			if (del) {
				log_(t, "    %s", user_scale->name_desc);
			} else {
				log_(t, "    %s: %.3f", user_scale->name_desc, user_scale->scale);
			}
		}
	}

	if (cfg->user_modes) {
		log_(t, "  Mode:");
		for (i = cfg->user_modes; i; i = i->nex) {
			user_mode = (struct UserMode*)i->val;
			print_user_mode(t, user_mode, del);
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

	if (cfg->laptop_display_prefix) {
		log_info("  Laptop display prefix: %s", cfg->laptop_display_prefix);
	}
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
		if (!head->current.enabled || head->current.mode != head->desired.mode) {
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
	struct SList *i;

	for (i = heads; i; i = i->nex) {
		head = i->val;
		if (!head)
			continue;

		switch (event) {
			case ARRIVED:
			case NONE:
				log_(t, "\n%s%s:%s", head->name, event == ARRIVED ? " Arrived" : "", t == DEBUG && changes_needed_head(head) ? " PENDING" : "");
				log_(t, "  info:");
				log_(t, "    name:     '%s'", head->name);
				log_(t, "    desc:     '%s'", head->description);
				if (head->width_mm && head->height_mm) {
					log_(t, "    width:    %dmm", head->width_mm);
					log_(t, "    height:   %dmm", head->height_mm);
					if (head->preferred_mode) {
						log_(t, "    dpi:      %.2f @ %dx%d", mode_dpi(head->preferred_mode), head->preferred_mode->width, head->preferred_mode->height);
					}
				} else {
					log_(t, "    width:    (not specified)");
					log_(t, "    height:   (not specified)");
				}
				print_modes_res_refresh(INFO, head->modes);
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

