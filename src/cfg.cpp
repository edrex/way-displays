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

#include "info.h"
#include "list.h"
#include "log.h"
#include "types.h"
}

#define CFG_FILE_NAME "cfg.yaml"

struct Cfg *default_cfg() {
	struct Cfg *cfg = (struct Cfg*)calloc(1, sizeof(struct Cfg));

	cfg->dirty = true;

	set_arrange(cfg, ArrangeDefault);
	set_align(cfg, AlignDefault);
	set_auto_scale(cfg, AutoScaleDefault);
	cfg->laptop_display_prefix = strdup(LaptopDisplayPrefixDefault);

	return cfg;
}

struct Cfg *clone_cfg(struct Cfg *from) {
	if (!from) {
		return NULL;
	}

	struct SList *i;
	struct Cfg *to = (struct Cfg*)calloc(1, sizeof(struct Cfg));

	// ARRANGE
	if (from->arrange) {
		set_arrange(to, get_arrange(from));
	}

	// ALIGN
	if (from->align) {
		set_align(to, get_align(from));
	}

	// ORDER
	for (i = from->order_name_desc; i; i = i->nex) {
		slist_append(&to->order_name_desc, strdup((char*)i->val));
	}

	// AUTO_SCALE
	if (from->auto_scale) {
		set_auto_scale(to, get_auto_scale(from));
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


bool resolve(struct Cfg *cfg, const char *prefix, const char *suffix) {
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

bool parse_cfg(struct Cfg *cfg, YAML::Node &node) {
	if (!cfg || !node || !node.IsMap()) {
		return false;
	}

	if (node["LOG_THRESHOLD"]) {
		const auto &level = node["LOG_THRESHOLD"].as<std::string>();
		if (level == "DEBUG") {
			log_threshold = LOG_LEVEL_DEBUG;
		} else if (level == "INFO") {
			log_threshold = LOG_LEVEL_INFO;
		} else if (level == "WARNING") {
			log_threshold = LOG_LEVEL_WARNING;
		} else if (level == "ERROR") {
			log_threshold = LOG_LEVEL_ERROR;
		} else {
			log_threshold = LOG_LEVEL_INFO;
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
		const auto &arrange = node["ARRANGE"].as<std::string>();
		if (arrange == "ROW") {
			set_arrange(cfg, ROW);
		} else if (arrange == "COLUMN") {
			set_arrange(cfg, COL);
		} else {
			log_warn("\nIgnoring invalid ARRANGE: %s, using default %s", arrange.c_str(), arrange_name(get_arrange(cfg)));
		}
	}

	if (node["ALIGN"]) {
		const auto &align = node["ALIGN"].as<std::string>();
		if (align == "TOP") {
			set_align(cfg, TOP);
		} else if (align == "MIDDLE") {
			set_align(cfg, MIDDLE);
		} else if (align == "BOTTOM") {
			set_align(cfg, BOTTOM);
		} else if (align == "LEFT") {
			set_align(cfg, LEFT);
		} else if (align == "RIGHT") {
			set_align(cfg, RIGHT);
		} else {
			log_warn("\nIgnoring invalid ALIGN: %s, using default %s", align.c_str(), align_name(get_align(cfg)));
		}
	}

	if (node["AUTO_SCALE"]) {
		const auto &orders = node["AUTO_SCALE"];
		set_auto_scale(cfg, orders.as<bool>());
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

bool parse_cfg_file(struct Cfg *cfg) {
	if (!cfg || !cfg->file_path) {
		return false;
	}

	try {
		YAML::Node node = YAML::LoadFile(cfg->file_path);
		parse_cfg(cfg, node);
	} catch (const std::exception &e) {
		log_error("\ncannot parse %s: %s", cfg->file_path, e.what());
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

bool set_in_cfg(struct Cfg *cfg, struct Cfg *cfg_set) {
	if (!cfg || !cfg_set) {
		return false;
	}

	struct SList *i;

	// ARRANGE
	if (cfg_set->arrange) {
		set_arrange(cfg, get_arrange(cfg_set));
	}

	// ALIGN
	if (cfg_set->align) {
		set_align(cfg, get_align(cfg_set));
	}

	// ORDER
	for (i = cfg_set->order_name_desc; i; i = i->nex) {
		if (!slist_find(&cfg->order_name_desc, slist_test_strcmp, i->val)) {
			slist_append(&cfg->order_name_desc, strdup((char*)i->val));
		}
	}

	// AUTO_SCALE
	if (cfg_set->auto_scale) {
		set_auto_scale(cfg, get_auto_scale(cfg_set));
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

bool del_from_cfg(struct Cfg *cfg, struct Cfg *cfg_del) {
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

struct Cfg *merge_cfg(struct Cfg *cfg, char *yaml) {
	if (!cfg || !yaml) {
		return NULL;
	}

	struct Cfg *merged = clone_cfg(cfg);

	try {
		YAML::Node node = YAML::Load(yaml);

		YAML::Node node_del = node["CFG_DEL"];
		if (node_del) {
			struct Cfg *cfg_del = (struct Cfg*)calloc(1, sizeof(struct Cfg));
			parse_cfg(cfg_del, node_del);
			log_debug("merge_cfg parsed del");
			del_from_cfg(merged, cfg_del);
			free_cfg(cfg_del);
		}

		YAML::Node node_set = node["CFG_SET"];
		if (node_set) {
			struct Cfg *cfg_set = (struct Cfg*)calloc(1, sizeof(struct Cfg));
			parse_cfg(cfg_set, node_set);
			log_debug("merge_cfg parsed set");
			set_in_cfg(merged, cfg_set);
			free_cfg(cfg_set);
		}

	} catch (const std::exception &e) {
		log_error("\ncannot parse YAML message: %s", e.what());
		free_cfg(merged);
		merged = NULL;
	}

	return merged;
}

void check_cfg(struct Cfg *cfg) {
	enum Align align = get_align(cfg);
	enum Arrange arrange = get_arrange(cfg);
	switch(arrange) {
		case COL:
			if (align != LEFT && align != MIDDLE && align != RIGHT) {
				log_warn("\nIgnoring invalid ALIGN: %s for %s arrange. Valid values are LEFT, MIDDLE and RIGHT. Using default LEFT.", align_name(align), arrange_name(arrange));
				set_align(cfg, LEFT);
			}
			break;
		case ROW:
		default:
			if (align != TOP && align != MIDDLE && align != BOTTOM) {
				log_warn("\nIgnoring invalid ALIGN: %s for %s arrange. Valid values are TOP, MIDDLE and BOTTOM. Using default TOP.", align_name(align), arrange_name(arrange));
				set_align(cfg, TOP);
			}
			break;
	}
}

void emit_cfg(YAML::Emitter &e, struct Cfg *cfg) {
	if (!cfg) {
		return;
	}

	e << YAML::BeginMap;

	if (cfg->arrange) {
		e << YAML::Key << "ARRANGE";
		e << YAML::Value << arrange_name(*cfg->arrange);
	}

	if (cfg->align) {
		e << YAML::Key << "ALIGN";
		e << YAML::Value << align_name(*cfg->align);
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
		e << YAML::Value << *cfg->auto_scale;
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

char *cfg_yaml_active(struct Cfg *cfg) {
	if (!cfg) {
		return NULL;
	}

	try {
		YAML::Emitter e;

		e << YAML::TrueFalseBool;
		e << YAML::UpperCase;

		e << YAML::BeginMap;

		e << YAML::Key << "CFG_ACTIVE";

		emit_cfg(e, cfg);

		e << YAML::EndMap;

		if (!e.good()) {
			log_error("cfg active yaml emit fail: %s", e.GetLastError().c_str());
			return NULL;
		}

		return strdup(e.c_str());

	} catch (const std::exception &e) {
		log_error("cfg active yaml emit fail: %s", e.what());
		return NULL;
	}
}

char *cfg_yaml_deltas(struct Cfg *cfg_set, struct Cfg *cfg_del) {
	if (!cfg_set && !cfg_del) {
		return NULL;
	}

	try {
		YAML::Emitter e;

		e << YAML::TrueFalseBool;
		e << YAML::UpperCase;

		e << YAML::BeginMap;

		if (cfg_set && cfg_set->dirty) {
			e << YAML::Key << "CFG_SET";
			emit_cfg(e, cfg_set);
		}

		if (cfg_del && cfg_del->dirty) {
			e << YAML::Key << "CFG_DEL";
			emit_cfg(e, cfg_del);
		}

		e << YAML::EndMap;

		if (!e.good()) {
			log_error("cfg delta yaml emit fail: %s", e.GetLastError().c_str());
			return NULL;
		}

		return strdup(e.c_str());

	} catch (const std::exception &e) {
		log_error("cfg delta yaml emit fail: %s", e.what());
		return NULL;
	}
}

struct Cfg *load_cfg() {
	bool found = false;

	struct Cfg *cfg = default_cfg();

	if (getenv("XDG_CONFIG_HOME"))
		found = resolve(cfg, getenv("XDG_CONFIG_HOME"), "");
	if (!found && getenv("HOME"))
		found = resolve(cfg, getenv("HOME"), "/.config");
	if (!found)
		found = resolve(cfg, "/usr/local/etc", "");
	if (!found)
		found = resolve(cfg, "/etc", "");

	if (found) {
		log_info("\nFound configuration file: %s", cfg->file_path);
		if (!parse_cfg_file(cfg)) {
			log_info("\nUsing default configuration:");
			struct Cfg *cfg_def = default_cfg();
			cfg_def->dir_path = strdup(cfg->dir_path);
			cfg_def->file_path = strdup(cfg->file_path);
			cfg_def->file_name = strdup(cfg->file_name);
			free_cfg(cfg);
			cfg = cfg_def;
		}
	} else {
		log_info("\nNo configuration file found, using defaults:");
	}
	check_cfg(cfg);
	print_cfg(cfg);

	return cfg;
}

struct Cfg *reload_cfg(struct Cfg *cfg) {
	if (!cfg || !cfg->file_path)
		return cfg;

	struct Cfg *cfg_new = default_cfg();
	cfg_new->dir_path = strdup(cfg->dir_path);
	cfg_new->file_path = strdup(cfg->file_path);
	cfg_new->file_name = strdup(cfg->file_name);

	log_info("\nReloading configuration file: %s", cfg->file_path);
	if (parse_cfg_file(cfg_new)) {
		check_cfg(cfg_new);
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

