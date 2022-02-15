// IWYU pragma: no_include <yaml-cpp/node/detail/iterator.h>
// IWYU pragma: no_include <yaml-cpp/node/impl.h>
// IWYU pragma: no_include <yaml-cpp/node/iterator.h>
// IWYU pragma: no_include <yaml-cpp/node/node.h>
// IWYU pragma: no_include <yaml-cpp/node/parse.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <yaml-cpp/yaml.h> // IWYU pragma: keep
#include <exception>
#include <string>

extern "C" {
#include "cfg.h"

#include "convert.h"
#include "info.h"
#include "list.h"
#include "log.h"
}

enum Arrange ARRANGE_DEFAULT = ROW;
enum Align ALIGN_DEFAULT = TOP;
enum AutoScale AUTO_SCALE_DEFAULT = ON;
const char *LAPTOP_DISPLAY_PREFIX_DEFAULT = "eDP";

struct Cfg *cfg_clone(struct Cfg *from) {
	if (!from) {
		return NULL;
	}

	struct SList *i;
	struct Cfg *to = (struct Cfg*)calloc(1, sizeof(struct Cfg));

	// ARRANGE
	if (from->arrange) {
		to->arrange = from->arrange;
	}

	// ALIGN
	if (from->align) {
		to->align = from->align;
	}

	// ORDER
	for (i = from->order_name_desc; i; i = i->nex) {
		slist_append(&to->order_name_desc, strdup((char*)i->val));
	}

	// AUTO_SCALE
	if (from->auto_scale) {
		to->auto_scale = from->auto_scale;
	}

	// SCALE
	for (i = from->user_scales; i; i = i->nex) {
		struct UserScale *from_scale = (struct UserScale*)i->val;
		struct UserScale *to_scale = (struct UserScale*)calloc(1, sizeof(struct UserScale));
		to_scale->name_desc = strdup(from_scale->name_desc);
		to_scale->scale = from_scale->scale;
		slist_append(&to->user_scales, to_scale);
	}

	// LAPTOP_DISPLAY_PREFIX
	if (from->laptop_display_prefix) {
		to->laptop_display_prefix = strdup(from->laptop_display_prefix);
	}

	// MAX_PREFERRED_REFRESH
	for (i = from->max_preferred_refresh_name_desc; i; i = i->nex) {
		slist_append(&to->max_preferred_refresh_name_desc, strdup((char*)i->val));
	}

	// DISABLED
	for (i = from->disabled_name_desc; i; i = i->nex) {
		slist_append(&to->disabled_name_desc, strdup((char*)i->val));
	}

	return to;
}

struct Cfg *cfg_default() {
	struct Cfg *cfg = (struct Cfg*)calloc(1, sizeof(struct Cfg));

	cfg->dirty = true;

	cfg->arrange = ARRANGE_DEFAULT;
	cfg->align = ALIGN_DEFAULT;
	cfg->auto_scale = AUTO_SCALE_DEFAULT;
	cfg->laptop_display_prefix = strdup(LAPTOP_DISPLAY_PREFIX_DEFAULT);

	return cfg;
}

bool cfg_resolve_paths(struct Cfg *cfg, const char *prefix, const char *suffix) {
	if (!cfg)
		return false;

	char path[PATH_MAX];
	snprintf(path, PATH_MAX, "%s%s/way-displays/cfg.yaml", prefix, suffix);
	if (access(path, R_OK) != 0) {
		return false;
	}

	char *file_path = realpath(path, NULL);
	if (!file_path) {
		return false;
	}
	if (access(file_path, R_OK) != 0) {
		free(file_path);
		return false;
	}

	cfg->file_path = file_path;

	strcpy(path, file_path);
	cfg->dir_path = strdup(dirname(path));

	strcpy(path, file_path);
	cfg->file_name = strdup(basename(path));

	return true;
}

