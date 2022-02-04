#ifndef CFG_H
#define CFG_H

#include "types.h"

#ifdef __cplusplus
extern "C" { //}
#endif

struct Cfg *load_cfg();

struct Cfg *reload_cfg(struct Cfg *cfg);

char *cfg_yaml_active(struct Cfg *cfg);

char *cfg_yaml_deltas(struct Cfg *cfg_set, struct Cfg *cfg_del);

struct Cfg *merge_cfg(struct Cfg *cfg, char *yaml);

#if __cplusplus
} // extern "C"
#endif

#endif // CFG_H

