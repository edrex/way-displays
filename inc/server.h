#ifndef SERVER_H
#define SERVER_H

extern struct Displ *displ;
extern struct OutputManager *output_manager;
extern struct Cfg *cfg;
extern struct Lid *lid;

int server(void);

#endif // SERVER_H

