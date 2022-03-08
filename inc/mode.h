#ifndef MODE_H
#define MODE_H

#include "cfg.h"
#include "list.h"
#include "types.h"

int32_t mhz_to_hz(int32_t mhz);

double mode_dpi(struct Mode *mode);

struct SList *modes_res_refresh(struct SList *modes);

bool mrr_satisfies_user_mode(struct ModesResRefresh *mrr, struct UserMode *user_mode);

#endif // MODE_H

