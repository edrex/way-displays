#ifndef HEAD_H
#define HEAD_H

#include "cfg.h"
#include "types.h"

bool head_name_desc_matches(struct Head *head, const char *s);

void head_desire_mode(struct Head *head);

bool head_current_is_desired(struct Head *head);

#endif // HEAD_H

