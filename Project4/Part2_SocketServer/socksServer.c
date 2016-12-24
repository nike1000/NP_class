#include "socksServer.h"

Connection cnt;

int main()
{
    int clifd = startSerever();
    Socks4Packet pkt = handleSocksRequest(clifd);
    //if(firewallAccessCheck(pkt))
    if(1)
    {
        if(pkt.cd == 1) /* CONNECT MODE */
        {
            unsigned char sock4_reply[MAX_SOCKS_LEN];

            sock4_reply[0] = 0;
            sock4_reply[2] = pkt.dstport[0];
            sock4_reply[3] = pkt.dstport[1];
            sock4_reply[4] = pkt.dstip[0];
            sock4_reply[5] = pkt.dstip[1];
            sock4_reply[6] = pkt.dstip[2];
            sock4_reply[7] = pkt.dstip[3];

            int serfd = connectTCP(cnt.dstip, cnt.dstport);
            if(serfd == -1)
            {
                /* TODO: send socks reply, request fail */
                sock4_reply[1] = 91;
                write(clifd, sock4_reply, strlen(sock4_reply));
                close(clifd);
            }
            else
            {
                /* TODO: send socks reply, request grant
                 *       redirect data */
                sock4_reply[1] = 90;
                fprintf(stderr, "==>REPLY: [%d][%d][%d][%d][%d][%d][%d][%d]\n", sock4_reply[0]&0xFF,sock4_reply[1],sock4_reply[2],sock4_reply[3],sock4_reply[4],sock4_reply[5],sock4_reply[6],sock4_reply[7]);
                write(clifd, sock4_reply, 8);
                doRedirect(clifd ,serfd);
            }
            exit(0);
        }
        else if(pkt.cd == 2) /* BIND MODE */
        {

        }
        else
        {
            /* TODO: send socks reply, request fail */
        }

    }
    else
    {
        /* TODO: send socks reply, request fail */
    }
}

int startSerever()
{
    int sockfd;
    struct sockaddr_in serv_addr, cli_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        fprintf(stderr, "socket error\n");
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
        fprintf(stderr, "getsocketopt error\n");
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
        fprintf(stderr, "listen error\n");
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

Socks4Packet handleSocksRequest(int clifd)
{
    Socks4Packet pkt;
    char dstip[16];
    unsigned char socks4_request[MAX_SOCKS_LEN];
    memset((char *)socks4_request, '\0', MAX_SOCKS_LEN);

    int len;
    if ((len = read(clifd, socks4_request, MAX_SOCKS_LEN)) > 0)
    {
        fprintf(stderr, "PKT: [%d][%d][%d][%d][%d][%d][%d][%d]\n", socks4_request[0]&0xFF,socks4_request[1],socks4_request[2],socks4_request[3],socks4_request[4],socks4_request[5],socks4_request[6],socks4_request[7]);
        if(socks4_request[0] != 4)
        {
            exit(0);
        }
        pkt.vn = socks4_request[0];
        pkt.cd = socks4_request[1];
        pkt.dstport[0] = socks4_request[2];
        pkt.dstport[1] = socks4_request[3];
        pkt.dstip[0] = socks4_request[4];
        pkt.dstip[1] = socks4_request[5];
        pkt.dstip[2] = socks4_request[6];
        pkt.dstip[3] = socks4_request[7];
        pkt.userid = socks4_request + 8;
    }

    if(!(pkt.dstip[0] || pkt.dstip[1] || pkt.dstip[2]) && pkt.dstip[3])
    {
            pkt.hostname = pkt.userid + strlen(pkt.userid) + 1;

            struct hostent *he = NULL;
            if ((he = gethostbyname((const char*)pkt.hostname)) == NULL)
            {
                fprintf(stderr, "gethostbyname error:");
                perror(strerror(errno));
            }
            else
            {
                char *dstip = inet_ntoa(*((struct in_addr *)he->h_addr));
                int i;
                char *iptoken;
                for(i = 0, iptoken = strtok(dstip, ".\r\n"); i <= 3 && iptoken; i++, iptoken = strtok(NULL, ".\r\n"))
                {
                    pkt.dstip[i] = atoi(iptoken);
                }
            }
    }

    sprintf(cnt.dstip, "%u.%u.%u.%u", pkt.dstip[0], pkt.dstip[1], pkt.dstip[2], pkt.dstip[3]);
    sprintf(cnt.dstport, "%d", pkt.dstport[0]*256+pkt.dstport[1]);
    fprintf(stderr, "DST:%s [%s]\n", cnt.dstip, cnt.dstport);

    return pkt;
}

int firewallAccessCheck(Socks4Packet pkt)
{

}

int connectTCP(char* dstip, char* dstport)
{
    struct sockaddr_in ser_addr;
    int serfd = socket(AF_INET, SOCK_STREAM, 0);
    memset((char *)&ser_addr, '\0', sizeof(ser_addr)); /* set serv_addr all bytes to zero*/
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_addr.s_addr = inet_addr(dstip);
    ser_addr.sin_port = htons(atoi(dstport));

    return connect(serfd, (struct sockaddr *)&ser_addr, sizeof(ser_addr)) < 0 ? -1 : serfd;
}

int doRedirect(int clifd, int serfd)
{
    fd_set readfds, allfds;
    int maxfdnum = clifd > serfd ? clifd : serfd;

    FD_ZERO(&allfds);
    FD_ZERO(&readfds);

    FD_SET(clifd, &allfds);
    FD_SET(serfd, &allfds);

    for (;;)
    {
        char buffer[2048] = {0};
        int len = 0;
        readfds = allfds; /* select will change fd set, copy it */

        if (select(maxfdnum + 1, &readfds, NULL, NULL, NULL) < 0)
        {
            fprintf(stderr, "select error:");
            perror(strerror(errno));
            exit(1);
        }

        if (FD_ISSET(clifd, &readfds))
        {
            len = read(clifd, buffer, BUFFER_LEN);
            fprintf(stderr, "==========>CLI READ\n");
            if(len < 0)
            {
                fprintf(stderr, "read error:");
                perror(strerror(errno));
            }
            else if(len == 0)
            {
                close(serfd);
                break;
            }
            else
            {
                write(serfd, buffer, len);
            }
        }

        if (FD_ISSET(serfd, &readfds))
        {
            len = read(serfd, buffer, BUFFER_LEN);
            fprintf(stderr, "==========>SER READ:%d\n", len);
            if(len < 0)
            {
                fprintf(stderr, "read error:");
                perror(strerror(errno));
            }
            else if(len == 0)
            {
                close(clifd);
                break;
            }
            else
            {
                write(clifd, buffer, len);
            }
        }
    }
}
