#ifndef FDS_H
#define FDS_H

#include <poll.h>
#include <stdbool.h>

#define PFDS_SIZE 6

extern int fd_signal;
extern int fd_ipc;
extern int fd_cfg_dir;
extern int fd_settle;

extern nfds_t npfds;
extern struct pollfd pfds[PFDS_SIZE];

extern struct pollfd *pfd_signal;
extern struct pollfd *pfd_ipc;
extern struct pollfd *pfd_wayland;
extern struct pollfd *pfd_lid;
extern struct pollfd *pfd_cfg_dir;
extern struct pollfd *pfd_settle;

void init_pfds(void);

void destroy_pfds(void);

bool cfg_file_modified(char *file_name);

#endif // FDS_H

