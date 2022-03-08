#include <stdbool.h>
#include <stdlib.h>

#include "types.h"

#include "cfg.h"
#include "lid.h"
#include "list.h"

void free_mode(void *data) {
	struct Mode *mode = data;

	if (!mode)
		return;

	free(mode);
}

void free_head(void *data) {
	struct Head *head = data;

	if (!head)
		return;

	slist_free(&head->modes_failed);
	slist_free_vals(&head->modes, free_mode);

	free(head->name);
	free(head->description);
	free(head->make);
	free(head->model);
	free(head->serial_number);

	free(head);
}

void free_output_manager(void *data) {
	struct OutputManager *output_manager = data;

	if (!output_manager)
		return;

	slist_free_vals(&output_manager->heads, free_head);
	slist_free_vals(&output_manager->heads_departed, free_head);

	slist_free(&output_manager->heads_arrived);
	slist_free(&output_manager->heads_changing);

	free(output_manager->interface);

	free(output_manager);
}

void free_displ(void *data) {
	struct Displ *displ = data;

	if (!displ)
		return;

	free_output_manager(displ->output_manager);

	free_cfg(displ->cfg);

	free_lid(displ->lid);

	free(displ);
}

void free_modes_res_refresh(void *data) {
	struct ModesResRefresh *modes_res_refresh = data;

	if (!modes_res_refresh)
		return;

	slist_free(&modes_res_refresh->modes);
	free(modes_res_refresh);
}

void head_free_mode(struct Head *head, struct Mode *mode) {
	if (!head || !mode)
		return;

	if (head->desired.mode == mode) {
		head->desired.mode = NULL;
	}
	if (head->current.mode == mode) {
		head->current.mode = NULL;
	}

	slist_remove_all(&head->modes, NULL, mode);

	free_mode(mode);
}

bool changes_needed_output_manager(struct OutputManager *output_manager) {
	struct SList *i;
	struct Head *head;

	if (!output_manager)
		return false;

	for (i = output_manager->heads; i; i = i->nex) {
		head = i->val;

		if (changes_needed_head(head)) {
			return true;
		}
	}

	return false;
}

bool changes_needed_head(struct Head *head) {
	return (head &&
			((head->desired.mode && head->desired.mode != head->current.mode) ||
			 head->desired.scale != head->current.scale ||
			 head->desired.enabled != head->current.enabled ||
			 head->desired.x != head->current.x ||
			 head->desired.y != head->current.y));
}

