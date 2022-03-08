#ifndef HEAD_H
#define HEAD_H

#include "cfg.h"
#include "types.h"

bool head_name_desc_matches(struct Head *head, const char *s);

bool head_is_max_preferred_refresh(struct Cfg *cfg, struct Head *head);

bool head_matches_user_mode(const void *user_mode, const void *head);

void head_desire_mode(struct Head *head, struct Cfg *cfg);

#endif // HEAD_H

