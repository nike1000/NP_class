#ifndef SOCKSSERVER_H
#define SOCKSSERVER_H

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_SOCKS_LEN 256
#define BUFFER_LEN 2048
#define SERV_TCP_PORT 8001
#define MAXCONN 20

typedef struct Connection
{
    char srcip[16];
    char srcport[6];
    char dstip[16];
    char dstport[6];
} Connection;

typedef struct Socks4Packet
{
    unsigned char vn;
    unsigned char cd;
    unsigned char dstport[2];
    unsigned char dstip[4];
    unsigned char* userid;   /* option */
    unsigned char* hostname; /* option */
} Socks4Packet;

int startSerever();
Socks4Packet handleSocksRequest(int clifd);
char* hostname_to_ip(char* hostname, char* ip);
int firewallAccessCheck(Socks4Packet pkt);
int connectTCP(char* dstip, char* dstport);
int doRedirect(int clifd, int serfd);

#endif

