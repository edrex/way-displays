#ifndef HEAD_H
#define HEAD_H

#include "cfg.h"
#include "types.h"

extern struct SList *heads;
extern struct SList *heads_arrived;
extern struct SList *heads_departed;

// TODO make these go away
extern struct Head *head_changing_mode;

bool head_name_desc_matches(struct Head *head, const char *s);

void head_desire_mode(struct Head *head);

bool head_current_is_desired(struct Head *head);

void heads_destroy(void);

#endif // HEAD_H

