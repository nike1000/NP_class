#include "../include/netshell.h"
#include "../include/li3.h"
#include "../include/myshm.h"

LineNode *headnode = NULL;
LineNode *curnode = NULL;
LineNode *tailnode = NULL;
int linecount = 1;
int clifd;
int uid;
int shmid;
CliInfo *shmdata;
int sbchk_flag = 0; /* recode type of second symbol */
int sb_data = -1;   /* recode the data in symbol_chk */

int main()
{
    chdir("rwg/");
    setenv("PATH", "bin:.", 1);
    headnode = create_node(0, "HEAD_NODE", 0);
    curnode = headnode;
    tailnode = headnode;

    signal(SIGALRM, sigMsg);  /* receive tell or yell msg */
    signal(SIGPIPE, sigPipe); /* open fifo for read, avoid write end block or open error with O_NONBLOCK */

    initshm(); /* allocate shared memory and attach shared memory by shmget and shmat function */

    /* start server socket and appcet connection,
     * after accept and fork, child process will return client file descriptor,
     *  server return -1 */
    clifd = start_server();

    if (clifd == -1) /* Parent return */
    {
        exit(0);
    }
    else
    {
        commuto_client();
    }
    return 0;
}

/*
 * when a client yell or tell, that client will copy msg to shm for other client,
 * and send a signal to other client.
 * So, when process get an signal, write the msg to clifd
 * */
void sigMsg(int sig)
{
    write(clifd, shmdata[uid].msg, strlen(shmdata[uid].msg));
    strcpy(shmdata[uid].msg, " ");
}

/*
 * open fifo for write will block until that fifo have open for read by default,
 * if open fifo for write with O_NONBLOCK flags, open for write will get ENXIO error without read end,
 * so after we create fifo, but before write, we send signal to receiver to open fifo for read avoid block
 * */
void sigPipe(int sig)
{
    int i;
    char fifoname[8];
    for (i = 1; i < MAX_CLIENTS; i++)
    {
        sprintf(fifoname, ".%dto%d", i, uid);
        if (access(fifoname, F_OK) == 0 && shmdata[uid].fifofd[i] == -1)
        {
            shmdata[uid].fifofd[i] = open(fifoname, O_RDONLY | O_NONBLOCK);
        }
    }
}

/*
 * get shared memory to store client info in CliInfo struct,
 * attech shared memory and init uid and pid for CliInfo inshm
 * */
void initshm()
{
    shmid = shmget(SHMKEY, sizeof(CliInfo) * MAX_CLIENTS, IPC_CREAT | 0666);
    if (shmid < 0)
    {
        err_dump("Init shared memory error");
    }

    if ((shmdata = (CliInfo *)shmat(shmid, NULL, 0)) < 0)
    {
        err_dump("Init shared memory error");
    }

    int i;
    for (i = 1; i < MAX_CLIENTS; i++)
    {
        shmdata[i].uid = -1;
        shmdata[i].pid = -1;

        int j;
        for (j = 1; j < MAX_CLIENTS; j++)
        {
            shmdata[i].fifofd[j] = -1;
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

    /* server socket file descriptor */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) /* sockfd = -1 if socket() error */
    {
        err_dump("server: can't open stream socket");
    }

    memset((char *)&serv_addr, '\0', sizeof(serv_addr)); /* set serv_addr all bytes to zero*/
    serv_addr.sin_family = AF_INET;                      /* AF_INET for IPv4 protocol */
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);       /* INADDR_ANY for listening on any IP address */
    serv_addr.sin_port = htons(SERV_TCP_PORT);           /* Server port number */

    int yes = 1;
    /* lose the pesky "Address already in use" error message */
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
        err_dump("server: can't set SO_REUSEADDR");
    }

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) < 0) /* -1 if bind fail, 0 if success */
    {
        err_dump("server: can't bind local address");
    }

    if (listen(sockfd, MAXCONN) < 0) /* -1 if listen fail, 0 if success */
    {
        err_dump("server: can't listen on socket");
    }

    for (;;)
    {
        socklen_t clilen = sizeof(cli_addr);
        clifd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen); /* accept client connection */

        if (clifd == -1)
        {
            err_dump("server: accept failed");
        }

        fpid = fork(); /* server get a connection, fork a child to communicate with client, and parent keep accept another client */
        if (fpid < 0)
        {
            err_dump("server: fork error");
        }
        else if (fpid > 0) /* Parent Process */
        {
            close(clifd); /* close clifd in parent process, we need to wait for next connection */
        }
        else /* First Child Process */
        {
            int spid;                /* Second fork PID */
            if ((spid = fork()) < 0) /* we fork twice and exit first child immediate ,let init be the parent of second child to avoid zombie */
            {
                err_dump("server: fork error");
            }
            else if (spid > 0) /* First Child Process */
            {
                exit(0);
            }
            else /* Second Child Process */
            {
                close(sockfd); /* close sockfd in child process, we only need clifd to communicate with client */
                uid = client_init(cli_addr.sin_addr, cli_addr.sin_port);
                return clifd; /* return clifd to main function to execute next function */
            }
        }

        if (waitpid(fpid, NULL, 0) != fpid) /* wait for First Child Process, it will exit immediate after fork */
        {
            err_dump("server: waitpid error");
        }
    }
    return -1;
}