void cfg_parse_node(struct Cfg *cfg, YAML::Node &node) {
	if (!cfg || !node || !node.IsMap()) {
		throw std::runtime_error("missing cfg");
	}

	if (node["LOG_THRESHOLD"]) {
		const auto &level = node["LOG_THRESHOLD"].as<std::string>();
		// TODO use converters
		if (level == "DEBUG") {
			log_level = LOG_LEVEL_DEBUG;
		} else if (level == "INFO") {
			log_level = LOG_LEVEL_INFO;
		} else if (level == "WARNING") {
			log_level = LOG_LEVEL_WARNING;
		} else if (level == "ERROR") {
			log_level = LOG_LEVEL_ERROR;
		} else {
			log_level = LOG_LEVEL_INFO;
			log_warn("\nIgnoring invalid LOG_THRESHOLD: %s, using default INFO", level.c_str());
		}
	}

	if (node["LAPTOP_DISPLAY_PREFIX"]) {
		free(cfg->laptop_display_prefix);
		cfg->laptop_display_prefix = strdup(node["LAPTOP_DISPLAY_PREFIX"].as<std::string>().c_str());
	}

	if (node["ORDER"]) {
		const auto &orders = node["ORDER"];
		for (const auto &order : orders) {
			slist_append(&cfg->order_name_desc, strdup(order.as<std::string>().c_str()));
		}
	}

	if (node["ARRANGE"]) {
		const auto &arrange_str = node["ARRANGE"].as<std::string>();
		enum Arrange arrange = arrange_val(arrange_str.c_str());
		if (arrange) {
			cfg->arrange = arrange;
		} else {
			cfg->arrange = ARRANGE_DEFAULT;
			log_warn("\nIgnoring invalid ARRANGE: %s, using default %s", arrange_str.c_str(), arrange_name(cfg->arrange));
		}
	}

	if (node["ALIGN"]) {
		const auto &align_str = node["ALIGN"].as<std::string>();
		enum Align align = align_val(align_str.c_str());
		if (align) {
			cfg->align = align;
		} else {
			cfg->align = ALIGN_DEFAULT;
			log_warn("\nIgnoring invalid ALIGN: %s, using default %s", align_str.c_str(), align_name(cfg->align));
		}
	}

	if (node["AUTO_SCALE"]) {
		const auto &auto_scale = node["AUTO_SCALE"];
		try {
			if (auto_scale.as<bool>()) {
				cfg->auto_scale = ON;
			} else {
				cfg->auto_scale = OFF;
			}
		} catch (YAML::BadConversion &e) {
			cfg->auto_scale = AUTO_SCALE_DEFAULT;
			log_warn("\nIgnoring invalid AUTO_SCALE: %s, using default %s", auto_scale.as<std::string>().c_str(), auto_scale_name(cfg->auto_scale));
		}
	}

	if (node["SCALE"]) {
		const auto &display_scales = node["SCALE"];
		for (const auto &display_scale : display_scales) {
			if (display_scale["NAME_DESC"] && display_scale["SCALE"]) {
				struct UserScale *user_scale = NULL;
				try {
					user_scale = (struct UserScale*)calloc(1, sizeof(struct UserScale));
					user_scale->name_desc = strdup(display_scale["NAME_DESC"].as<std::string>().c_str());
					try {
						user_scale->scale = display_scale["SCALE"].as<float>();
						if (user_scale->scale <= 0) {
							log_warn("\nA Ignoring invalid scale for %s: %.3f", user_scale->name_desc, user_scale->scale);
							free(user_scale);
						} else {
							slist_append(&cfg->user_scales, user_scale);
						}
					} catch (YAML::BadConversion &e) {
						log_warn("\nIgnoring invalid scale for %s: %s", user_scale->name_desc, display_scale["SCALE"].as<std::string>().c_str());
						free(user_scale);
					}
				} catch (...) {
					if (user_scale) {
						free_user_scale(user_scale);
					}
					throw;
				}
			}
		}
	}

	if (node["MAX_PREFERRED_REFRESH"]) {
		const auto &name_desc = node["MAX_PREFERRED_REFRESH"];
		for (const auto &name_desc : name_desc) {
			slist_append(&cfg->max_preferred_refresh_name_desc, strdup(name_desc.as<std::string>().c_str()));
		}
	}

	if (node["DISABLED"]) {
		const auto &name_desc = node["DISABLED"];
		for (const auto &name_desc : name_desc) {
			slist_append(&cfg->disabled_name_desc, strdup(name_desc.as<std::string>().c_str()));
		}
	}
}

void cfg_fix(struct Cfg *cfg) {
	if (!cfg) {
		return;
	}
	enum Align align = cfg->align;
	enum Arrange arrange = cfg->arrange;
	switch(arrange) {
		case COL:
			if (align != LEFT && align != MIDDLE && align != RIGHT) {
				log_warn("\nIgnoring invalid ALIGN: %s for %s arrange. Valid values are LEFT, MIDDLE and RIGHT. Using default LEFT.", align_name(align), arrange_name(arrange));
				cfg->align = LEFT;
			}
			break;
		case ROW:
		default:
			if (align != TOP && align != MIDDLE && align != BOTTOM) {
				log_warn("\nIgnoring invalid ALIGN: %s for %s arrange. Valid values are TOP, MIDDLE and BOTTOM. Using default TOP.", align_name(align), arrange_name(arrange));
				cfg->align = TOP;
			}
			break;
	}
}

