#ifndef CONVERT_H
#define CONVERT_H

#include "cfg.h"
#include "ipc.h"
#include "log.h"

enum Arrange arrange_val(const char *str);
const char *arrange_name(enum Arrange arrange);

enum Align align_val(const char *name);
const char *align_name(enum Align align);

enum AutoScale auto_scale_val(const char *name);
const char *auto_scale_name(enum AutoScale auto_scale);

enum CfgElement cfg_element_val(const char *name);
const char *cfg_element_name(enum CfgElement cfg_element);

enum IpcRequestCommand ipc_request_command_val(const char *name);
const char *ipc_request_command_name(enum IpcRequestCommand ipc_request_command);
const char *ipc_request_command_friendly(enum IpcRequestCommand ipc_request_command);

enum IpcResponseField ipc_response_fields_val(const char *name);
const char *ipc_response_field_name(enum IpcResponseField ipc_response_field);

enum LogLevel log_level_val(const char *name);
const char *log_level_name(enum LogLevel log_level);

#endif // CONVERT_H

