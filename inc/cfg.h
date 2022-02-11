#ifndef CFG_H
#define CFG_H

#include <stdbool.h>

#include "ipc.h"

#ifdef __cplusplus
extern "C" { //}
#endif

struct UserScale {
	char *name_desc;
	float scale;
};

enum Arrange {
	ROW = 1,
	COL,
};

enum Align {
	TOP = 1,
	MIDDLE,
	BOTTOM,
	LEFT,
	RIGHT,
};

enum AutoScale {
	ON = 1,
	OFF,
};

struct Cfg {
	char *dir_path;
	char *file_path;
	char *file_name;

	bool dirty;

	char *laptop_display_prefix;
	struct SList *order_name_desc;
	enum Arrange arrange;
	enum Align align;
	enum AutoScale auto_scale;
	struct SList *user_scales;
	struct SList *max_preferred_refresh_name_desc;
	struct SList *disabled_name_desc;
};

extern const char *laptop_display_prefix_default;

enum CfgElement {
	ARRANGE = 1,
	ALIGN,
	ORDER,
	AUTO_SCALE,
	SCALE,
	LAPTOP_DISPLAY_PREFIX,
	MAX_PREFERRED_REFRESH,
	LOG_THRESHOLD,
	DISABLED,
};

struct Cfg *cfg_clone(struct Cfg *from);

struct Cfg *cfg_file_load();

struct Cfg *cfg_file_reload(struct Cfg *cfg);

void cfg_fix(struct Cfg *cfg);

struct Cfg *cfg_merge_request(struct Cfg *cfg, struct IpcRequest *ipc_request);

void free_user_scale(struct UserScale *user_scale);

void free_cfg(struct Cfg *cfg);

#if __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

void cfg_parse_node(struct Cfg *cfg, YAML::Node &node);

#endif

#endif // CFG_H

