#include <string.h>

#include "head.h"

#include "mode.h"

bool head_name_desc_matches(struct Head *head, const char *s) {
	if (!head || !s)
		return false;

	return (
			(head->name && strcasecmp(s, head->name) == 0) ||
			(head->description && strcasestr(head->description, s))
			);
}

bool head_is_max_preferred_refresh(struct Cfg *cfg, struct Head *head) {
	if (!cfg || !head)
		return false;

	for (struct SList *i = cfg->max_preferred_refresh_name_desc; i; i = i->nex) {
		if (head_name_desc_matches(head, i->val)) {
			return true;
		}
	}
	return false;
}

bool head_matches_user_mode(const void *user_mode, const void *head) {
	return user_mode && head && head_name_desc_matches((struct Head*)head, ((struct UserMode*)user_mode)->name_desc);
}

struct Mode *optimal_mode(struct Head *head, struct Cfg *cfg) {
	if (!head)
		return NULL;

	struct Mode *mode, *optimal, *preferred_mode;

	optimal = NULL;
	preferred_mode = NULL;
	for (struct SList *i = head->modes; i; i = i->nex) {
		mode = i->val;

		if (!mode) {
			continue;
		}

		if (!optimal) {
			optimal = mode;
		}

		// preferred first
		if (mode->preferred) {
			optimal = mode;
			preferred_mode = mode;
			break;
		}

		// highest resolution
		if (mode->width * mode->height > optimal->width * optimal->height) {
			optimal = mode;
			continue;
		}

		// highest refresh at highest resolution
		if (mode->width == optimal->width &&
				mode->height == optimal->height &&
				mode->refresh_mhz > optimal->refresh_mhz) {
			optimal = mode;
			continue;
		}
	}

	if (preferred_mode && head_is_max_preferred_refresh(cfg, head)) {
		optimal = preferred_mode;
		for (struct SList *i = head->modes; i; i = i->nex) {
			mode = i->val;
			if (mode->width == optimal->width && mode->height == optimal->height) {
				if (mode->refresh_mhz > optimal->refresh_mhz) {
					optimal = mode;
				}
			}
		}
	}

	return optimal;
}

struct Mode *user_mode(struct Head *head, struct SList *user_modes) {
	if (!head || !head->name || !user_modes)
		return NULL;

	struct SList *i, *j;
	struct UserMode *user_mode;

	// locate user mode
	if (!(user_mode = (struct UserMode*)slist_find_val(user_modes, head_matches_user_mode, head)))
		return NULL;

	// highest mode matching the user mode
	struct SList *mrrs = modes_res_refresh(head->modes);
	for (i = mrrs; i; i = i->nex) {
		struct ModesResRefresh *mrr = i->val;
		if (mrr && mrr_satisfies_user_mode(mrr, user_mode)) {
			for (j = mrr->modes; j; j = j->nex) {
				struct Mode *mode = j->val;
				if (!slist_find(head->modes_failed, NULL, mode)) {
					slist_free_vals(&mrrs, free_modes_res_refresh);
					return mode;
				}
			}
		}
	}
	slist_free_vals(&mrrs, free_modes_res_refresh);

	return NULL;
}

struct Mode *head_choose_mode(struct Head *head, struct Cfg *cfg) {
	if (!head)
		return NULL;

	struct Mode *mode = user_mode(head, cfg->user_modes);
	// TODO message about falling back; maybe separate preferred and optimal
	if (!mode) {
		mode = optimal_mode(head, cfg);
	}

	if (mode && !slist_find(head->modes_failed, NULL, mode)) {
		return mode;
	}

	return NULL;
}

