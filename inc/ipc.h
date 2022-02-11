#ifndef IPC_H
#define IPC_H

#include <stdbool.h>

#include "cfg.h"

#ifdef __cplusplus
extern "C" { //}
#endif

enum IpcCommand {
	CFG_GET = 1,
	CFG_ADD,
	CFG_SET,
	CFG_DEL,
};

enum IpcResponses {
	RC = 1,
	MESSAGES,
};

struct IpcRequest {
	enum IpcCommand command;
	struct Cfg *cfg;
};

char *ipc_marshal_request(struct IpcRequest *request);

struct IpcRequest *ipc_unmarshal_request(char *yaml);

char *ipc_marshal_response(int rc);

int ipc_print_response(char *yaml);

void free_ipc_request(struct IpcRequest *request);

#if __cplusplus
} // extern "C"
#endif

#endif // IPC_H

