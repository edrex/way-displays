#ifndef IPC_H
#define IPC_H

#include <stdbool.h>

#include "cfg.h"

#ifdef __cplusplus
extern "C" { //}
#endif

enum IpcRequestCommand {
	CFG_GET = 1,
	CFG_ADD,
	CFG_SET,
	// TODO CFG_REM
	CFG_DEL,
};

enum IpcResponseField {
	RC = 1,
	MESSAGES,
	DONE,
};

struct IpcRequest {
	enum IpcRequestCommand command;
	struct Cfg *cfg;
};

struct IpcResponse {
	bool done;
	int rc;
	int fd;
};

char *ipc_marshal_request(struct IpcRequest *request);

struct IpcRequest *ipc_unmarshal_request(char *yaml);

char *ipc_marshal_response(struct IpcResponse *response);

struct IpcResponse *ipc_unmarshal_response(char *yaml);

void free_ipc_request(struct IpcRequest *request);

void free_ipc_response(struct IpcResponse *response);

#if __cplusplus
} // extern "C"
#endif

#endif // IPC_H

