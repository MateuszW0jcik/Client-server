#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>

#define SERVER_NAME "server"
#define SERVER_FREE "server_free"

struct query_t{
    sem_t server_start;
    sem_t server_stop;

    char name[30];
    size_t length;

    int32_t min;
    int32_t max;

    int status;
    pid_t server_pid;
};

#endif //COMMON_H
