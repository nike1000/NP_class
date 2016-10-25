#include "../include/netshell.h"

linenode* headnode = NULL;
linenode* curnode = NULL;
linenode* tailnode = NULL;
char* colors[] = {ANSI_COLOR_RED, ANSI_COLOR_GREEN, ANSI_COLOR_YELLOW, ANSI_COLOR_BLUE, ANSI_COLOR_MAGENTA, ANSI_COLOR_CYAN};
int clifd;

void err_dump(char *string)
{
    fprintf(stderr, "%s\n", string);
    exit(EXIT_FAILURE);
}

int main()
{
    headnode = create_node(0,"HEAD_NODE",0);
    curnode = headnode;
    tailnode = headnode;

    //setenv("PATH", "bin:.", 1);

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

/*
 * start socket, fork when accept client connection
 * */
int start_server()
{
    int sockfd, clifd;
    int fpid;
    struct sockaddr_in serv_addr, cli_addr;

    // server socket file descriptor
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if(sockfd < 0)    /* sockfd = -1 if socket() error */
    {
        err_dump("server: can't open stream socket");
    }

    memset((char *) &serv_addr, '\0', sizeof(serv_addr));    /* set serv_addr all bytes to zero*/
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
        clifd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);    /* accept client connection */

        if(clifd == -1)
        {
            err_dump("server: accept failed");
        }

        fpid = fork();    /* server get a connection, fork a child to communicate with client, and parent keep accept another client */
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

/*
 * send a colorful welcome message to client
 * */
void send_welmsg(int clifd)
{
    char buffer[LINEMAX];
    sprintf(buffer, "%s%s%s", colors[getpid() % 6], WELCOME_MSG, ANSI_COLOR_RESET);    /* different colors of welcome message for client */
    write(clifd, buffer, strlen(buffer));    /* write welcome message to client */
}

/*
 * receive line from client, store line to LineLinkedList, then parse command from line and execute command with pipe
 * */
void recv_cli_cmd(int clifd)
{
    char buffer[LINEMAX];
    int readstat;
    char *delim = "\r\n";

    int count=1;
    for(;;)
    {
        write(clifd, PROMOT, sizeof(PROMOT));    /* show promot to client */
        memset((char *)buffer, '\0', LINEMAX);
        readstat = read(clifd, buffer, LINEMAX);

        if(readstat <= 0)
        {
            continue;
        }

        char *line = strtok(buffer, delim);    /* for some program, we may receive more than one line with once client write */

        rm_fespace(line);    /* to catch |N in the end of line, we remove all space at the end */

        do
        {
            int pipetonum = get_endnum(line);    /* get the number at the end of line */

            char *linecopy = malloc(sizeof(char)*strlen(line));
            strcpy(linecopy,line);      /* copy string to store in linelinklist, if use line ,it will be change because of pointer, linecmd of each node will same as last node */
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
                curnode->pipeto = pipetonum;
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

            if(curnode->done == 0)
            {
                execute_cmdline(parse_cmd_seq(line));
                curnode->done == 1;
            }

            line = strtok(NULL, delim);
        }while(line);// strtok return NULL when , NULL pointer is false

        if(strcmp(curnode->cmdline,"exit")==0)
        {
            free_lists(headnode);
            break;
        }

    }

    shutdown(clifd, SHUT_RDWR);
    close(clifd);
    exit(0);
}

/*
 * remove whitespace at the front and end of the string
 * */
char *rm_fespace(char* line)
{
    int count;
    if (!line)  /* if line is NULL pointer (from end of strtok) */
    {
        return line;
    }

    while (isspace(*line))
    {
        ++line;    /* size of char is 1 byte, *line is char, add 1(byte) to its address means we don't need this char */
    }

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

    return line;
}

/*
 * check whether tje end of line is legal, get |N number at the end of line, then remove it from line
 * */
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
        line[count] = 0;    /* remove |N from string */
        return atoi(pipetonum);
    }
    else    /* just fileNNN, not pipe format */
    {
        return 0;
    }
}

