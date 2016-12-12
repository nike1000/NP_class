#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_CLIENTS 5
#define BUFFER_LEN 1024
#define COLOR_WELCOME_MSG 1

int parseQueryString();
int responseHTML();
int openBatchFile();
int connectServer();
int script_content(char *buffer, int id, int bold);
int prompt_check(char *buffer);

typedef struct ClientInfo
{
    int id;
    int clifd;
    char host_ip[64];
    char host_port[6];
    char batch_file[32];
    int is_active;
    FILE *batch_fp;
} ClientInfo;

ClientInfo clients[5];

int main()
{
    parseQueryString();
    responseHTML();
    openBatchFile();
    connectServer();
}

int parseQueryString()
{
    /* QUERY_STRING: h1=npbsd3.cs.nctu.edu.tw&p1=6085&f1=t1.txt&h2=&p2=&f2=&h3=&p3=&f3=&h4=&p4=&f4=&h5=&p5=&f5= */
    char *query_string = getenv("QUERY_STRING");

    if (query_string != NULL)
    {
        int i = 0;
        for (i = 0; i < MAX_CLIENTS; i++)
        {
            clients[i].id = i;
            /* +3 to remove h1= p1= f1= ... */
            strcpy(clients[i].host_ip, strtok((i == 0 ? query_string : NULL), "&") + 3);
            strcpy(clients[i].host_port, strtok(NULL, "&") + 3);
            strcpy(clients[i].batch_file, strtok(NULL, "&") + 3);

            /* active only if host, port, file all complete */
            if (strlen(clients[i].host_ip) && strlen(clients[i].host_port) && strlen(clients[i].batch_file))
            {
                clients[i].is_active = 1;
            }
            else
            {
                clients[i].is_active = 0;
            }
        }
    }
    return 0;
}

int responseHTML()
{
    printf("Content-Type: text/html\n\n");
    printf("<html>\n");
    printf("<head>\n");
    printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=big5\" />\n");
    printf("<title>Network Programming Homework 3</title>\n");
    printf("</head>\n");
    printf("<body bgcolor=\"#336699\">\n");
    printf("<font face=\"Courier New\" size=2 color=#FFFF99>");
    printf("<table width=\"800\" border=\"1\">\n");
    printf("<tr>\n");

    int i;
    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].is_active)
        {
            printf("<th>%s</th>\n", clients[i].host_ip);
        }
    }
    printf("</tr>\n");

    printf("<tr>\n");

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].is_active)
        {
            printf("<td valign=\"top\" id=\"m%d\"></td>\n", i);
        }
    }
    printf("</tr>\n");
    printf("</table>\n");
    printf("</body>\n");
    printf("</html>\n");
    return 0;
}

int openBatchFile()
{
    int i;
    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].is_active)
        {
            /* inactive client if batch file can't read */
            if ((clients[i].batch_fp = fopen(clients[i].batch_file, "r")) == NULL)
            {
                fprintf(stderr, "fopen error:");
                perror(strerror(errno));
                clients[i].is_active = 0;
            }
        }
    }
    return 0;
}

