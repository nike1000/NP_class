#include "../include/netshell.h"

LineNode* headnode = NULL;
LineNode* curnode = NULL;
LineNode* tailnode = NULL;
int linecount = 1;

CliInfo clidata[MAX_CLIENTS];
int uid;
fd_set allfds;

void err_dump(char *string)
{
    fprintf(stderr, "%s\n", string);
    exit(EXIT_FAILURE);
}

int main()
{
    chdir("ras/");

    initdata();

    setenv("PATH", "bin:.", 1);

    start_server();

    return 0;
}

void initdata()
{
    int i;
    for(i = 0; i< MAX_CLIENTS; i++)
    {
        clidata[i].clifd = -1;
        clidata[i].uid = -1;
        clidata[i].linecount = -1;

        int j;
        for(j = 0; j< MAX_CLIENTS; j++)
        {
            clidata[i].fifofd[j] = -1;
        }
    }
}

/*
 * start socket, fork when accept client connection
 * */
int start_server()
{
    int sockfd, clifd;
    int fpid;
    struct sockaddr_in serv_addr, cli_addr;

    fd_set readfds;
    int maxfdnum;

    FD_ZERO(&allfds);
    FD_ZERO(&readfds);

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

    int yes = 1;
    /* lose the pesky "Address already in use" error message */
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,sizeof(int)) == -1)
    {
        err_dump("server: can't set SO_REUSEADDR");
    }

    if(listen(sockfd, MAXCONN) < 0)    /* -1 if listen fail, 0 if success */
    {
        err_dump("server: can't listen on socket");
    }

    FD_SET(sockfd, &allfds);
    maxfdnum = sockfd;

    while(1)
    {
        readfds = allfds;

        if(select(maxfdnum+1, &readfds, NULL, NULL, (struct timeval*)0) < 0)
        {
            fprintf(stderr, "%s,%d\n", strerror(errno), maxfdnum);
            err_dump("select error");
        }

        int fd;
        for(fd = 0; fd <= maxfdnum; fd++)
        {
            if(FD_ISSET(fd, &readfds))    /*  */
            {
                if(fd == sockfd)    /* this fd is server socket fd, means new client incoming */
                {
                    socklen_t clilen = sizeof(cli_addr);
                    int clifd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);    /* accept client connection */

                    if(clifd == -1)
                    {
                        err_dump("server: accept failed");
                    }
                    else
                    {
                        FD_SET(clifd, &allfds);
                        if(clifd > maxfdnum)
                        {
                            maxfdnum = clifd;
                        }
                        uid = client_init(cli_addr.sin_addr, cli_addr.sin_port, clifd);
                        send_welmsg(clifd);

                        /*char msg[MAX_MSG_LEN];*/
                        /*sprintf(msg, "*** User '%s' entered from %s/%d. ***\n% ",clidata[uid].name, clidata[uid].ip, clidata[uid].port);*/
                        /*yell(msg);*/
                        write(clifd, PROMOT, sizeof(char)*strlen(PROMOT));    /* show promot to client */
                    }
                }
                else    /* some client send message to server */
                {
                    for(uid = 1; uid < MAX_CLIENTS; uid++)
                    {
                        if(clidata[uid].clifd == fd)    /* find the uid of client match the file descriptor */
                        {
                            break;
                        }
                    }
                    headnode = clidata[uid].headnode;
                    curnode = clidata[uid].curnode;
                    tailnode = clidata[uid].tailnode;

                    recv_cli_cmd(fd, uid);    /* for loop to receive cmd from client */
                    write(fd, PROMOT, sizeof(char)*strlen(PROMOT));    /* show promot to client */
                }
            }
        }
    }
    return 0;
}

/*
 * set client uid, pid, default name in shared memory
 * */
