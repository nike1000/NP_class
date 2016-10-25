#include "../include/netshell.h"

linenode* headnode = NULL;
linenode* curnode = NULL;
linenode* tailnode = NULL;
char* colors[] = {ANSI_COLOR_RED, ANSI_COLOR_GREEN, ANSI_COLOR_YELLOW, ANSI_COLOR_BLUE, ANSI_COLOR_MAGENTA, ANSI_COLOR_CYAN};

void err_dump(char *string)
{
    fprintf(stderr, "%s\n", string);
    exit(EXIT_FAILURE);
}

int main()
{
    int clifd;
    headnode = create_node(0,"HEAD_NODE",0);
    curnode = headnode;
    tailnode = headnode;

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

        if (waitpid(fpid, NULL, 0) != fpid)     /* wait for First Child Process, it will exit immediate after fork */
        {
            err_dump("server: waitpid error");
        }
    }
    return -1;
}

void send_welmsg(int clifd)
{
    char buffer[LINEMAX];
    sprintf(buffer, "%s%s%s", colors[getpid() % 6], WELCOME_MSG, ANSI_COLOR_RESET);
    write(clifd, buffer, strlen(buffer));
}

void recv_cli_cmd(int clifd)
{
    char buffer[LINEMAX];
    int readstat;
    char *delim = "\r\n";

    int count=1;
    for(;;)
    {
        write(clifd, PROMOT, sizeof(PROMOT));
        bzero((char *)buffer, LINEMAX);
        readstat = read(clifd, buffer, LINEMAX);

        if(readstat > 0)
        {
            char *line = strtok(buffer, delim);

            rm_endspace(line);    /* to catch |N in the end of line, we remove all space at the end */

            do
            {
                char *linecopy = malloc(sizeof(char)*strlen(line));
                strcpy(linecopy,line);      /* copy string to store in linelinklist, if use line ,it will be change because of pointer, linecmd of each node will same as last node */

                int pipetonum = get_endnum(line);    /* get the number at the end of line */

                if(curnode->nextPtr == NULL)    /* curnode equal to tailnode, is at the end of list, so we make new node for incoming cmdline */
                {
                    linenode* newnode = create_node(count++,linecopy,pipetonum);
                    insert_node(tailnode, newnode);
                    tailnode = tailnode->nextPtr;
                    curnode = curnode->nextPtr;
                }
                /* curnode is not equal to tailnode,
                 * means the earlier cmdline have a pipe to the cmd not coming yet and we create node without cmd at that time,
                 * so now we can set cmdline to node without create node again */
                else
                {
                    curnode = curnode->nextPtr;
                    curnode->cmdline = linecopy;
                }



                /* if the cmdline end of |N, we need to creat node for later N cmdline to pipe current node cmd to that file descriptor */
                if(pipetonum > 0)    /* end of line is |N */
                {
                    int stillneed = pipetonum-((tailnode->linenum)-(curnode->linenum));    /* some empty node may be create by earlier |N cmdline, we only need to create the number of node we still need*/

                    while(stillneed > 0)
                    {
                        linenode* newnode = create_node(count++, NULL, 0);
                        insert_node(tailnode, newnode);
                        tailnode = tailnode->nextPtr;
                        stillneed--;
                    }
                }

                print_lists(headnode);

                line = strtok(NULL, delim);
            }while(line);

        }

        if(strcmp(curnode->cmdline,"exit")==0)
        {
            free_lists(headnode);
            break;
        }

        /*execute_cmdline();*/
    }

    shutdown(clifd, SHUT_RDWR);
    close(clifd);
    exit(0);
}

void rm_endspace(char* line)
{
    int count;
    for(count = strlen(line)-1; count >= 0; count--)
    {
        if(line[count] == ' ')
        {
            line[count] = 0;
        }
        else
        {
            break;
        }
    }
}

int get_endnum(char* line)
{
    int count;
    for(count = strlen(line)-1; count >= 0; count--)   /* count will stop at the index of first non-digital char come from end */
    {
        if(!isdigit(line[count]))
        {
            break;
        }
    }

    if(count == strlen(line)-1)    /* count didn't move, means line not end by number */
    {
        if(line[count] == '|' || line[count] == '>' || line[count] == '<')    /* |>< at the end of line, error format */
        {
            return -1;
        }
        else    /* no pipe to later line */
        {
            return 0;
        }
    }
    else if(line[count] == '|')    /* match |NN.. at end of line */
    {
        char pipetonum[strlen(line)-1-count];
        strcpy(pipetonum, &line[count+1]);
        return atoi(pipetonum);
    }
    else    /* just fileNNN, not pipe format */
    {
        return 0;
    }
}

/*void execute_cmdline()*/
/*{*/
    
/*}*/
