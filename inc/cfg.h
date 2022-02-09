#ifndef CFG_H
#define CFG_H

#include "types.h"

#ifdef __cplusplus
extern "C" { //}
#endif

struct Cfg *cfg_file_load();

struct Cfg *cfg_file_reload(struct Cfg *cfg);

void cfg_fix(struct Cfg *cfg);

char *cfg_active_yaml(struct Cfg *cfg);

char *cfg_deltas_yaml(struct Cfg *cfg_set, struct Cfg *cfg_del);

struct Cfg *cfg_merge_deltas_yaml(struct Cfg *cfg, char *yaml);

bool cfg_parse_file(struct Cfg *cfg);

bool cfg_parse_active_yaml(struct Cfg *cfg, const char *yaml);

#if __cplusplus
} // extern "C"
#endif

#endif // CFG_H