/*
 * set client uid, pid, default name in shared memory
 * */
int client_init(struct in_addr in, unsigned short in_port)
{
    int i;
    for (i = 1; i < MAX_CLIENTS; i++)
    {
        if (shmdata[i].pid == -1)
        {
            char *ip_tmp;
            shmdata[i].pid = getpid();
            shmdata[i].uid = i;
            ip_tmp = inet_ntoa(in);
            strcpy(shmdata[i].ip, ip_tmp);
            shmdata[i].port = ntohs(in_port);
            strcpy(shmdata[i].name, "(no name)");
            break;
        }
    }
    return i;
}

void commuto_client()
{
    char msg[MAX_MSG_LEN];
    send_welmsg(clifd);
    sprintf(msg, "*** User '%s' entered from %s/%d. ***\n% ", shmdata[uid].name, shmdata[uid].ip, shmdata[uid].port);
    yell(msg);
    recv_cli_cmd(clifd); /* for loop to receive cmd from client */
    sprintf(msg, "*** User '%s' left. ***\n", shmdata[uid].name);
    yell(msg);
    clean_cli(uid, shmid);
}

/*
 * send a colorful welcome message to client
 * */
void send_welmsg(int clifd)
{
    char *colors[] = {ANSI_COLOR_RED, ANSI_COLOR_GREEN, ANSI_COLOR_YELLOW, ANSI_COLOR_BLUE, ANSI_COLOR_MAGENTA, ANSI_COLOR_CYAN};
    char buffer[LINEMAX];
    sprintf(buffer, "%s%s%s", colors[getpid() % 6], WELCOME_MSG, ANSI_COLOR_RESET); /* different colors of welcome message for client */
    write(clifd, buffer, strlen(buffer));                                           /* write welcome message to client */
}

/*
 * clean data when client exit
 * */
void clean_cli(int uid, int shmid)
{
    shmdata[uid].pid = -1;
    shmdata[uid].uid = -1;

    int i;
    for (i = 1; i < MAX_CLIENTS; i++)
    {
        if (shmdata[uid].fifofd[i] != -1)
        {
            close(shmdata[uid].fifofd[i]); /* close fd for fifo read */
            shmdata[uid].fifofd[i] = -1;   /* reset fifofd to -1 */

            char fifoname[8];
            sprintf(fifoname, ".%dto%d", i, uid);
            if (access(fifoname, F_OK) == 0)
            {
                unlink(fifoname); /* remove fifo file */
            }
        }
    }

    strcpy(shmdata[uid].msg, " ");
    shmdt(shmdata);
    shmctl(shmid, IPC_RMID, NULL);
    free_lists(headnode);
    shutdown(clifd, SHUT_RDWR);
    close(clifd);
}

