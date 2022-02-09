#ifndef IPC_H
#define IPC_H

#include "types.h"

int create_fd_ipc_server();

int create_fd_ipc_client();

int socket_accept(int fd_sock);

char *socket_read(int fd);

ssize_t socket_write(int fd, char *data, size_t len);

#endif // IPC_H

