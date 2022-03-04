#ifndef MODE_H
#define MODE_H

#include "list.h"
#include "types.h"

int32_t mhz_to_hz(int32_t mhz);

double mode_dpi(struct Mode *mode);

struct SList *modes_res_refresh(struct SList *modes);

struct Mode *mode_optimal(struct SList *modes, bool max_preferred_refresh);

#endif // MODE_H

