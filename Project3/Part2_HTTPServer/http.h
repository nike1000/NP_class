#ifndef HTTP_H
#define HTTP_H

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define SERV_TCP_PORT 6001
#define MAXCONN 10
#define MAX_HEADER_LEN 8192

typedef struct Header
{
    char method[8];
    char path[256];
    char query_string[512];
    char protocol[16];
} Header;

typedef struct ContentType
{
    char *extension;
    char *type;
} ContentType;

int startServer();
Header parseRequest(int clifd);
int checkResource(Header request, int clifd);
int handleRequest(Header request, int clifd);
int setHttpEnv(Header request);
char *getContentType(char *path);

#endif
