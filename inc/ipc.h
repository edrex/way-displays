#ifndef IPC_H
#define IPC_H

#include <stdbool.h>

#include "types.h"

int create_fd_ipc_server();

bool process_ipc_message(int fd_sock, struct Displ *displ);

int client_stuff(int argc, const char **argv);

#endif // IPC_H