int client_init(struct in_addr in, unsigned short in_port, int clifd)
{
    int i;
    for (i = 1; i < MAX_CLIENTS; i++)
    {
        if (clidata[i].uid == -1)
        {
            char *ip_tmp;
            clidata[i].clifd = clifd;                                 /* clifd */
            clidata[i].uid = i;                                       /* uid */
            strcpy(clidata[i].name, "(no name)");                     /* name */
            ip_tmp = inet_ntoa(in);                                   /* ip */
            strcpy(clidata[i].ip, ip_tmp);
            clidata[i].port = ntohs(in_port);                         /* port */
            clidata[i].headnode = create_node(0,"HEAD_NODE",0);       /* headnode */
            clidata[i].curnode = clidata[i].headnode;                 /* curnode */
            clidata[i].tailnode = clidata[i].headnode;                /* tailnode */
            break;
        }
    }
    return i;
}


/*
 * send a colorful welcome message to client
 * */
void send_welmsg(int clifd)
{
    char* colors[] = {ANSI_COLOR_RED, ANSI_COLOR_GREEN, ANSI_COLOR_YELLOW, ANSI_COLOR_BLUE, ANSI_COLOR_MAGENTA, ANSI_COLOR_CYAN};
    char buffer[LINEMAX];
    sprintf(buffer, "%s%s%s", colors[getpid() % 6], WELCOME_MSG, ANSI_COLOR_RESET);    /* different colors of welcome message for client */
    write(clifd, buffer, strlen(buffer));    /* write welcome message to client */
}

/*
 * receive line from client, store line to LineLinkedList, then parse command from line and execute command with pipe
 * */
void recv_cli_cmd(int clifd, int uid)
{
    char buffer[LINEMAX];
    char *delim = "\r\n";

    memset((char *)buffer, '\0', LINEMAX);
    int byte_read = read(clifd, buffer, LINEMAX);

    while(buffer[byte_read-1]!='\n')    /* in Internet, a large string line may be seperate to several packet, and we probably not get \n from one read */
    {
        char* tmpbuffer = buffer;
        char append[LINEMAX];
        memset((char *)append, '\0', LINEMAX);
        byte_read += read(clifd, append, LINEMAX);
        sprintf(buffer, "%s%s", tmpbuffer, append);
    }

    char *line = strtok(buffer, delim);    /* for some program, we may receive more than one line with once client write */
    if(!line)    /* just \r\n ,empty line, line will be NULL after strtok */
    {
        return;
    }

    do
    {
        line = rm_fespace(line);    /* to catch |N in the end of line, we remove all space at the end */

        if(strcmp(line,"exit")==0)
        {
           clean_cli(uid); 
        }

        int is_setenv = reg_match("^(setenv)", line);    /* setenv */
        int is_printenv = reg_match("^(printenv)", line);    /* printenv */
        if(is_setenv||is_printenv)
        {
            // not pipe or redirect, do all by self
            create_linenode(line, 0);
            char ***argvs = parse_cmd_seq(line);
            if(is_setenv)
            {
                setenv(argvs[0][1],argvs[0][2],1);
            }
            else if(is_printenv)
            {
                char *env = getenv(argvs[0][1]);
                char *envstr =  malloc(snprintf(NULL, 0, "%s=%s\n", argvs[0][1], env) + 1);
                sprintf(envstr, "%s=%s\n", argvs[0][1], env);
                write(clifd, envstr, sizeof(char)*strlen(envstr));
            }
            line = strtok(NULL, delim);
            continue;
        }

        int is_tofile = reg_match(">[ ]*[^\\|/]+$", line);    /* match > to file at the end of line */
        if(is_tofile)
        {
            create_linenode(line, 0);
            curnode->filename = rm_fespace(get_filename(line));
            execute_cmdline(parse_cmd_seq(line), clifd);
            line = strtok(NULL, delim);
            continue;
        }

        int pipe_err = reg_match("![1-9][0-9]*$", line);    /* match !N at the end of line */
        int pipe_out = reg_match("\\|[1-9][0-9]*$", line);  /* match |N at the end of line */

        if(!pipe_err && !pipe_out)
        {
            create_linenode(line, 0);    /* xxx | yyy | zzz  */
        }
        else
        {
            create_linenode(line, get_endnum(line));
            if(pipe_err)
            {
                curnode->pipe_err = 1;
            }
        }

        //print_lists(headnode);

        execute_cmdline(parse_cmd_seq(line), clifd);

        line = strtok(NULL, delim);
    }while(line);// strtok return NULL when , NULL pointer is false
}