bool cfg_parse_file(struct Cfg *cfg) {
	if (!cfg || !cfg->file_path) {
		return false;
	}

	try {
		YAML::Node node = YAML::LoadFile(cfg->file_path);
		cfg_parse_node(cfg, node);
	} catch (const std::exception &e) {
		log_error("\nparsing file %s: %s", cfg->file_path, e.what());
		return false;
	}

	return true;
}

bool slist_test_scale_name(void *value, void *data) {
	if (!value || !data) {
		return false;
	}

	struct UserScale *lhs = (struct UserScale*)value;
	struct UserScale *rhs = (struct UserScale*)data;

	if (!lhs->name_desc || !rhs->name_desc) {
		return false;
	}

	return strcmp(lhs->name_desc, rhs->name_desc) == 0;
}

struct Cfg *cfg_merge_add(struct Cfg *cfg_cur, struct Cfg *cfg_add) {
	if (!cfg_cur || !cfg_add) {
		return NULL;
	}

	struct Cfg *cfg_merged = cfg_clone(cfg_cur);

	struct SList *i, *f;

	// SCALE
	struct UserScale *add_user_scale = NULL;
	struct UserScale *merged_user_scale = NULL;
	for (i = cfg_add->user_scales; i; i = i->nex) {
		add_user_scale = (struct UserScale*)i->val;
		f = slist_find(&cfg_merged->user_scales, slist_test_scale_name, add_user_scale);
		if (f) {
			merged_user_scale = (struct UserScale*)f->val;
			merged_user_scale->scale = add_user_scale->scale;
		} else {
			merged_user_scale = (struct UserScale*)calloc(1, sizeof(struct UserScale));
			merged_user_scale->name_desc = strdup(add_user_scale->name_desc);
			merged_user_scale->scale = add_user_scale->scale;
			slist_append(&cfg_merged->user_scales, merged_user_scale);
		}
	}

	// MAX_PREFERRED_REFRESH
	for (i = cfg_add->max_preferred_refresh_name_desc; i; i = i->nex) {
		if (!slist_find(&cfg_merged->max_preferred_refresh_name_desc, slist_test_strcmp, i->val)) {
			slist_append(&cfg_merged->max_preferred_refresh_name_desc, strdup((char*)i->val));
		}
	}

	// DISABLED
	for (i = cfg_add->disabled_name_desc; i; i = i->nex) {
		if (!slist_find(&cfg_merged->disabled_name_desc, slist_test_strcmp, i->val)) {
			slist_append(&cfg_merged->disabled_name_desc, strdup((char*)i->val));
		}
	}

	return cfg_merged;
}

struct Cfg *cfg_merge_set(struct Cfg *cfg_cur, struct Cfg *cfg_set) {
	if (!cfg_cur || !cfg_set) {
		return NULL;
	}

	struct Cfg *cfg_merged = cfg_clone(cfg_cur);

	struct SList *i, *r;

	// ARRANGE
	if (cfg_set->arrange) {
		cfg_merged->arrange = cfg_set->arrange;
	}

	// ALIGN
	if (cfg_set->align) {
		cfg_merged->align = cfg_set->align;
	}

	// ORDER, replace
	if (cfg_set->order_name_desc) {
		i = cfg_merged->order_name_desc;
		while(i) {
			free(i->val);
			r = i;
			i = i->nex;
			slist_remove(&cfg_merged->order_name_desc, &r);
		}
		for (i = cfg_set->order_name_desc; i; i = i->nex) {
			slist_append(&cfg_merged->order_name_desc, strdup((char*)i->val));
		}
	}

	// AUTO_SCALE
	if (cfg_set->auto_scale) {
		cfg_merged->auto_scale = cfg_set->auto_scale;
	}

	return cfg_merged;
}

struct Cfg *cfg_merge_del(struct Cfg *cfg_cur, struct Cfg *cfg_del) {
	if (!cfg_cur || !cfg_del) {
		return NULL;
	}

	struct Cfg *cfg_merged = cfg_clone(cfg_cur);

	struct SList *i, *j;

	// SCALE
	for (i = cfg_del->user_scales; i; i = i->nex) {
		bool removed = false;
		while ((j = slist_find(&cfg_merged->user_scales, slist_test_scale_name, i->val))) {
			free_user_scale((struct UserScale*)j->val);
			slist_remove(&cfg_merged->user_scales, &j);
			removed = true;
		}
		if (!removed) {
			log_error("\nSCALE for %s not found", ((struct UserScale*)i->val)->name_desc);
			free_cfg(cfg_merged);
			return NULL;
		}
	}