void execute_cmdline(char ***argvs)
{
    int C, P;

    int cmd_count = 0;
    while (argvs[cmd_count])    /* count cmd in this line */
    {
        ++cmd_count;
    }

    int pipes_fd[MAX_CMD_COUNT][2];    /* prepare pipe fd ,read from [0], write to [1] */

    for (P = 0; P < cmd_count; ++P)    /* create pipes, use between cmd, the last one pipe is pipe to later line, not always used */
    {
        if (pipe(pipes_fd[P]) == -1)
        {
            fprintf(stderr, "Error: Unable to create pipe. (%d)\n", P);
            exit(EXIT_FAILURE);
        }
    }

    for (C = 0; C < cmd_count; ++C)
    {
        int fd_in;    /* this is input fd for cmd, cmd will read input from this fd */

        if(C == 0 && curnode->fd_in == -1)    /* cmd is at beginning of line, and no earlier line pipe to this line */
        {
            fd_in = STDIN_FILENO;
        }
        else if(C == 0 && curnode->fd_in != -1)    /* cmd is at beginning of line, but some earlier line pipe output to this line */
        {
            fd_in = curnode->fd_in;
        }
        else    /* cmd is not at beginning of line, just read input from previous pipe */
        {
            fd_in = pipes_fd[C - 1][0];
        }

        int fd_out;    /* this is output fd for cmd, cmd will write output to this fd */

        if(C == cmd_count-1 && curnode->pipeto == 0)    /* cmd is at the end of line, and no pipe to later line */
        {
            fd_out = clifd;
        }
        else if(C == cmd_count-1 && curnode->pipeto != 0)    /* cmd is at the end of line, but pipe to later line */
        {
            fd_out = pipes_fd[C][1];    /* output fd is the last one of pipe [1] (write end) */
            linenode* tmpnode = curnode;
            while(tmpnode->linenum != curnode->linenum + curnode->pipeto)    /* find the node of line we want to pipe to */
            {
                tmpnode = tmpnode->nextPtr;
            }

            tmpnode->fd_in = pipes_fd[C][0];    /* keep the last pipe fd [0] (read end) in later line node fd_in */
        }
        else    /* cmd is not at the end of line, just write output to next pipe */
        {
            fd_out = pipes_fd[C][1];
        }
        creat_proc(argvs[C], fd_in, fd_out, cmd_count-1, pipes_fd);
    }

    for (P = 0; P < cmd_count-1; ++P)    /* close fd that we already used and no longer need */
    {
        close(pipes_fd[P][0]);
        close(pipes_fd[P][1]);
    }

    close(pipes_fd[P][1]);    /* close write end of last pipe, but keep read end for later line use */
    if(curnode->fd_in >= 0)
    {
        close(curnode->fd_in);
    }

    for (C = 0; C < cmd_count; ++C)
    {
        int status;
        wait(&status);
    }
}

void creat_proc(char **argv, int fd_in, int fd_out, int pipes_count, int pipes_fd[][2])
{
    pid_t proc = fork();

    if (proc < 0)
    {
        fprintf(stderr, "Error: Unable to fork.\n");
        exit(EXIT_FAILURE);
    }
    else if (proc == 0)
    {
        if (fd_in != STDIN_FILENO)
        {
            dup2(fd_in, STDIN_FILENO);
        }
        if (fd_out != STDOUT_FILENO)
        {
            dup2(fd_out, STDOUT_FILENO);
        }

        int P;
        for (P = 0; P < pipes_count; ++P)
        {
            close(pipes_fd[P][0]);
            close(pipes_fd[P][1]);
        }
        close(pipes_fd[P][1]);

        if(curnode->fd_in >= 0)
        {
            close(curnode->fd_in);
        }

        if (execvp(argv[0], argv) == -1)
        {
            fprintf(stderr, "Error: Unable to load the executable %s.\n", argv[0]);
            exit(EXIT_FAILURE);
        }

        /* NEVER REACH */
        exit(EXIT_FAILURE);
    }
}

/*
 * parse line to argvs array, use to execvp
 *
 * input: line of command, ex: ls -l -a | wc -l
 * output: argvs[0] = {"ls", "-l", "-a"}
 *         argvs[1] = {"wc", "-l"}
 * */
char ***parse_cmd_seq(char *str)
{
    int i, j;

    static char *cmds[MAX_CMD_COUNT + 1];
    memset(cmds, '\0', sizeof(cmds));

    cmds[0] = rm_fespace(strtok(str, "|"));
    for (i = 1; i <= MAX_CMD_COUNT; ++i)
    {
        cmds[i] = rm_fespace(strtok(NULL, "|"));
        if (cmds[i] == NULL)
        {
            break;
        }
    }

    /*
     * cmds[0] = "ls -l -a"
     * cmds[1] = "wc -l"
     * */

    static char *argvs_array[MAX_CMD_COUNT + 1][MAX_ARG_COUNT + 1];
    static char **argvs[MAX_CMD_COUNT + 1];

    /*
     * argvs_array[0][0] = "ls"
     * argvs_array[0][1] = "-l"
     * argvs_array[0][2] = "-a"
     * argvs_array[1][0] = "wc"
     * argvs_array[1][1] = "-l"
     *
     * argvs[0]= {"ls", "-l", "-a"}
     * argvs[1]= {"wc", "-l"}
     * */

    memset(argvs_array, '\0', sizeof(argvs_array));
    memset(argvs, '\0', sizeof(argvs));

    for (i = 0; cmds[i]; ++i)
    {
        argvs[i] = argvs_array[i];

        argvs[i][0] = strtok(cmds[i], " \t\n\r");
        for (j = 1; j <= MAX_ARG_COUNT; ++j)
        {
            argvs[i][j] = strtok(NULL, " \t\n\r");
            if (argvs[i][j] == NULL)
            {
                break;
            }
        }
    }

    return argvs;
}