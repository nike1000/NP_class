#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>

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
#define PROMOT "% "
#define WELCOME_MSG "****************************************\n"\
                    "** Welcome to the information server. **\n"\
                    "****************************************\n"

void err_dump(char *);
int start_server();
void send_welmsg(int);
void recv_cli_cmd(int);


void err_dump(char *string)
{
    fprintf(stderr, "%s\n", string);
    exit(EXIT_FAILURE);
}

int main()
{
    int clifd;

    /* start server socket and appcet connection,
     * after accept and fork, child process will return client file descriptor,
     *  server return -1 */
    clifd = start_server();

    if(clifd == -1)    /* Parent return */
    {
        exit(0);
    }

    send_welmsg(clifd);
    recv_cli_cmd(clifd);
    return 0;
}

int start_server()
{
    int sockfd, clifd;
    int fpid;
    struct sockaddr_in cli_addr, serv_addr;

    // server socket file descriptor
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    if(sockfd < 0)    /* sockfd = -1 if socket() error */
    {
        err_dump("server: can't open stream socket");
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));    /* set serv_addr all bytes to zero*/
    serv_addr.sin_family = AF_INET;    /* AF_INET for IPv4 protocol */
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);    /* INADDR_ANY for listening on any IP address */
    serv_addr.sin_port = htons(SERV_TCP_PORT);    /* Server port number */

    if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)    /* -1 if bind fail, 0 if success */
    {
        err_dump("server: can't bind local address");
    }

    if(listen(sockfd, MAXCONN) < 0)    /* -1 if listen fail, 0 if success */
    {
        err_dump("server: can't listen on socket");
    }

    for(;;)
    {
        int clilen = sizeof(cli_addr);
        clifd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        
        if(clifd == -1)
        {
            err_dump("server: accept failed");
        }

        fpid = fork();
        if(fpid < 0)
        {
            err_dump("server: fork error");
        }
        else if(fpid > 0)    /* Parent Process */
        {
            close(clifd);    /* close clifd in parent process, we need to wait for next connection */
        }
        else                 /* First Child Process */
        {
            int spid;    /* Second fork PID */
            if((spid=fork()) < 0)    /* we fork twice and exit first child immediate ,let init be the parent of second child to avoid zombie */
            {
                err_dump("server: fork error");
            }
            else if(spid > 0)        /* First Child Process */
            {
                exit(0);
            }
            else                     /* Second Child Process */
            {
                close(sockfd);    /* close sockfd in child process, we only need clifd to communicate with client */
                sleep(1);
                return clifd;    /* return clifd to main function to execute next function */
            }
        }

        if (waitpid(fpid, NULL, 0) != fpid)
        {
            err_dump("server: waitpid error");
        }
    }
    return -1;
}

void send_welmsg(int clifd)
{
    char buffer[LINEMAX];
    sprintf(buffer, "%s%s%s", ANSI_COLOR_MAGENTA, WELCOME_MSG, ANSI_COLOR_RESET);
    write(clifd, buffer, strlen(buffer));
}

void recv_cli_cmd(int clifd)
{
    char buffer[LINEMAX];
    int readstat;
    char *delim = "\r\n";

    for(;;)
    {
        write(clifd, PROMOT, sizeof(PROMOT));
        bzero((char *)buffer, LINEMAX);
        readstat = read(clifd, buffer, LINEMAX);

        if(readstat > 0)
        {
            char *subline = strtok(buffer, delim);

            do
            {
                printf("%s\n", subline);
                subline = strtok(NULL, delim);
            }while(subline);

        }
    }

    shutdown(clifd, SHUT_RDWR);
    close(clifd);
    exit(0);
}
