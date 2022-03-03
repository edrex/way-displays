#ifndef CFG_H
#define CFG_H

#ifdef __cplusplus
#include <yaml-cpp/emitter.h>
#include <yaml-cpp/node/node.h>
#endif

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" { //}
#endif

#include <stdint.h>
#include "log.h"

struct UserScale {
	char *name_desc;
	float scale;
};

enum Arrange {
	ROW = 1,
	COL,
	ARRANGE_DEFAULT = ROW,
};

enum Align {
	TOP = 1,
	MIDDLE,
	BOTTOM,
	LEFT,
	RIGHT,
	ALIGN_DEFAULT = TOP,
};

enum AutoScale {
	ON = 1,
	OFF,
	AUTO_SCALE_DEFAULT = ON,
};

struct UserMode {
	char *name_desc;
	bool max;
	int32_t width;
	int32_t height;
	int32_t refresh_hz;
};

struct Cfg {
	char *dir_path;
	char *file_path;
	char *file_name;

	bool written;

	char *laptop_display_prefix;
	struct SList *order_name_desc;
	enum Arrange arrange;
	enum Align align;
	enum AutoScale auto_scale;
	struct SList *user_scales;
	struct SList *user_modes;
	struct SList *max_preferred_refresh_name_desc;
	struct SList *disabled_name_desc;
	enum LogThreshold log_threshold;
};

extern const char *LAPTOP_DISPLAY_PREFIX_DEFAULT;

enum CfgElement {
	ARRANGE = 1,
	ALIGN,
	ORDER,
	AUTO_SCALE,
	SCALE,
	MODE,
	LAPTOP_DISPLAY_PREFIX,
	MAX_PREFERRED_REFRESH,
	LOG_THRESHOLD,
	DISABLED,
	ARRANGE_ALIGN,
};

enum CfgMergeType {
	SET = 1,
	DEL,
};

struct Cfg *cfg_merge(struct Cfg *to, struct Cfg *from, enum CfgMergeType merge_type);

struct Cfg *cfg_file_load();

struct Cfg *cfg_file_reload(struct Cfg *cfg);

void cfg_file_write(struct Cfg *cfg);

struct Cfg *cfg_default();

struct UserMode *cfg_user_mode_default();

void free_user_scale(void *user_scale);

void free_user_mode(void *user_mode);

void free_cfg(struct Cfg *cfg);

#if __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

void cfg_emit(YAML::Emitter &e, struct Cfg *cfg);

void cfg_parse_node(struct Cfg *cfg, YAML::Node &node);

#endif

#endif // CFG_H