/*
 * clean data when client exit
 * */
void clean_cli(int uid)
{
    int i;
    for(i = 0;i < MAX_CLIENTS; i++)
    {
        if(clidata[uid].fifofd[i] != -1)
        {
            close(clidata[uid].fifofd[i]);    /* close fd for fifo read */
            clidata[uid].fifofd[i] = -1;      /* reset fifofd to -1 */

            char fifoname[8];
            sprintf(fifoname, ".%dto%d", i, uid);
            if(access(fifoname,F_OK) == 0)
            {
                unlink(fifoname);             /* remove fifo file */
            }
        }
    }

    strcpy(clidata[uid].msg, " ");
    free_lists(headnode);
    shutdown(clidata[uid].clifd, SHUT_RDWR);
    close(clidata[uid].clifd);
    clidata[uid].clifd = -1;
    clidata[uid].uid = -1;
    clidata[uid].linecount = -1;

}


/*
 * one cmdline one node, cmd line may include multiple cmds
 * */
void create_linenode(char* line, int pipetonum)
{

    char *linecopy = malloc(sizeof(char)*strlen(line));
    strcpy(linecopy,line);    /* copy string to store in linelinklist, if use line ,it will be change because of pointer, linecmd of each node will same as last node */

    if(curnode->nextPtr == NULL)    /* curnode equal to tailnode, is at the end of list, so we make new node for incoming cmdline */
    {
        LineNode* newnode = create_node(linecount++,linecopy,pipetonum);
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
            LineNode* newnode = create_node(linecount++, NULL, 0);
            insert_node(tailnode, newnode);
            tailnode = tailnode->nextPtr;
            stillneed--;
        }
    }
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
 * regular expression, if line match pattern return true, else false
 * */
