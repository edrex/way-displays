#ifndef SERVER_H
#define SERVER_H

extern struct Displ *displ;
extern struct Cfg *cfg;
extern struct Lid *lid;

int create_fd_settle(void);

int server(void);

#endif // SERVER_H

