#ifndef NETSHELL_H
#define NETSHELL_H

#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <regex.h>
#include "li3.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define SERV_TCP_PORT 6085
#define MAXCONN 5
#define LINEMAX 15000
#define MAX_ARG_COUNT 15
#define MAX_CMD_COUNT 5


#define PROMOT "% "
#define WELCOME_MSG "****************************************\n"\
                    "** Welcome to the information server. **\n"\
                    "****************************************\n"

void err_dump(char *);
int start_server();
void send_welmsg(int);
void recv_cli_cmd(int);
void create_linenode(char *,int);
char *rm_fespace(char *);
int get_endnum(char *);
char ***parse_cmd_seq(char *);
void execute_cmdline(char ***);
void creat_proc(char **argv, int fd_in, int fd_out, int pipes_count, int pipes_fd[][2]);
#endif
