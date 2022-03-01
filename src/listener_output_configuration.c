#include <stdbool.h>
#include <stddef.h>

#include "listeners.h"

#include "list.h"
#include "log.h"
#include "types.h"
#include "wlr-output-management-unstable-v1.h"

// OutputManager data

void cleanup(struct zwlr_output_configuration_v1 *zwlr_output_configuration_v1,
		struct OutputManager *output_manager,
		enum ConfigState config_state) {

	for (struct SList *i = output_manager->heads; i; i = i->nex) {
		struct Head *head = i->val;
		if (head->zwlr_config_head) {
			zwlr_output_configuration_head_v1_destroy(head->zwlr_config_head);
			head->zwlr_config_head = NULL;
		}
	}

	zwlr_output_configuration_v1_destroy(zwlr_output_configuration_v1);

	output_manager->config_state = SUCCEEDED;
}

static void succeeded(void *data,
		struct zwlr_output_configuration_v1 *zwlr_output_configuration_v1) {
	cleanup(zwlr_output_configuration_v1, data, SUCCEEDED);
}

static void failed(void *data,
		struct zwlr_output_configuration_v1 *zwlr_output_configuration_v1) {
	cleanup(zwlr_output_configuration_v1, data, FAILED);
}

static void cancelled(void *data,
		struct zwlr_output_configuration_v1 *zwlr_output_configuration_v1) {
	cleanup(zwlr_output_configuration_v1, data, CANCELLED);
}

static const struct zwlr_output_configuration_v1_listener listener = {
	.succeeded = succeeded,
	.failed = failed,
	.cancelled = cancelled,
};

const struct zwlr_output_configuration_v1_listener *output_configuration_listener() {
	return &listener;
}

