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
#include <time.h>
#include <unistd.h>

#define MAX_SOCKS_LEN 256
#define BUFFER_LEN 2048
#define SERV_TCP_PORT 8001
#define MAXCONN 20
#define MAX_RULE 100
#define FIREWALL_CONF "socks.conf"

typedef struct Connection
{
    char* mode;
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

typedef struct FirewallRule
{
    char mode[1];
    unsigned int srcip;
    unsigned int srcmask;
    char srcport[6];
    unsigned int dstip;
    unsigned int dstmask;
    char dstport[6];
} FirewallRule;

int startSerever();
Socks4Packet handleSocksRequest(int clifd);
char* hostname_to_ip(char* hostname, char* ip);
int firewallAccessCheck(int rulesum);
int connectTCP(char* dstip, char* dstport);
int doRedirect(int clifd, int serfd);

#endif

