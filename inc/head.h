#ifndef HEAD_H
#define HEAD_H

#include "cfg.h"
#include "types.h"

bool head_name_desc_matches(struct Head *head, const char *s);

void head_desire_mode(struct Head *head, struct Cfg *cfg, bool user_delta);

#endif // HEAD_H

