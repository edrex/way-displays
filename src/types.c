#include <stdbool.h>
#include <stdlib.h>

#include "types.h"

#include "cfg.h"
#include "displ.h"
#include "lid.h"
#include "list.h"
#include "server.h"

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