int connectServer()
{
    fd_set readfds, allfds;
    int maxfdnum = 0;

    /* clean FD set */
    FD_ZERO(&allfds);
    FD_ZERO(&readfds);

    int i;
    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].is_active)
        {
            struct hostent *hostname;
            struct sockaddr_in cli_addr;

            if ((hostname = gethostbyname(clients[i].host_ip)) == NULL)
            {
                fprintf(stderr, "gethostbyname error:");
                perror(strerror(errno));
            }
            else
            {
                clients[i].clifd = socket(AF_INET, SOCK_STREAM, 0);
                memset((char *)&cli_addr, '\0', sizeof(cli_addr)); /* set serv_addr all bytes to zero*/
                cli_addr.sin_family = AF_INET;
                cli_addr.sin_addr = *((struct in_addr *)hostname->h_addr);
                cli_addr.sin_port = htons(atoi(clients[i].host_port));

                int flags = fcntl(clients[i].clifd, F_GETFL, 0);
                fcntl(clients[i].clifd, F_SETFL, flags | O_NONBLOCK);

                if (connect(clients[i].clifd, (struct sockaddr *)&cli_addr, sizeof(cli_addr)) < 0)
                {
                    /* call connect on non-blocking mode,
                     * get EINPROGRESS instead of blocking waiting for the connection handshake to complete
                     */
                    if (errno != EINPROGRESS)
                    {
                        fprintf(stderr, "connect error:");
                        perror(strerror(errno));
                        clients[i].is_active = 0;
                    }
                }
            }

            FD_SET(clients[i].clifd, &allfds); /* add clients[i].clifd in allfds set */
            if (clients[i].clifd > maxfdnum)
            {
                maxfdnum = clients[i].clifd;
            }
        }
    }

    for (;;)
    {
        readfds = allfds; /* select will change fd set, copy it */

        if (select(maxfdnum + 1, &readfds, NULL, NULL, NULL) < 0)
        {
            fprintf(stderr, "select error:");
            perror(strerror(errno));
            exit(1);
        }

        for (i = 0; i < MAX_CLIENTS; i++)
        {
            char buffer[BUFFER_LEN];

            /* If *cmdline is NULL,
             * then getline() will allocate a buffer for storing the line,
             *  which should be freed by the user program,
             *  otherwise getline() may core dump */
            char *cmdline = NULL;
            int cmdlen = 0;

            if (clients[i].is_active)
            {
                if (FD_ISSET(clients[i].clifd, &readfds))
                {
                    int error;
                    socklen_t n = sizeof(error);
                    if (getsockopt(clients[i].clifd, SOL_SOCKET, SO_ERROR, &error, &n) < 0 || error != 0)
                    {
                        /* Non-blocking failed */
                        fprintf(stderr, "Non-blocking failed");
                        perror(strerror(errno));
                        continue;
                    }

                    int socketlen = read(clients[i].clifd, buffer, BUFFER_LEN);
                    if (socketlen > 0)
                    {
                        buffer[socketlen] = '\0';
                        script_content(buffer, i, 0);

                        if (prompt_check(buffer))
                        {
                            getline(&cmdline, &cmdlen, clients[i].batch_fp);
                            int w = write(clients[i].clifd, cmdline, strlen(cmdline));
                            while(w < 0)
                            {
                                sleep(1);
                                w = write(clients[i].clifd, cmdline, strlen(cmdline));
                            }

                            script_content(cmdline, i, 1);
                            free(cmdline);
                        }
                    }
                    else
                    {
                        FD_CLR(clients[i].clifd, &allfds);
                        close(clients[i].clifd);
                        clients[i].is_active = 0;
                    }
                }
                fflush(stdout); /* flush output */
            }
        }

        int running = 0;
        for (i = 0; i < MAX_CLIENTS; i++)
        {
            running = running | clients[i].is_active;
        }
        if (!running)
        {
            break;
        }
    }

    return 0;
}

int script_content(char *buffer, int id, int bold)
{
    printf("<script>document.getElementById(\"m%d\").innerHTML +=\"%s", id, bold ? "<b>" : "");

    int i;
    for (i = 0; buffer[i] != '\0'; i++)
    {
        switch (buffer[i])
        {
            case '<':
                printf("&lt");
                break;
            case '>':
                printf("&gt");
                break;
            case ' ':
                printf("&nbsp;");
                break;
            case '\r':
                break;
            case '\n':
                printf("<br>");
                break;
            case '\\':
                printf("&#039");
                break;
            case '\"':
                printf("&quot;");
                break;
            case '[':
                if (!COLOR_WELCOME_MSG)
                {
                    printf("%c", buffer[i]);
                    break;
                }

                if (strncmp(buffer + i, "[31m", 3) == 0)
                {
                    printf("<font color=\\\"red\\\">");
                }
                else if (strncmp(buffer + i, "[32m", 3) == 0)
                {
                    printf("<font color=\\\"green\\\">");
                }
                else if (strncmp(buffer + i, "[33m", 3) == 0)
                {
                    printf("<font color=\\\"yellow\\\">");
                }
                else if (strncmp(buffer + i, "[34m", 3) == 0)
                {
                    printf("<font color=\\\"blue\\\">");
                }
                else if (strncmp(buffer + i, "[35m", 3) == 0)
                {
                    printf("<font color=\\\"magenta\\\">");
                }
                else if (strncmp(buffer + i, "[36m", 3) == 0)
                {
                    printf("<font color=\\\"cyan\\\">");
                }
                else if (strncmp(buffer + i, "[0m", 2) == 0)
                {
                    printf("</font>");
                    i += 2;
                    break;
                }
                else
                {
                    printf("%c", buffer[i]);
                    break;
                }
                i += 3;
                break;
            default:
                printf("%c", buffer[i]);
        }
    }

    printf("%s\";</script>", bold ? "</b>" : "");
    return 0;
}

int prompt_check(char *buffer)
{
    int i;
    for (i = 0; i < strlen(buffer) - 1; i++)
    {
        if (buffer[i] == '%' && buffer[i + 1] == ' ')
        {
            return 1;
        }
    }

    return 0;
}
