#ifndef CALC_H
#define CALC_H

#include <stdbool.h>
#include <wayland-util.h>

#include "cfg.h"
#include "list.h"
#include "types.h"

wl_fixed_t calc_auto_scale(struct Head *head);

void calc_layout_dimensions(struct Head *head);

struct SList *calc_head_order(struct SList *order_name_desc, struct SList *heads);

void calc_head_positions(struct SList *heads, struct Cfg *cfg);

#endif // CALC_H