/*
 * receive line from client, store line to LineLinkedList, then parse command from line and execute command with pipe
 * */
void recv_cli_cmd(int clifd)
{
    char buffer[LINEMAX];
    char *delim = "\r\n";

    for (;;)
    {
        write(clifd, PROMOT, sizeof(char) * strlen(PROMOT)); /* show promot to client */
        memset((char *)buffer, '\0', LINEMAX);
        int byte_read = read(clifd, buffer, LINEMAX);

        while (buffer[byte_read - 1] != '\n') /* in Internet, a large string line may be seperate to several packet, and we probably not get \n from one read */
        {
            char *tmpbuffer = buffer;
            char append[LINEMAX];
            memset((char *)append, '\0', LINEMAX);
            byte_read += read(clifd, append, LINEMAX);
            sprintf(buffer, "%s%s", tmpbuffer, append);
        }

        char *line, *last = NULL;
        /* for some program, we may receive more than one line with once client write */
        for (line = strtok_r(buffer, delim, &last); line; line = strtok_r(NULL, delim, &last))
        {
            line = rm_fespace(line); /* to catch |N in the end of line, we remove all space at the end */

            if (reg_match("^(exit)", line))
            {
                return;
            }
            else if (reg_match("^(who)", line))
            {
                create_linenode(line, 0);
                who();
            }
            else if (reg_match("^(name)", line))
            {
                create_linenode(line, 0);
                char *newname = line + 5; /* 5 offset of char to skip cmd 'name' and a space at beginning of line */
                if (strlen(newname) > 20)
                {
                    newname[20] = 0;
                }
                name(newname);
            }
            else if (reg_match("^(yell)", line))
            {
                create_linenode(line, 0);
                char buf[MAX_MSG_LEN + 50];
                char *msg = line + 5; /* 5 offset of char to skip cmd 'yell' and a space at beginning of line */
                if (strlen(msg) > MAX_MSG_LEN)
                {
                    msg[MAX_MSG_LEN] = 0;
                }
                sprintf(buf, "*** %s yelled ***: %s\n", shmdata[uid].name, msg);
                yell(buf);
            }
            else if (reg_match("^(tell)", line))
            {
                create_linenode(line, 0);
                char buf[MAX_MSG_LEN + 50], tmp[MAX_MSG_LEN + 50];
                strcpy(tmp, line);

                char ***argvs = parse_cmd_seq(line);
                int touid = atoi(argvs[0][1]);
                char *msg = tmp + strlen(argvs[0][0]) + strlen(argvs[0][1]) + 2; /* offset of char to skip cmd 'tell' ,touid and two spaces at beginning of line */
                if (strlen(msg) > MAX_MSG_LEN)
                {
                    msg[MAX_MSG_LEN] = 0;
                }
                sprintf(buf, "*** %s told you ***: %s\n", shmdata[uid].name, msg);
                tell(buf, touid);
            }
            else if (reg_match("^(setenv)", line))
            {
                create_linenode(line, 0);
                char ***argvs = parse_cmd_seq(line);
                setenv(argvs[0][1], argvs[0][2], 1);
            }
            else if (reg_match("^(printenv)", line))
            {
                create_linenode(line, 0);
                char ***argvs = parse_cmd_seq(line);
                char *env = getenv(argvs[0][1]);
                char *envstr = malloc(snprintf(NULL, 0, "%s=%s\n", argvs[0][1], env) + 1);
                sprintf(envstr, "%s=%s\n", argvs[0][1], env);
                write(clifd, envstr, sizeof(char) * strlen(envstr));
            }
            else if (reg_match(">[1-9][0-9]*$", line))
            {
                create_linenode(line, 0);
                int pipetouid = get_endnum(line);
                char ***argvs = parse_cmd_seq(line);
                char fifoname[8], msg[MAX_MSG_LEN];
                sprintf(fifoname, ".%dto%d", uid, pipetouid);

                if (pipe_to_user(pipetouid, fifoname))
                {
                    int len = 0;
                    while (argvs[0][len])
                    {
                        ++len;
                    }
                    int sbchk_flag = symbol_chk(argvs);
                    if (sbchk_flag == 1) /* case: cmd <N >M */
                    {
                        argvs[0][len - 1] = NULL;
                        sprintf(msg, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", shmdata[uid].name, uid, shmdata[sb_data].name, sb_data, curnode->cmdline);
                        yell(msg);
                    }
                    else if (sbchk_flag > 0)
                    {
                        char *msg = "Illegal Symbol\n";
                        write(clifd, msg, strlen(msg));
                        return;
                    }

                    curnode->filename = fifoname;
                    curnode->is_fifofile = 1;
                    curnode->pipe_err = 1;
                    execute_cmdline(argvs);

                    if (sbchk_flag == 1) /* case cat <N >M */
                    {
                        shmdata[uid].fifofd[sb_data] = -1;
                        sprintf(fifoname, ".%dto%d", sb_data, uid);
                        unlink(fifoname);
                    }
                    sprintf(msg, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n", shmdata[uid].name, uid, curnode->cmdline, shmdata[pipetouid].name, pipetouid);
                    yell(msg);
                }
            }
            else if (reg_match("<[1-9][0-9]*$", line))
            {
                create_linenode(line, 0);
                int pipefromuid = get_endnum(line);
                char ***argvs = parse_cmd_seq(line);
                char fifoname[8], msg[MAX_MSG_LEN];
                sprintf(fifoname, ".%dto%d", pipefromuid, uid);
                if (pipe_from_user(pipefromuid, fifoname))
                {
                    int len = 0;
                    while (argvs[0][len])
                    {
                        ++len;
                    }

                    int sbchk_flag = symbol_chk(argvs);
                    if (sbchk_flag == 1) /* Illegal case: cmd <N <M */
                    {
                        char *msg = "Illegal Symbol\n";
                        write(clifd, msg, strlen(msg));
                        return;
                    }
                    else if (sbchk_flag == 2) /* case cmd >N <M */
                    {
                        argvs[0][len - 1] = NULL;
                    }
                    else if (sbchk_flag == 3) /* case: cmd > file <M */
                    {
                        argvs[0][len - 1] = NULL;
                        argvs[0][len - 2] = NULL;
                    }
                    sprintf(msg, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", shmdata[uid].name, uid, shmdata[pipefromuid].name, pipefromuid, curnode->cmdline);
                    yell(msg);
                    execute_cmdline(argvs);

                    if (sbchk_flag == 2) /* case cat >N <M */
                    {
                        sprintf(msg, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n", shmdata[uid].name, uid, curnode->cmdline, shmdata[sb_data].name, sb_data);
                        yell(msg);
                    }

                    shmdata[uid].fifofd[pipefromuid] = -1;
                    sprintf(fifoname, ".%dto%d", pipefromuid, uid);
                    unlink(fifoname);
                }
            }
            else if (reg_match(">[ ]*[^\\|/]+$", line)) /* match > to file at the end of line */
            {
                char msg[MAX_MSG_LEN], fifoname[8];
                create_linenode(line, 0);
                curnode->filename = rm_fespace(get_filename(line));
                char ***argvs = parse_cmd_seq(line);

                int len = 0;
                while (argvs[0][len])
                {
                    ++len;
                }

                int sbchk_flag = symbol_chk(argvs);
                if (sbchk_flag == 1) /* case: cmd <N > file */
                {
                    argvs[0][len - 1] = NULL;
                    sprintf(msg, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", shmdata[uid].name, uid, shmdata[sb_data].name, sb_data, curnode->cmdline);
                    yell(msg);
                }
                else if (sbchk_flag > 0)
                {
                    char *msg = "Illegal Symbol\n";
                    write(clifd, msg, strlen(msg));
                    return;
                }

                execute_cmdline(argvs);
                if (sbchk_flag == 1) /* case cat <N >M */
                {
                    shmdata[uid].fifofd[sb_data] = -1;
                    sprintf(fifoname, ".%dto%d", sb_data, uid);
                    unlink(fifoname);
                }
            }
            else
            {
                int pipe_err = reg_match("![1-9][0-9]*$", line);   /* match !N at the end of line */
                int pipe_out = reg_match("\\|[1-9][0-9]*$", line); /* match |N at the end of line */

                if (!pipe_err && !pipe_out)
                {
                    create_linenode(line, 0); /* xxx | yyy | zzz  */
                }
                else
                {
                    create_linenode(line, get_endnum(line));
                    if (pipe_err)
                    {
                        curnode->pipe_err = 1;
                    }
                }

                char ***argvs = parse_cmd_seq(line);
                char fifoname[8], msg[MAX_MSG_LEN];

                int len = 0;
                while (argvs[0][len])
                {
                    ++len;
                }

                int sbchk_flag = symbol_chk(argvs);
                if (sbchk_flag == 1) /* case: cmd <N |N */
                {
                    argvs[0][len - 1] = NULL;
                    sprintf(msg, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", shmdata[uid].name, uid, shmdata[sb_data].name, sb_data, curnode->cmdline);
                    yell(msg);
                }
                else if (sbchk_flag > 0)
                {
                    char *msg = "Illegal Symbol\n";
                    write(clifd, msg, strlen(msg));
                    return;
                }

                execute_cmdline(argvs);

                if (sbchk_flag == 1) /* case cat <N >M */
                {
                    shmdata[uid].fifofd[sb_data] = -1;
                    sprintf(fifoname, ".%dto%d", sb_data, uid);
                    unlink(fifoname);
                }
            }

            // print_lists(headnode);
        }
    }
}

/*
 * one cmdline one node, cmd line may include multiple cmds
 * */
void create_linenode(char *line, int pipetonum)
{
    char *linecopy = malloc(sizeof(char) * strlen(line));
    strcpy(linecopy, line); /* copy string to store in linelinklist, if use line ,it will be change because of pointer, linecmd of each node will same as last node */

    if (curnode->nextPtr == NULL) /* curnode equal to tailnode, is at the end of list, so we make new node for incoming cmdline */
    {
        LineNode *newnode = create_node(linecount++, linecopy, pipetonum);
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
    if (pipetonum > 0) /* end of line is |N */
    {
        int stillneed = pipetonum - ((tailnode->linenum) - (curnode->linenum)); /* some empty node may be create by earlier |N cmdline, we only need to create the number of node we still need*/

        while (stillneed > 0)
        {
            LineNode *newnode = create_node(linecount++, NULL, 0);
            insert_node(tailnode, newnode);
            tailnode = tailnode->nextPtr;
            stillneed--;
        }
    }
}

/*
 * remove whitespace at the front and end of the string
 * */
char *rm_fespace(char *line)
{
    int count;
    if (!line) /* if line is NULL pointer (from end of strtok) */
    {
        return line;
    }

    while (isspace(*line))
    {
        ++line; /* size of char is 1 byte, *line is char, add 1(byte) to its address means we don't need this char */
    }

    for (count = strlen(line) - 1; count >= 0; count--)
    {
        if (line[count] == ' ')
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
int reg_match(char *pattern, char *line)
{
    int status;
    int cflags = REG_EXTENDED;
    regmatch_t pmatch[1];
    const size_t nmatch = 1;
    regex_t reg;

    regcomp(&reg, pattern, cflags);
    status = regexec(&reg, line, nmatch, pmatch, 0);
    regfree(&reg);

    if (status == REG_NOMATCH)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

char *get_filename(char *line)
{
    int count;
    for (count = strlen(line) - 1; count >= 0; count--) /* count will stop at the index of first non-digital char come from end */
    {
        if (line[count] == '>')
        {
            break;
        }
    }

    char *filename = malloc(sizeof(char) * (strlen(line) - 1 - count));
    strcpy(filename, &line[count + 1]);
    line[count] = 0; /* remove > xxx from string */
    return filename;
}

/*
 * check whether tje end of line is legal, get |N number at the end of line, then remove it from line
 * */
int get_endnum(char *line)
{
    int count;
    for (count = strlen(line) - 1; count >= 0; count--) /* count will stop at the index of first non-digital char come from end */
    {
        if (!isdigit(line[count]))
        {
            break;
        }
    }

    char pipetonum[strlen(line) - 1 - count];
    strcpy(pipetonum, &line[count + 1]);
    line[count] = 0; /* remove |N from string */
    return atoi(pipetonum);
}

void execute_cmdline(char ***argvs)
{
    int C;
    int cmd_count = 0;
    while (argvs[cmd_count]) /* count cmd in this line */
    {
        ++cmd_count;
    }

    int pipes_fd[cmd_count][2]; /* prepare pipe fd ,read from [0], write to [1] , there are MAX_CMD_COUNT fd group, but last one not always used */

    for (C = 0; C < cmd_count; ++C)
    {
        if (C < cmd_count - 1) /* create pipes, use between cmd, pipes equals to command-1 */
        {
            if (pipe(pipes_fd[C]) == -1)
            {
                fprintf(stderr, "Error: Unable to create pipe. (%d)\n", C);
                exit(EXIT_FAILURE);
            }
        }

        int fd_in; /* this is input fd for cmd, cmd will read input from this fd */

        if (C == 0 && curnode->fd_readout == -1) /* cmd is at beginning of line, and no earlier line pipe to this line */
        {
            fd_in = STDIN_FILENO;
        }
        else if (C == 0 && curnode->fd_readout != -1) /* cmd is at beginning of line, but some earlier line pipe output to this line */
        {
            fd_in = curnode->fd_readout;
        }
        else /* cmd is not at beginning of line, just read input from previous pipe */
        {
            fd_in = pipes_fd[C - 1][0];
        }

        int fd_out; /* this is output fd for cmd, cmd will write output to this fd */

        if (C == cmd_count - 1 && curnode->filename != NULL)
        {
            if (curnode->is_fifofile) /* to fife file */
            {
                if ((curnode->fd_tofile = open(curnode->filename, O_WRONLY)) == -1)
                {
                    err_dump(strerror(errno));
                }
            }
            else /* to regular file */
            {
                if ((curnode->fd_tofile = open(curnode->filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IROTH)) == -1)
                {
                    err_dump(strerror(errno));
                }
            }
            fd_out = curnode->fd_tofile;
        }
        else if (C == cmd_count - 1 && curnode->pipeto == 0) /* cmd is at the end of line, and no pipe to later line */
        {
            fd_out = clifd;
        }
        else if (C == cmd_count - 1 && curnode->pipeto != 0) /* cmd is at the end of line, but pipe to later line */
        {
            LineNode *tmpnode = curnode;
            while (tmpnode->linenum != curnode->linenum + curnode->pipeto) /* find the node of line we want to pipe to */
            {
                tmpnode = tmpnode->nextPtr;
            }

            if (tmpnode->fd_readout == -1) /* if no other earlier line pipe to the same line we want to pipe (for later line node, this is the first line want to pipe to it)*/
            {
                if (pipe(pipes_fd[C]) == -1) /* create a new pipe */
                {
                    fprintf(stderr, "Error: Unable to create pipe. (%d)\n", C);
                    exit(EXIT_FAILURE);
                }
                tmpnode->fd_readout = pipes_fd[C][0]; /* keep the last pipe fd [0] (read end) in later line node fd_readout */
                tmpnode->fd_writein = pipes_fd[C][1]; /* keep the last pipe fd [1] (write end) in later line node fd_writein */
            }

            fd_out = tmpnode->fd_writein; /* output fd is the last one of pipe [1] (write end) */
        }
        else /* cmd is not at the end of line, just write output to next pipe */
        {
            fd_out = pipes_fd[C][1];
        }

        int fd_err;
        if (C == cmd_count - 1 && curnode->pipe_err == 1)
        {
            fd_err = fd_out;
        }
        else
        {
            fd_err = clifd;
        }

        if (C == 0 && curnode->fd_writein >= 0) /* if cmd at the first of begining, and this line will read in from previous line pipe */
        {
            close(curnode->fd_writein); /* we need to close previous pipe writein end to send EOF to the first cmd of this line */
        }

        creat_proc(argvs[C], fd_in, fd_out, fd_err, C, pipes_fd);

        if (C != cmd_count - 1 || curnode->fd_tofile != -1) /* cmd pipe to cmd or cmd output to file*/
        {
            /*we need to close fd_out to send an EOF, avoid next cmd stuck (e.g: cat will not end until receive EOF)*/
            close(fd_out);
        }

        if (C == 0 && curnode->fd_readout >= 0)
        {
            close(curnode->fd_readout);
        }

        if (C != 0) /* close fd_in but avoid first cmd, don't close STDIN_FILENO */
        {
            close(fd_in);
        }

        int stat;
        wait(&stat); /* wait for the client who call exec to run cmd, avoid child become zombie */

        if (WIFEXITED(stat)) /* if occur Unknown command, give up other cmds after this in the same line */
        {
            if (WEXITSTATUS(stat) == EXIT_FAILURE)
            {
                break;
            }
        }
    }
}

int symbol_chk(char ***argvs)
{
    int len = 0;
    char **fircmd = argvs[0];
    char buf[MAX_MSG_LEN];

    while (argvs[0][len])
    {
        ++len;
    }
    len--;

    sprintf(buf, "%s %s", fircmd[len - 1], fircmd[len]);
    if (reg_match("<[1-9][0-9]*$", fircmd[len]))
    {
        int pipefromuid = get_endnum(fircmd[len]);
        char *fifoname = malloc(sizeof(char) * 8);
        sprintf(fifoname, ".%dto%d", pipefromuid, uid);
        if (pipe_from_user(pipefromuid, fifoname))
        {
            sb_data = pipefromuid;
            return 1;
        }
    }
    else if (reg_match(">[1-9][0-9]*$", fircmd[len]))
    {
        int pipetouid = get_endnum(fircmd[len]);
        char *fifoname = malloc(sizeof(char) * 8);
        sprintf(fifoname, ".%dto%d", uid, pipetouid);
        if (pipe_to_user(pipetouid, fifoname))
        {
            curnode->filename = fifoname;
            curnode->is_fifofile = 1;
            curnode->pipe_err = 1;
            sb_data = pipetouid;
        }
        return 2;
    }
    else if (reg_match("> [^\\|/]+$", buf))
    {
        curnode->filename = fircmd[len];
        return 3;
    }
    else
    {
        return 0;
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
            if (dup2(fd_in, STDIN_FILENO) == -1)
                err_dump("FD_IN ERROR");
        }
        if (fd_out != STDOUT_FILENO)
        {
            if (dup2(fd_out, STDOUT_FILENO) == -1)
                err_dump("FD_OUT ERROR");
        }
        if (fd_err != STDERR_FILENO)
        {
            if (dup2(fd_err, STDERR_FILENO) == -1)
                err_dump("FD_ERR ERROR");
        }

        int P;
        for (P = 0; P < pipes_count; ++P)
        {
            close(pipes_fd[P][0]);
            close(pipes_fd[P][1]);
        }

        if (curnode->fd_readout >= 0)
        {
            close(curnode->fd_readout);
            close(curnode->fd_writein);
        }

        if (curnode->fd_tofile >= 0)
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

    char *last = NULL;
    cmds[0] = rm_fespace(strtok_r(str, "|", &last));
    for (i = 1; i <= MAX_CMD_COUNT; ++i)
    {
        cmds[i] = rm_fespace(strtok_r(NULL, "|", &last));
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

        argvs[i][0] = strtok_r(cmds[i], " \t\n\r", &last);
        for (j = 1; j <= MAX_ARG_COUNT; ++j)
        {
            argvs[i][j] = strtok_r(NULL, " \t\n\r", &last);
            if (argvs[i][j] == NULL)
            {
                break;
            }
        }
    }

    return argvs;
}

void who()
{
    char msg[1024];
    strcpy(msg, "<ID>      <nickname>               <IP/port>               <indicate me>\n");
    write(clifd, msg, strlen(msg));

    int i;
    for (i = 1; i < MAX_CLIENTS; i++)
    {
        if (shmdata[i].uid != -1)
        {
            if (shmdata[i].uid == uid)
            {
                sprintf(msg, "%-10d%-25s%-s/%-8d<-me\n", shmdata[i].uid, shmdata[i].name, shmdata[i].ip, shmdata[i].port);
            }
            else
            {
                sprintf(msg, "%-10d%-25s%-s/%-8d\n", shmdata[i].uid, shmdata[i].name, shmdata[i].ip, shmdata[i].port);
            }
            write(clifd, msg, strlen(msg));
        }
    }
}

void name(char *newname)
{
    int i, exist = 0;
    char msg[MAX_MSG_LEN];

    for (i = 1; i < MAX_CLIENTS; i++)
    {
        if (strcmp(newname, shmdata[i].name) == 0)
        {
            exist = 1;
            break;
        }
    }

    if (exist)
    {
        sprintf(msg, "*** User '%s' already exists. ***\n", newname);
        write(clifd, msg, strlen(msg));
    }
    else
    {
        strcpy(shmdata[uid].name, newname);
        sprintf(msg, "*** User from %s/%d is named '%s'. ***\n", shmdata[uid].ip, shmdata[uid].port, shmdata[uid].name);
        yell(msg);
    }
}

void tell(char *msg, int touid)
{
    if (shmdata[touid].pid != -1)
    {
        strcpy(shmdata[touid].msg, msg);
        kill(shmdata[touid].pid, SIGALRM);
    }
    else
    {
        char msg[MAX_MSG_LEN];
        sprintf(msg, "*** Error: user #%d does not exist yet. ***\n", touid);
        write(clifd, msg, strlen(msg));
    }
}

void yell(char *msg)
{
    int i;
    for (i = 1; i < MAX_CLIENTS; i++)
    {
        if (shmdata[i].pid != -1)
        {
            strcpy(shmdata[i].msg, msg);
            kill(shmdata[i].pid, SIGALRM);
        }
    }
}

int pipe_to_user(int touid, char *fifoname)
{
    char msg[MAX_MSG_LEN];
    if (shmdata[touid].uid == -1)
    {
        sprintf(msg, "*** Error: user #%d does not exist yet. ***\n", touid);
        write(clifd, msg, strlen(msg));
    }
    else
    {
        int status = mknod(fifoname, S_IFIFO | 0666, 0);
        if (status)
        {
            if (errno == EEXIST)
            {
                sprintf(msg, "*** Error: the pipe #%d->#%d already exists. ***\n", uid, touid);
                write(clifd, msg, strlen(msg));
            }
            else
            {
                err_dump("FIFO create fail.");
            }
        }
        else
        {
            kill(shmdata[touid].pid, SIGPIPE);
            return 1;
        }
    }
    return 0;
}

int pipe_from_user(int fromuid, char *fifoname)
{
    char msg[MAX_MSG_LEN];
    if (shmdata[uid].fifofd[fromuid] == -1)
    {
        sprintf(msg, "*** Error: the pipe #%d->#%d does not exist yet. ***\n", fromuid, uid);
        write(clifd, msg, strlen(msg));
    }
    else
    {
        curnode->fd_readout = shmdata[uid].fifofd[fromuid];
        return 1;
    }
    return 0;
}

void err_dump(char *string)
{
    fprintf(stderr, "%s\n", string);
    exit(EXIT_FAILURE);
}
