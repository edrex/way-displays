#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <wayland-util.h>

#include "calc.h"

#include "cfg.h"
#include "list.h"
#include "types.h"

double calc_dpi(struct Mode *mode) {
	if (!mode || !mode->head || !mode->head->width_mm || !mode->head->height_mm) {
		return 0;
	}

	double dpi_horiz = (double)(mode->width) / mode->head->width_mm * 25.4;
	double dpi_vert = (double)(mode->height) / mode->head->height_mm * 25.4;
	return (dpi_horiz + dpi_vert) / 2;
}

struct Mode *optimal_mode(struct SList *modes, bool max_preferred_refresh) {
	struct Mode *mode, *optimal_mode, *preferred_mode;

	optimal_mode = NULL;
	preferred_mode = NULL;
	for (struct SList *i = modes; i; i = i->nex) {
		mode = i->val;

		if (!mode) {
			continue;
		}

		if (!optimal_mode) {
			optimal_mode = mode;
		}

		// preferred first
		if (mode->preferred) {
			optimal_mode = mode;
			preferred_mode = mode;
			break;
		}

		// highest resolution
		if (mode->width * mode->height > optimal_mode->width * optimal_mode->height) {
			optimal_mode = mode;
			continue;
		}

		// highest refresh at highest resolution
		if (mode->width == optimal_mode->width &&
				mode->height == optimal_mode->height &&
				mode->refresh_mHz > optimal_mode->refresh_mHz) {
			optimal_mode = mode;
			continue;
		}
	}

	if (preferred_mode && max_preferred_refresh) {
		optimal_mode = preferred_mode;
		for (struct SList *i = modes; i; i = i->nex) {
			mode = i->val;
			if (mode->width == optimal_mode->width && mode->height == optimal_mode->height) {
				if (mode->refresh_mHz > optimal_mode->refresh_mHz) {
					optimal_mode = mode;
				}
			}
		}
	}

	return optimal_mode;
}

wl_fixed_t auto_scale(struct Head *head) {
	if (!head || !head->desired.mode) {
		return wl_fixed_from_int(1);
	}

	// average dpi
	double dpi = calc_dpi(head->desired.mode);
	if (dpi == 0) {
		return wl_fixed_from_int(1);
	}

	// round the dpi to the nearest 12, so that we get a nice even wl_fixed_t
	long dpi_quantized = (long)(dpi / 12 + 0.5) * 12;

	// 96dpi approximately correct for older monitors and became the convention for 1:1 scaling
	return 256 * dpi_quantized / 96;
}

void calc_layout_dimensions(struct Head *head) {
	if (!head || !head->desired.mode || !head->desired.scale) {
		return;
	}

	if (head->transform % 2 == 0) {
		head->calculated.width = head->desired.mode->width;
		head->calculated.height = head->desired.mode->height;
	} else {
		head->calculated.width = head->desired.mode->height;
		head->calculated.height = head->desired.mode->width;
	}

	head->calculated.height = (int32_t)((double)head->calculated.height * 256 / head->desired.scale + 0.5);
	head->calculated.width = (int32_t)((double)head->calculated.width * 256 / head->desired.scale + 0.5);
}

struct SList *order_heads(struct SList *order_name_desc, struct SList *heads) {
	struct SList *heads_ordered = NULL;
	struct Head *head;
	struct SList *i, *j, *r;

	struct SList *sorting = slist_shallow_clone(heads);

	// specified order first
	for (i = order_name_desc; i; i = i->nex) {
		j = sorting;
		while(j) {
			head = j->val;
			r = j;
			j = j->nex;
			if (!head) {
				continue;
			}
			if (i->val &&
					((head->name && strcmp(i->val, head->name) == 0) ||
					 (head->description && strcasestr(head->description, i->val)))) {
				slist_append(&heads_ordered, head);
				slist_remove(&sorting, &r);
			}
		}
	}

	// remaing in discovered order
	for (i = sorting; i; i = i->nex) {
		head = i->val;
		if (!head) {
			continue;
		}

		slist_append(&heads_ordered, head);
	}

	slist_free(&sorting);

	return heads_ordered;
}

void position_heads(struct SList *heads, struct Cfg *cfg) {
	struct Head *head;
	int32_t tallest = 0, widest = 0, x = 0, y = 0;

	// find tallest/widest
	for (struct SList *i = heads; i; i = i->nex) {
		head = i->val;
		if (!head || !head->desired.mode || !head->desired.enabled) {
			continue;
		}
		if (head->calculated.height > tallest) {
			tallest = head->calculated.height;
		}
		if (head->calculated.width > widest) {
			widest = head->calculated.width;
		}
	}

	// arrange each in the predefined order
	for (struct SList *i = heads; i; i = i->nex) {
		head = i->val;
		if (!head || !head->desired.mode || !head->desired.enabled) {
			continue;
		}

		switch (cfg->arrange) {
			case COL:
				// position
				head->desired.y = y;
				y += head->calculated.height;

				// align
				switch (cfg->align) {
					case RIGHT:
						head->desired.x = widest - head->calculated.width;
						break;
					case MIDDLE:
						head->desired.x = (widest - head->calculated.width + 0.5) / 2;
						break;
					case LEFT:
					default:
						head->desired.x = 0;
						break;
				}
				break;
			case ROW:
			default:
				// position
				head->desired.x = x;
				x += head->calculated.width;

				// align
				switch (cfg->align) {
					case BOTTOM:
						head->desired.y = tallest - head->calculated.height;
						break;
					case MIDDLE:
						head->desired.y = (tallest - head->calculated.height + 0.5) / 2;
						break;
					case TOP:
					default:
						head->desired.y = 0;
						break;
				}
				break;
		}
	}
}