int reg_match(char *pattern, char* line)
{
    int status;
    int cflags = REG_EXTENDED;
    regmatch_t pmatch[1];
    const size_t nmatch = 1;
    regex_t reg;

    regcomp(&reg, pattern, cflags);
    status = regexec(&reg, line, nmatch, pmatch, 0);
    regfree(&reg);

    if(status == REG_NOMATCH)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

char* get_filename(char* line)
{
    int count;
    for(count = strlen(line)-1; count >= 0; count--)   /* count will stop at the index of first non-digital char come from end */
    {
        if(line[count] == '>')
        {
            break;
        }
    }

    char* filename = malloc(sizeof(char)*(strlen(line)-1-count));
    strcpy(filename, &line[count+1]);
    line[count] = 0;    /* remove > xxx from string */
    return filename;
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

    char pipetonum[strlen(line)-1-count];
    strcpy(pipetonum, &line[count+1]);
    line[count] = 0;    /* remove |N from string */
    return atoi(pipetonum);
}

void execute_cmdline(char ***argvs, int clifd)
{
    int C;
    int cmd_count = 0;
    while (argvs[cmd_count])    /* count cmd in this line */
    {
        ++cmd_count;
    }

    int pipes_fd[cmd_count][2];    /* prepare pipe fd ,read from [0], write to [1] , there are MAX_CMD_COUNT fd group, but last one not always used */

    for (C = 0; C < cmd_count; ++C)
    {
        if(C < cmd_count-1)    /* create pipes, use between cmd, pipes equals to command-1 */
        {
            if (pipe(pipes_fd[C]) == -1)
            {
                fprintf(stderr, "Error: Unable to create pipe. (%d)\n", C);
                exit(EXIT_FAILURE);
            }
        }

        int fd_in;    /* this is input fd for cmd, cmd will read input from this fd */

        if(C == 0 && curnode->fd_readout == -1)    /* cmd is at beginning of line, and no earlier line pipe to this line */
        {
            fd_in = STDIN_FILENO;
        }
        else if(C == 0 && curnode->fd_readout != -1)    /* cmd is at beginning of line, but some earlier line pipe output to this line */
        {
            fd_in = curnode->fd_readout;
        }
        else    /* cmd is not at beginning of line, just read input from previous pipe */
        {
            fd_in = pipes_fd[C - 1][0];
        }

        int fd_out;    /* this is output fd for cmd, cmd will write output to this fd */

        if(C == cmd_count-1 && curnode->filename != NULL)
        {
            curnode->fd_tofile = open(curnode->filename, O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IROTH);
            fd_out = curnode->fd_tofile;
        }
        else if(C == cmd_count-1 && curnode->pipeto == 0)    /* cmd is at the end of line, and no pipe to later line */
        {
            fd_out = clifd;
        }
        else if(C == cmd_count-1 && curnode->pipeto != 0)    /* cmd is at the end of line, but pipe to later line */
        {
            LineNode* tmpnode = curnode;
            while(tmpnode->linenum != curnode->linenum + curnode->pipeto)    /* find the node of line we want to pipe to */
            {
                tmpnode = tmpnode->nextPtr;
            }

            if(tmpnode->fd_readout == -1)    /* if no other earlier line pipe to the same line we want to pipe (for later line node, this is the first line want to pipe to it)*/
            {
                if (pipe(pipes_fd[C]) == -1)    /* create a new pipe */
                {
                    fprintf(stderr, "Error: Unable to create pipe. (%d)\n", C);
                    exit(EXIT_FAILURE);
                }
                tmpnode->fd_readout = pipes_fd[C][0];    /* keep the last pipe fd [0] (read end) in later line node fd_readout */
                tmpnode->fd_writein = pipes_fd[C][1];    /* keep the last pipe fd [1] (write end) in later line node fd_writein */
            }

            fd_out = tmpnode->fd_writein;    /* output fd is the last one of pipe [1] (write end) */
        }
        else    /* cmd is not at the end of line, just write output to next pipe */
        {
            fd_out = pipes_fd[C][1];
        }

        int fd_err;
        if(C == cmd_count-1 && curnode->pipe_err == 1)
        {
            fd_err = fd_out;
        }
        else
        {
            fd_err = clifd;
        }

        if(C == 0 && curnode->fd_writein >= 0)    /* if cmd at the first of begining, and this line will read in from previous line pipe */
        {
            close(curnode->fd_writein);    /* we need to close previous pipe writein end to send EOF to the first cmd of this line */
        }

        creat_proc(argvs[C], fd_in, fd_out, fd_err, C, pipes_fd);

        if(C != cmd_count-1 || curnode->fd_tofile != -1)    /* cmd pipe to cmd or cmd output to file*/
        {
            /*we need to close fd_out to send an EOF, avoid next cmd stuck (e.g: cat will not end until receive EOF)*/
            close(fd_out);
        }

        if(C != 0)    /* close fd_in but avoid first cmd, don't close STDIN_FILENO */
        {
            close(fd_in);
        }

        int stat;
        wait(&stat);    /* wait for the client who call exec to run cmd, avoid child become zombie */
    }
}

void creat_proc(char **argv, int fd_in, int fd_out, int fd_err, int pipes_count, int pipes_fd[][2])
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
            if(dup2(fd_in, STDIN_FILENO)==-1)
                err_dump("FD_IN ERROR");
        }
        if (fd_out != STDOUT_FILENO)
        {
            if(dup2(fd_out, STDOUT_FILENO)==-1)
                err_dump("FD_OUT ERROR");
        }
        if(fd_err != STDERR_FILENO)
        {
            if(dup2(fd_err, STDERR_FILENO)==-1)
                err_dump("FD_ERR ERROR");
        }

        int P;
        for (P = 0; P < pipes_count; ++P)
        {
            close(pipes_fd[P][0]);
            close(pipes_fd[P][1]);
        }

        if(curnode->fd_readout >= 0)
        {
            close(curnode->fd_readout);
            close(curnode->fd_writein);
        }

        if(curnode->fd_tofile >= 0)
        {
            close(curnode->fd_tofile);
        }

        if (execvp(argv[0], argv) == -1)
        {
            fprintf(stderr, "Unknown command: [%s].\n", argv[0]);
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
