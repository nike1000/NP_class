#ifndef MYSHM_H
#define MYSHM_H

#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>

#define MAX_CLIENTS 30
#define SHMKEY 999
#define MAX_MSG_LEN 1024

void initshm();
void showshm();
void sigHandler(int);
int client_init(struct in_addr in, unsigned short in_port);
void yell(char*);

typedef struct CliInfo
{
    int pid;
    int uid;
    char name[20];
    char msg[MAX_MSG_LEN];
    char ip[20];
    unsigned short port;
}CliInfo;

#endif
