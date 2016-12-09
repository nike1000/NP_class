#include "http.h"

/* Content-Type support in this http server */
ContentType extensions[] = {
    {"txt", "text/html"},
    {"cgi", "text/cgi"},
    {"htm", "text/html"},
    {"html", "text/html"},
    {"png", "image/png"},
    {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"ogg", "audio/ogg"},
    {"mp4", "video/mp4"},
    {0, 0},
};

int main()
{
    chdir("../public_html/");
    int clifd = startServer();
    Header request = parseRequest(clifd);
    checkResource(request, clifd);
    handleRequest(request, clifd);
}

/*
 * run socket, bind, listen, accept
 * return client file discriptor
 * */
int startServer()
{
    int sockfd;
    struct sockaddr_in serv_addr, cli_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0)
    {
        perror(strerror(errno));
        exit(1);
    }

    memset((char *)&serv_addr, '\0', sizeof(serv_addr)); /* set serv_addr all bytes to zero*/
    serv_addr.sin_family = AF_INET;                      /* AF_INET for IPv4 protocol */
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);       /* INADDR_ANY for listening on any IP address */
    serv_addr.sin_port = htons(SERV_TCP_PORT);           /* Server port number */

    int yes = 1;
    /* lose the pesky "Address already in use" error message */
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
        perror(strerror(errno));
        exit(1);
    }

    /* -1 if bind fail, 0 if success */
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) < 0)
    {
        perror(strerror(errno));
        exit(1);
    }

    if (listen(sockfd, MAXCONN) < 0) /* -1 if listen fail, 0 if success */
    {
        perror(strerror(errno));
        exit(1);
    }

    for (;;)
    {
        socklen_t clilen = sizeof(cli_addr);
        int clifd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen); /* accept client connection */
        if (clifd < 0)
        {
            perror(strerror(errno));
            exit(1);
        }

        int fork_pid = fork();
        if (fork_pid < 0)
        {
            perror(strerror(errno));
            exit(1);
        }
        else if (fork_pid > 0)
        {
            close(clifd);
            if (waitpid(fork_pid, NULL, 0) != fork_pid)
            {
                perror(strerror(errno));
                exit(1);
            }
        }
        else
        {
            int fork_pid2 = fork();
            if (fork_pid2 < 0)
            {
                perror(strerror(errno));
                exit(1);
            }
            else if (fork_pid2 > 0)
            {
                exit(0);
            }
            else
            {
                close(sockfd);
                return clifd;
            }
        }
    }
}

/*
 * Parse the first line of HTTP Header
 * e.g. GET /index.html?a=xx HTTP/1.1
 * return Header struct, include method, path, query_string, protocol
 * */
Header parseRequest(int clifd)
{
    Header request;
    char buffer[MAX_HEADER_LEN];
    memset((char *)buffer, '\0', MAX_HEADER_LEN);

    int len;
    if ((len = read(clifd, buffer, MAX_HEADER_LEN)) > 0)
    {
        char tmp[MAX_HEADER_LEN];
        strcpy(tmp, strtok(buffer, "\r\n"));

        strcpy(request.method, strtok(buffer, " "));
        if (strchr(tmp, '?') == NULL) /* string contain ? or not */
        {
            strcpy(request.path, strtok(NULL, " ") + 1); /* +1 to remove / at the beginning of path */
            strcpy(request.query_string, "\0");
        }
        else
        {
            strcpy(request.path, strtok(NULL, "?") + 1);
            strcpy(request.query_string, strtok(NULL, " "));
        }
        strcpy(request.protocol, strtok(NULL, "\r\n"));
    }

    return request;
}

/*
 *  check request resource exist and readable or not
 *  404, 403, 200
 * */
int checkResource(Header request, int clifd)
{
    char buffer[MAX_HEADER_LEN];
    if (access(request.path, F_OK) < 0)
    {
        // 404
        sprintf(buffer, "%s 404 NOT FOUND\n", request.protocol);
        sprintf(buffer, "%sContent-Type: text/html\n\n", buffer);
        sprintf(buffer, "%s<h1>404 Not Found</h1>\n", buffer);
        write(clifd, buffer, strlen(buffer));
        exit(0);
    }
    else if (access(request.path, R_OK) < 0)
    {
        // 403
        sprintf(buffer, "%s 403 Forbidden\n", request.protocol);
        sprintf(buffer, "%sContent-Type: text/html\n\n", buffer);
        sprintf(buffer, "%s<h1>403 Forbidden</h1>\n", buffer);
        write(clifd, buffer, strlen(buffer));
        exit(0);
    }
    else
    {
        // 200
    }
}

/*
 * run cgi, or read resource to response
 * */
int handleRequest(Header request, int clifd)
{
    char *type;
    if (strcmp(request.method, "GET") == 0)
    {
        char buffer[MAX_HEADER_LEN];
        sprintf(buffer, "%s 200 OK\n", request.protocol);

        if (type = getContentType(request.path))
        {

            if (strcmp(type, "text/cgi") == 0)
            {
                write(clifd, buffer, strlen(buffer));

                int fork_pid = fork();
                if (fork_pid < 0)
                {
                    perror(strerror(errno));
                    exit(1);
                }
                else if (fork_pid > 0)
                {
                    close(clifd);
                    if (waitpid(fork_pid, NULL, 0) != fork_pid)
                    {
                        perror(strerror(errno));
                        exit(1);
                    }
                }
                else
                {
                    setHttpEnv(request);
                    if (dup2(clifd, STDIN_FILENO) < 0)
                    {
                        perror(strerror(errno));
                    }

                    if (dup2(clifd, STDOUT_FILENO) < 0)
                    {
                        perror(strerror(errno));
                    }

                    if (execl(request.path, request.path, NULL) < 0)
                    {
                        perror(strerror(errno));
                    }
                }
            }
            else
            {
                sprintf(buffer, "%sContent-Type: %s\n\n", buffer, type);
                write(clifd, buffer, strlen(buffer));

                char read_buffer[1024];
                int read_len;
                int readfd = open(request.path, O_RDONLY);
                while ((read_len = read(readfd, read_buffer, 1024)) > 0)
                {
                    write(clifd, read_buffer, read_len);
                }

                close(readfd);
            }
        }
        else
        {
            // extension not support
            sprintf(buffer, "%sContent-Type: text/html\n\n", buffer);
            sprintf(buffer, "%sFile Extension Not Support.\n", buffer);
            write(clifd, buffer, strlen(buffer));
        }
    }
    else
    {
        // Only Support GET method
    }
}

int setHttpEnv(Header request)
{
    setenv("REQUEST_METHOD", request.method, 1);
    setenv("SCRIPT_NAME", request.path, 1);
    setenv("QUERY_STRING", request.query_string, 1);
    setenv("CONTENT_LENGTH", " ", 1);
    setenv("REMOTE_HOST", " ", 1);
    setenv("REMOTE_ADDR", " ", 1);
    setenv("ANTH_TYPE", " ", 1);
    setenv("AUTH_TYPE", " ", 1);
    setenv("REMOTE_USER", " ", 1);
    setenv("REMOTE_IDENT", " ", 1);
}

/*
 * According to the extension, return its ContentType
 * */
char *getContentType(char *path)
{
    int i, extlen, pathlen;
    pathlen = strlen(path);

    for (i = 0; extensions[i].extension; i++)
    {
        extlen = strlen(extensions[i].extension);
        if (strncmp(&path[pathlen - extlen], extensions[i].extension, extlen) == 0)
        {
            return extensions[i].type;
        }
    }

    return NULL;
}
