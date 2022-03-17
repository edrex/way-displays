#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "head.h"

#include "cfg.h"
#include "info.h"
#include "list.h"
#include "log.h"
#include "mode.h"
#include "server.h"

struct SList *heads = NULL;
struct SList *heads_arrived = NULL;
struct SList *heads_departed = NULL;

bool head_is_max_preferred_refresh(struct Head *head) {
	if (!head)
		return false;

	for (struct SList *i = cfg->max_preferred_refresh_name_desc; i; i = i->nex) {
		if (head_matches_name_desc(i->val, head)) {
			return true;
		}
	}
	return false;
}

bool head_matches_user_mode(const void *user_mode, const void *head) {
	return user_mode && head && head_matches_name_desc(((struct UserMode*)user_mode)->name_desc, (struct Head*)head);
}

struct Mode *user_mode(struct Head *head, struct UserMode *user_mode) {
	if (!head || !head->name || !user_mode)
		return NULL;

	struct SList *i, *j;

	// highest mode matching the user mode
	struct SList *mrrs = modes_res_refresh(head->modes);
	for (i = mrrs; i; i = i->nex) {
		struct ModesResRefresh *mrr = i->val;
		if (mrr && mrr_satisfies_user_mode(mrr, user_mode)) {
			for (j = mrr->modes; j; j = j->nex) {
				struct Mode *mode = j->val;
				if (!slist_find_equal(head->modes_failed, NULL, mode)) {
					slist_free_vals(&mrrs, mode_res_refresh_free);
					return mode;
				}
			}
		}
	}
	slist_free_vals(&mrrs, mode_res_refresh_free);

	return NULL;
}

struct Mode *preferred_mode(struct Head *head) {
	if (!head)
		return NULL;

	struct Mode *mode = NULL;
	for (struct SList *i = head->modes; i; i = i->nex) {
		if (!i->val)
			continue;
		mode = i->val;

		if (mode->preferred && !slist_find_equal(head->modes_failed, NULL, mode)) {
			return mode;
		}
	}

	return NULL;
}

struct Mode *max_preferred_mode(struct Head *head) {
	struct Mode *preferred = preferred_mode(head);

	if (!preferred)
		return NULL;

	struct Mode *mode = NULL, *max = NULL;

	for (struct SList *i = head->modes; i; i = i->nex) {
		if (!i->val)
			continue;
		mode = i->val;

		if (slist_find_equal(head->modes_failed, NULL, mode)) {
			continue;
		}

		if (mode->width != preferred->width || mode->height != preferred->height) {
			continue;
		}

		if (!max) {
			max = mode;
		} else if (mode->refresh_mhz > max->refresh_mhz) {
			max = mode;
		}
	}

	return max;
}

struct Mode *max_mode(struct Head *head) {
	if (!head)
		return NULL;

	struct Mode *mode = NULL, *max = NULL;
	for (struct SList *i = head->modes; i; i = i->nex) {
		if (!i->val)
			continue;
		mode = i->val;

		if (slist_find_equal(head->modes_failed, NULL, mode)) {
			continue;
		}

		if (!max) {
			max = mode;
			continue;
		}

		// highest resolution
		if (mode->width * mode->height > max->width * max->height) {
			max = mode;
			continue;
		}

		// highest refresh at highest resolution
		if (mode->width == max->width &&
				mode->height == max->height &&
				mode->refresh_mhz > max->refresh_mhz) {
			max = mode;
			continue;
		}
	}

	return max;
}

bool head_matches_name_desc(const void *a, const void *b) {
	const char *name_desc = a;
	const struct Head *head = b;

	if (!name_desc || !head)
		return false;

	return (
			(head->name && strcasecmp(name_desc, head->name) == 0) ||
			(head->description && strcasestr(head->description, name_desc))
		   );
}

struct Mode *head_find_mode(struct Head *head) {
	if (!head)
		return NULL;

	static char buf[512];
	static char msg_no_user[1024];
	static char msg_no_preferred[1024];
	*msg_no_user = '\0';
	*msg_no_preferred = '\0';

	struct Mode *mode = NULL;

	// maybe a user mode
	struct UserMode *um = slist_find_equal_val(cfg->user_modes, head_matches_user_mode, head);
	if (um) {
		mode = user_mode(head, um);
		if (!mode) {
			info_user_mode_string(um, buf, sizeof(buf));
			snprintf(msg_no_user, sizeof(msg_no_user), "No matching user mode for %s: %s, falling back to preferred:", head->name, buf);
		}
	}

	// always preferred
	if (!mode) {
		if (head_is_max_preferred_refresh(head)) {
			mode = max_preferred_mode(head);
		} else {
			mode = preferred_mode(head);
		}
		if (!mode) {
			snprintf(msg_no_preferred, sizeof(msg_no_preferred), "No preferred mode for %s, falling back to maximum available:", head->name);
		}
	}

	// last change maximum
	if (!mode) {
		mode = max_mode(head);
	}

	// TODO store modes_warned and warn regardless of changes
	// warn on actual changes or user interactions that may not result in a change
	if (head->current.mode != mode) {
		bool first_line = true;
		if (*msg_no_user != '\0') {
			if (first_line) {
				log_warn("");
				first_line = false;
			}
			log_warn(msg_no_user);
		}
		if (*msg_no_preferred != '\0') {
			if (first_line) {
				log_warn("");
				first_line = false;
			}
			log_warn(msg_no_preferred);
		}
		if (!first_line) {
			print_mode(WARNING, mode);
		}
		if (!mode) {
			if (first_line) {
				log_warn("");
				first_line = false;
			}
			// TODO move to layout
			log_warn("No mode for %s, disabling.", head->name);
			print_head(WARNING, NONE, head);
		}
	}

	return mode;
}

bool head_current_not_desired(const void *data) {
	const struct Head *head = data;

	return (head &&
			(head->desired.mode != head->current.mode ||
			 head->desired.scale != head->current.scale ||
			 head->desired.enabled != head->current.enabled ||
			 head->desired.x != head->current.x ||
			 head->desired.y != head->current.y));
}

bool head_current_mode_not_desired(const void *data) {
	const struct Head *head = data;

	return (head && head->desired.mode != head->current.mode);
}

void head_free(void *data) {
	struct Head *head = data;

	if (!head)
		return;

	slist_free(&head->modes_failed);
	slist_free_vals(&head->modes, mode_free);

	free(head->name);
	free(head->description);
	free(head->make);
	free(head->model);
	free(head->serial_number);

	free(head);
}

void head_release_mode(struct Head *head, struct Mode *mode) {
	if (!head || !mode)
		return;

	if (head->desired.mode == mode) {
		head->desired.mode = NULL;
	}
	if (head->current.mode == mode) {
		head->current.mode = NULL;
	}

	slist_remove_all(&head->modes, NULL, mode);
}

void heads_release_head(struct Head *head) {
	slist_remove_all(&heads_arrived, NULL, head);
	slist_remove_all(&heads_departed, NULL, head);
	slist_remove_all(&heads, NULL, head);
}

void heads_destroy(void) {

	slist_free_vals(&heads, head_free);
	slist_free_vals(&heads_departed, head_free);

	slist_free(&heads_arrived);
}

