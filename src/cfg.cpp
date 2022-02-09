#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <yaml-cpp/yaml.h> // IWYU pragma: keep
#include <yaml-cpp/emitter.h>
#include <yaml-cpp/emittermanip.h>
#include <yaml-cpp/node/detail/iterator.h>
#include <yaml-cpp/node/impl.h>
#include <yaml-cpp/node/iterator.h>
#include <yaml-cpp/node/node.h>
#include <yaml-cpp/node/parse.h>
#include <exception>
#include <iosfwd>
#include <stdexcept>
#include <string>

extern "C" {
#include "cfg.h"

#include "convert.h"
#include "info.h"
#include "list.h"
#include "log.h"
#include "types.h"
}

#define CFG_FILE_NAME "cfg.yaml"

struct Cfg *cfg_default() {
	struct Cfg *cfg = (struct Cfg*)calloc(1, sizeof(struct Cfg));

	cfg->dirty = true;

	cfg->arrange = ArrangeDefault;
	cfg->align = AlignDefault;
	cfg->auto_scale = AutoScaleDefault;
	cfg->laptop_display_prefix = strdup(LaptopDisplayPrefixDefault);

	return cfg;
}

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


bool cfg_resolve_paths(struct Cfg *cfg, const char *prefix, const char *suffix) {
	if (!cfg)
		return false;

	char path[PATH_MAX];
	snprintf(path, PATH_MAX, "%s%s/way-displays/%s", prefix, suffix, CFG_FILE_NAME);
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

bool cfg_parse_node(struct Cfg *cfg, YAML::Node &node) {
	if (!cfg || !node || !node.IsMap()) {
		return false;
	}

	if (node["LOG_THRESHOLD"]) {
		const auto &level = node["LOG_THRESHOLD"].as<std::string>();
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
			log_warn("\nIgnoring invalid ARRANGE: %s, using default %s", arrange_str.c_str(), arrange_name(cfg->arrange));
		}
	}

	if (node["ALIGN"]) {
		const auto &align_str = node["ALIGN"].as<std::string>();
		enum Align align = align_val(align_str.c_str());
		if (align) {
			cfg->align = align;
		} else {
			log_warn("\nIgnoring invalid ALIGN: %s, using default %s", align_str.c_str(), align_name(cfg->align));
		}
	}

	if (node["AUTO_SCALE"]) {
		const auto &orders = node["AUTO_SCALE"];
		if (orders.as<bool>()) {
			cfg->auto_scale = ON;
		} else {
			cfg->auto_scale = OFF;
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
					user_scale->scale = display_scale["SCALE"].as<float>();
					if (user_scale->scale <= 0) {
						log_warn("\nIgnoring invalid scale for %s: %.3f", user_scale->name_desc, user_scale->scale);
						free(user_scale);
					} else {
						slist_append(&cfg->user_scales, user_scale);
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

	return true;
}

void cfg_print_messages(const char *yaml) {
	if (!yaml) {
		return;
	}

	try {
		YAML::Node node = YAML::Load(yaml);
		if (node["MESSAGES"]) {
			const auto &messages = node["MESSAGES"];
			for (const auto &message : messages) {
				log_info("%s", message.as<std::string>().c_str());
			}
		}
	} catch (const std::exception &e) {
		// this space intentionally left blank
	}
}

bool cfg_parse_active_yaml(struct Cfg *cfg, const char *yaml) {
	if (!cfg || !yaml) {
		return false;
	}

	try {
		YAML::Node node = YAML::Load(yaml);
		if (node["CFG_ACTIVE"]) {
			YAML::Node node_active = node["CFG_ACTIVE"];
			cfg_parse_node(cfg, node_active);
			return true;
		} else {
			log_error("\nactive configuration not available:\n%s", yaml);
			return false;
		}
	} catch (const std::exception &e) {
		log_error("parsing: %s\n%s", e.what(), yaml);
		return false;
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
		log_error("\nparsing %s: %s", cfg->file_path, e.what());
		return false;
	}

	return true;
}

bool test_scale_name(void *value, void *data) {
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

bool cfg_process_set(struct Cfg *cfg, struct Cfg *cfg_set) {
	if (!cfg || !cfg_set) {
		return false;
	}

	struct SList *i;

	// ARRANGE
	if (cfg_set->arrange) {
		cfg->arrange = cfg_set->arrange;
	}

	// ALIGN
	if (cfg_set->align) {
		cfg->align = cfg_set->align;
	}

	// ORDER
	for (i = cfg_set->order_name_desc; i; i = i->nex) {
		if (!slist_find(&cfg->order_name_desc, slist_test_strcmp, i->val)) {
			slist_append(&cfg->order_name_desc, strdup((char*)i->val));
		}
	}

	// AUTO_SCALE
	if (cfg_set->auto_scale) {
		cfg->auto_scale = cfg_set->auto_scale;
	}

	// SCALE
	for (i = cfg_set->user_scales; i; i = i->nex) {
		struct UserScale *from = (struct UserScale*)i->val;
		struct SList *f = slist_find(&cfg->user_scales, test_scale_name, from);
		if (f) {
			struct UserScale *existing = (struct UserScale*)f->val;
			existing->scale = from->scale;
		} else {
			struct UserScale *created = (struct UserScale*)calloc(1, sizeof(struct UserScale));
			created->name_desc = strdup(from->name_desc);
			created->scale = from->scale;
			slist_append(&cfg->user_scales, created);
		}
	}

	// LAPTOP_DISPLAY_PREFIX
	if (cfg_set->laptop_display_prefix) {
		if (cfg->laptop_display_prefix) {
			free(cfg->laptop_display_prefix);
		}
		cfg->laptop_display_prefix = strdup(cfg_set->laptop_display_prefix);
	}

	// MAX_PREFERRED_REFRESH
	for (i = cfg_set->max_preferred_refresh_name_desc; i; i = i->nex) {
		if (!slist_find(&cfg->max_preferred_refresh_name_desc, slist_test_strcmp, i->val)) {
			slist_append(&cfg->max_preferred_refresh_name_desc, strdup((char*)i->val));
		}
	}

	// DISABLED
	for (i = cfg_set->disabled_name_desc; i; i = i->nex) {
		if (!slist_find(&cfg->disabled_name_desc, slist_test_strcmp, i->val)) {
			slist_append(&cfg->disabled_name_desc, strdup((char*)i->val));
		}
	}

	return true;
}

bool cfg_process_del(struct Cfg *cfg, struct Cfg *cfg_del) {
	if (!cfg || !cfg_del) {
		return false;
	}

	struct SList *i, *j;

	// ORDER
	for (i = cfg_del->order_name_desc; i; i = i->nex) {
		while ((j = slist_find(&cfg->order_name_desc, slist_test_strcmp, i->val))) {
			free(j->val);
			slist_remove(&cfg->order_name_desc, &j);
		}
	}

	// SCALE
	for (i = cfg_del->user_scales; i; i = i->nex) {
		while ((j = slist_find(&cfg->user_scales, test_scale_name, i->val))) {
			free_user_scale((struct UserScale*)j->val);
			slist_remove(&cfg->user_scales, &j);
		}
	}

	// MAX_PREFERRED_REFRESH
	for (i = cfg_del->max_preferred_refresh_name_desc; i; i = i->nex) {
		while ((j = slist_find(&cfg->max_preferred_refresh_name_desc, slist_test_strcmp, i->val))) {
			free(j->val);
			slist_remove(&cfg->max_preferred_refresh_name_desc, &j);
		}
	}

	// DISABLED
	for (i = cfg_del->disabled_name_desc; i; i = i->nex) {
		while ((j = slist_find(&cfg->disabled_name_desc, slist_test_strcmp, i->val))) {
			free(j->val);
			slist_remove(&cfg->disabled_name_desc, &j);
		}
	}

	return true;
}

void cfg_fix(struct Cfg *cfg) {
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

struct Cfg *cfg_merge_deltas_yaml(struct Cfg *cfg, char *yaml) {
	if (!cfg || !yaml) {
		return NULL;
	}

	struct Cfg *cfg_merged = cfg_clone(cfg);
	struct Cfg *cfg_set = NULL;
	struct Cfg *cfg_del = NULL;

	try {
		YAML::Node node = YAML::Load(yaml);

		YAML::Node node_del = node["CFG_DEL"];
		if (node_del) {
			cfg_del = (struct Cfg*)calloc(1, sizeof(struct Cfg));
			cfg_parse_node(cfg_del, node_del);
			cfg_process_del(cfg_merged, cfg_del);
		}

		YAML::Node node_set = node["CFG_SET"];
		if (node_set) {
			cfg_set = (struct Cfg*)calloc(1, sizeof(struct Cfg));
			cfg_parse_node(cfg_set, node_set);
			cfg_process_set(cfg_merged, cfg_set);
		}

	} catch (const std::exception &e) {
		log_error("parsing: %s\n%s", e.what(), yaml);
		free_cfg(cfg_merged);
		cfg_merged = NULL;
		goto end;
	}

	print_cfg_deltas(cfg_set, cfg_del);

end:
	free_cfg(cfg_set);
	free_cfg(cfg_del);

	return cfg_merged;
}

void cfg_emit(YAML::Emitter &e, struct Cfg *cfg) {
	if (!cfg) {
		return;
	}

	e << YAML::BeginMap;

	if (cfg->arrange) {
		e << YAML::Key << "ARRANGE";
		e << YAML::Value << arrange_name(cfg->arrange);
	}

	if (cfg->align) {
		e << YAML::Key << "ALIGN";
		e << YAML::Value << align_name(cfg->align);
	}

	if (cfg->order_name_desc) {
		e << YAML::Key << "ORDER";
		e << YAML::BeginSeq;
		for (struct SList *i = cfg->order_name_desc; i; i = i->nex) {
			e << (char*)i->val;
		}
		e << YAML::EndSeq;
	}

	if (cfg->auto_scale) {
		e << YAML::Key << "AUTO_SCALE";
		e << YAML::Value << (cfg->auto_scale == ON);
	}

	if (cfg->user_scales) {
		e << YAML::Key << "SCALE";
		e << YAML::BeginSeq;
		for (struct SList *i = cfg->user_scales; i; i = i->nex) {
			struct UserScale *user_scale = (struct UserScale*)i->val;
			e << YAML::BeginMap;
			e << YAML::Key << "NAME_DESC";
			e << YAML::Value << user_scale->name_desc;
			e << YAML::Key << "SCALE";
			e << YAML::Value << user_scale->scale;
			e << YAML::EndMap;
		}
		e << YAML::EndSeq;
	}

	if (cfg->laptop_display_prefix) {
		e << YAML::Key << "LAPTOP_DISPLAY_PREFIX";
		e << YAML::Value << cfg->laptop_display_prefix;
	}

	if (cfg->max_preferred_refresh_name_desc) {
		e << YAML::Key << "MAX_PREFERRED_REFRESH";
		e << YAML::BeginSeq;
		for (struct SList *i = cfg->max_preferred_refresh_name_desc; i; i = i->nex) {
			e << (char*)i->val;
		}
		e << YAML::EndSeq;
	}

	if (cfg->disabled_name_desc) {
		e << YAML::Key << "DISABLED";
		e << YAML::BeginSeq;
		for (struct SList *i = cfg->disabled_name_desc; i; i = i->nex) {
			e << (char*)i->val;
		}
		e << YAML::EndSeq;
	}

	e << YAML::EndMap;
}

char *cfg_active_yaml(struct Cfg *cfg) {
	if (!cfg) {
		return NULL;
	}

	try {
		YAML::Emitter e;

		e << YAML::TrueFalseBool;
		e << YAML::UpperCase;

		e << YAML::BeginMap;

		if (log_cap.num_lines > 0) {

			e << YAML::Key << "MESSAGES";

			e << YAML::BeginSeq;

			for (int i = 0; i < log_cap.num_lines; i++) {
				struct LogCapLine *cap_line = log_cap.lines[i];
				if (cap_line && cap_line->line) {
					e << cap_line->line;
				}
			}

			e << YAML::EndSeq;
		}

		e << YAML::Key << "CFG_ACTIVE";

		cfg_emit(e, cfg);

		e << YAML::EndMap;

		if (!e.good()) {
			log_error("emitting active: %s", e.GetLastError().c_str());
			return NULL;
		}

		return strdup(e.c_str());

	} catch (const std::exception &e) {
		log_error("emitting active: %s\n%s", e.what());
		return NULL;
	}
}

char *cfg_deltas_yaml(struct Cfg *cfg_set, struct Cfg *cfg_del) {
	try {
		YAML::Emitter e;

		e << YAML::TrueFalseBool;
		e << YAML::UpperCase;

		e << YAML::BeginMap;

		if (cfg_set) {
			e << YAML::Key << "CFG_SET";
			cfg_emit(e, cfg_set);
		}

		if (cfg_del) {
			e << YAML::Key << "CFG_DEL";
			cfg_emit(e, cfg_del);
		}

		e << YAML::EndMap;

		if (!e.good()) {
			log_error("emitting deltas: %s", e.GetLastError().c_str());
			return NULL;
		}

		return strdup(e.c_str());

	} catch (const std::exception &e) {
		log_error("emitting deltas: %s\n%s", e.what());
		return NULL;
	}
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

