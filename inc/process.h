#ifndef PROCESS_H
#define PROCESS_H

#include <sys/types.h>

__pid_t running_pid();

void create_pid_file();

#endif // PROCESS_H

