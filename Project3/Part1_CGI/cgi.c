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

typedef struct ClientInfo
{
    int id;
    int clifd;
    char host_ip[64];
    char host_port[6];
    char batch_file[32];
    int is_active;
} ClientInfo;

ClientInfo clients[5];

int main()
{
    parseQueryString();
    responseHTML();
    connectServer();
    printf("</body>\n");
    printf("</html>\n");
}

int parseQueryString()
{
    char *query_string = getenv("QUERY_STRING");

    if (query_string != NULL)
    {
        int i = 0;
        for (i = 0; i < MAX_CLIENTS; i++)
        {
            clients[i].id = i;
            strcpy(clients[i].host_ip, strtok((i == 0 ? query_string : NULL), "&") + 3);
            strcpy(clients[i].host_port, strtok(NULL, "&") + 3);
            strcpy(clients[i].batch_file, strtok(NULL, "&") + 3);

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
}

int responseHTML()
{
    printf("Content-Type: text/html\n\n");
    printf("<html>\n");
    printf("<head>\n");
    printf("<title>Network Programming Homework 3</title>\n");
    printf("</head>\n");
    printf("<body bgcolor=\"#336699\">\n");
    printf("<table width=\"100%%\" border=\"1\">\n");
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
            printf("<td id=\"m%d\"></td>\n", i);
        }
    }
    printf("</tr>\n");
    printf("</table>\n");
}

int connectServer()
{
    fd_set readfds, allfds;
    int maxfdnum;

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
                perror(strerror(errno));
            }
            else
            {
                clients[i].clifd = socket(AF_INET, SOCK_STREAM, 0);
                memset((char *)&cli_addr, '\0', sizeof(cli_addr)); /* set serv_addr all bytes to zero*/
                cli_addr.sin_family = AF_INET;
                memcpy(&cli_addr.sin_addr, hostname->h_addr_list[0], hostname->h_length);
                /*cli_addr.sin_addr = *((struct in_addr *)hostname->h_addr);*/
                cli_addr.sin_port = htons(atoi(clients[i].host_port));

                int flags = fcntl(clients[i].clifd, F_GETFL, 0);
                fcntl(clients[i].clifd, F_SETFL, flags | O_NONBLOCK);
            }

            if (connect(clients[i].clifd, (struct sockaddr *)&cli_addr, sizeof(cli_addr)) < 0)
            {
                /* call connect on non-blocking mode,
                 * get EINPROGRESS instead of blocking waiting for the connection handshake to complete
                 */
                if (errno != EINPROGRESS)
                {
                    perror(strerror(errno));
                    clients[i].is_active = 0;
                }
                else
                {
                    FD_SET(clients[i].clifd, &allfds); /* addclients[i].clifd in allfds set */
                    if (clients[i].clifd > maxfdnum)
                    {
                        maxfdnum = clients[i].clifd;
                    }
                }
            }
            else
            {
                FD_SET(clients[i].clifd, &allfds); /* addclients[i].clifd in allfds set */
                if (clients[i].clifd > maxfdnum)
                {
                    maxfdnum = clients[i].clifd;
                }
            }
        }
    }

    for (;;)
    {
        readfds = allfds; /* select will change fd set, copy it */

        if (select(maxfdnum + 1, &readfds, NULL, NULL, NULL) < 0)
        {
            perror(strerror(errno));
            exit(1);
        }

        for (i = 0; i < MAX_CLIENTS; i++)
        {
            char buffer[BUFFER_LEN];
            if (clients[i].is_active)
            {
                if (FD_ISSET(clients[i].clifd, &readfds))
                {
                    int bytes_read = read(clients[i].clifd, buffer, BUFFER_LEN);
                    script_content(buffer, i);
                    clients[i].is_active = 0;
                }
            }
        }

        int running = 0;
        for (i = 0; i < MAX_CLIENTS; i++)
        {
            running = running | clients[i].is_active;
        }
        if (!running)
            break;
    }
}

int script_content(char *buffer, int id)
{
    printf("<script>document.getElementById(\"m%d\").innerHTML +=\"", id);

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
            case '\n':
                printf("<br>");
                break;
            case '\\':
                printf("&#039");
                break;
            case '\"':
                printf("&quot;");
                break;
            default:
                printf("%c", buffer[i]);
        }
    }

    printf("\";</script>");
}
