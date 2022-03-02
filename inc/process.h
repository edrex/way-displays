#ifndef PROCESS_H
#define PROCESS_H

#include <sys/types.h>

__pid_t pid_active_server();

void pid_file_create();

void exit_fail();

#endif // PROCESS_H

