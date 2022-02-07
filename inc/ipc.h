#ifndef IPC_H
#define IPC_H

#include <stdbool.h>

#include "types.h"

int create_fd_ipc_server();

int create_fd_ipc_client();

ssize_t write_to_socket(int fd, char *data, size_t len);

char *read_from_socket(int fd);

bool process_ipc_message(int fd_sock, struct Displ *displ);

#endif // IPC_H

