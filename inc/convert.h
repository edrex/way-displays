#ifndef CONVERT_H
#define CONVERT_H

#include "list.h"
#include "types.h"

enum Arrange arrange_val(const char *str);
char *arrange_name(enum Arrange arrange);

enum Align align_val(const char *name);
char *align_name(enum Align align);

enum AutoScale auto_scale_val(const char *name);
char *auto_scale_name(enum AutoScale auto_scale);

enum CfgElement cfg_element_val(const char *name);
char *cfg_element_name(enum CfgElement cfg_element);

#endif // CONVERT_H