	// MAX_PREFERRED_REFRESH
	for (i = cfg_del->max_preferred_refresh_name_desc; i; i = i->nex) {
		bool removed = false;
		while ((j = slist_find(&cfg_merged->max_preferred_refresh_name_desc, slist_test_strcmp, i->val))) {
			free(j->val);
			slist_remove(&cfg_merged->max_preferred_refresh_name_desc, &j);
			removed = true;
		}
		if (!removed) {
			log_error("\nMAX_PREFERRED_REFRESH for %s not found", i->val);
			free_cfg(cfg_merged);
			return NULL;
		}
	}

	// DISABLED
	for (i = cfg_del->disabled_name_desc; i; i = i->nex) {
		bool removed = false;
		while ((j = slist_find(&cfg_merged->disabled_name_desc, slist_test_strcmp, i->val))) {
			free(j->val);
			slist_remove(&cfg_merged->disabled_name_desc, &j);
			removed = true;
		}
		if (!removed) {
			log_error("\nDISABLED for %s not found", i->val);
			free_cfg(cfg_merged);
			return NULL;
		}
	}

	return cfg_merged;
}

struct Cfg *cfg_merge_request(struct Cfg *cfg, struct IpcRequest *ipc_request) {
	if (!ipc_request || !cfg) {
		return NULL;
	}

	struct Cfg *cfg_merged = NULL;

	switch (ipc_request->command) {
		case CFG_ADD:
			cfg_merged = cfg_merge_add(cfg, ipc_request->cfg);
			break;
		case CFG_SET:
			cfg_merged = cfg_merge_set(cfg, ipc_request->cfg);
			break;
		case CFG_DEL:
			cfg_merged = cfg_merge_del(cfg, ipc_request->cfg);
			break;
		case CFG_GET:
		default:
			break;
	}

	cfg_fix(cfg_merged);

	return cfg_merged;
}

struct Cfg *cfg_file_load() {
	bool found = false;

	struct Cfg *cfg = cfg_default();

	if (getenv("XDG_CONFIG_HOME"))
		found = cfg_resolve_paths(cfg, getenv("XDG_CONFIG_HOME"), "");
	if (!found && getenv("HOME"))
		found = cfg_resolve_paths(cfg, getenv("HOME"), "/.config");
	if (!found)
		found = cfg_resolve_paths(cfg, "/usr/local/etc", "");
	if (!found)
		found = cfg_resolve_paths(cfg, "/etc", "");

	if (found) {
		log_info("\nFound configuration file: %s", cfg->file_path);
		if (!cfg_parse_file(cfg)) {
			log_info("\nUsing default configuration:");
			struct Cfg *cfg_def = cfg_default();
			cfg_def->dir_path = strdup(cfg->dir_path);
			cfg_def->file_path = strdup(cfg->file_path);
			cfg_def->file_name = strdup(cfg->file_name);
			free_cfg(cfg);
			cfg = cfg_def;
		}
	} else {
		log_info("\nNo configuration file found, using defaults:");
	}
	cfg_fix(cfg);
	print_cfg(cfg);

	return cfg;
}

struct Cfg *cfg_file_reload(struct Cfg *cfg) {
	if (!cfg || !cfg->file_path)
		return cfg;

	struct Cfg *cfg_new = cfg_default();
	cfg_new->dir_path = strdup(cfg->dir_path);
	cfg_new->file_path = strdup(cfg->file_path);
	cfg_new->file_name = strdup(cfg->file_name);

	log_info("\nReloading configuration file: %s", cfg->file_path);
	if (cfg_parse_file(cfg_new)) {
		cfg_fix(cfg_new);
		print_cfg(cfg_new);
		free_cfg(cfg);
		return cfg_new;
	} else {
		log_info("\nConfiguration unchanged:");
		print_cfg(cfg);
		free_cfg(cfg_new);
		return cfg;
	}
}

void free_cfg(struct Cfg *cfg) {
	if (!cfg)
		return;

	free(cfg->dir_path);
	free(cfg->file_path);
	free(cfg->file_name);

	for (struct SList *i = cfg->order_name_desc; i; i = i->nex) {
		free(i->val);
	}
	slist_free(&cfg->order_name_desc);

	for (struct SList *i = cfg->user_scales; i; i = i->nex) {
		free_user_scale((struct UserScale*)i->val);
	}
	slist_free(&cfg->user_scales);

	for (struct SList *i = cfg->max_preferred_refresh_name_desc; i; i = i->nex) {
		free(i->val);
	}
	slist_free(&cfg->max_preferred_refresh_name_desc);

	for (struct SList *i = cfg->disabled_name_desc; i; i = i->nex) {
		free(i->val);
	}
	slist_free(&cfg->disabled_name_desc);

	if (cfg->laptop_display_prefix) {
		free(cfg->laptop_display_prefix);
	}

	free(cfg);
}

void free_user_scale(struct UserScale *user_scale) {
	if (!user_scale)
		return;

	free(user_scale->name_desc);

	free(user_scale);
}

