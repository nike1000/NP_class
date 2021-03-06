#include "socksServer.h"

Connection cnt; /* socks connection info */
FirewallRule rules[MAX_RULE];

int main()
{
    int rulesum = getFirewallRules();
    int clifd = startSerever();
    Socks4Packet pkt = handleSocksRequest(clifd);
    printf("\nVN: %u, CD: %u, DST IP: %s, DST PORT: %s, USERID: %x\n", pkt.vn, pkt.cd, cnt.dstip, cnt.dstport, pkt.userid);

    if (firewallAccessCheck(rulesum)) /* Permit, Pass FireWall */
    {
        printf("Permit Src = %s(%s), Dst = %s(%s)\n", cnt.srcip, cnt.srcport, cnt.dstip, cnt.dstport);
        if (pkt.cd == 1) /* SOCKS CONNECT MODE */
        {
            int serfd = connectTCP(cnt.dstip, cnt.dstport);
            if (serfd == -1)
            {
                /* TODO: send socks reply, request fail */
                printf("SOCKS_CONNECT FAIL...\n");
                socksConnectReply(pkt, 91, clifd);
                close(clifd);
            }
            else
            {
                /* TODO: send socks reply, request grant
                 *       redirect data */
                printf("SOCKS_CONNECT GRANTED...\n");
                socksConnectReply(pkt, 90, clifd);
                doRedirect(clifd, serfd);
            }
        }
        else if (pkt.cd == 2) /* SOCKS BIND MODE */
        {
            int port;
            int serfd, bindfd;
            struct sockaddr_in ser_addr;
            socklen_t serlen = sizeof(ser_addr);

            bindfd = passiveTCP();

            if (getsockname(bindfd, (struct sockaddr *)&ser_addr, &serlen) == -1)
            {
                fprintf(stderr, "getsocketname error:");
                perror(strerror(errno));
                return -1;
            }
            else
            {
                port = ntohs(ser_addr.sin_port);
                printf("Bind Port: %d\n", ntohs(ser_addr.sin_port));
            }

            if (bindfd == -1)
            {
                printf("SOCKS_BIND FAIL...\n");
                socksBindReply(port, 91, clifd);
                close(clifd);
            }
            else
            {
                printf("SOCKS_BIND GRANTED...\n");
                socksBindReply(port, 90, clifd);

                if ((serfd = accept(bindfd, (struct sockaddr *)&ser_addr, &serlen)) < 0)
                {
                    fprintf(stderr, "bind accept error\n");
                    perror(strerror(errno));
                    exit(1);
                }

                socksBindReply(port, 90, clifd);
                doRedirect(clifd, serfd);
            }
        }
        else
        {
            /* TODO: send socks reply, request fail */
            printf("Socks mode refused\n");
            socksConnectReply(pkt, 91, clifd);
            close(clifd);
        }
    }
    else
    {
        /* TODO: send socks reply, request fail */
        printf("Deny Src = %s(%s), Dst = %s(%s)\n", cnt.srcip, cnt.srcport, cnt.dstip, cnt.dstport);
        socksConnectReply(pkt, 91, clifd);
        close(clifd);
    }

    exit(0);
}

int getFirewallRules()
{
    FILE *conf_fp;
    if ((conf_fp = fopen(FIREWALL_CONF, "r")) == NULL)
    {
        fprintf(stderr, "fopen error:");
        perror(strerror(errno));
        return 0;
    }
    else
    {
        char *rule = NULL;
        int len = 0;
        int i = 0;

        while (getline(&rule, &len, conf_fp) > 0)
        {
            char *delim = " /\r\n";
            char *tmp, *last = NULL;
            struct in_addr addr;

            if (strlen(rule) < 6 || strcmp(strtok_r(rule, delim, &last), "permit") != 0) /* if rule not start with permit,then ignore. strlen use to handle empty line */
            {
                continue;
            }

            strcpy(rules[i].mode, strtok_r(NULL, delim, &last)); /* socks mode: c or b */

            tmp = strtok_r(NULL, delim, &last); /* src ip */
            inet_aton(tmp, &addr);
            if (strcmp(tmp, "-") == 0) /* permit all src ip */
            {
                rules[i].srcmask = 0x00000000; /* src mask */
            }
            else
            {
                rules[i].srcmask = 0xffffffff << (32 - atoi(strtok_r(NULL, delim, &last))); /* get CIDR submask */
            }

            rules[i].srcip = ntohl(addr.s_addr) & rules[i].srcmask;
            strcpy(rules[i].srcport, strtok_r(NULL, delim, &last)); /* src port */

            tmp = strtok_r(NULL, delim, &last);
            inet_aton(tmp, &addr);

            if (strcmp(tmp, "-") == 0) /* permit all dst ip */
            {
                rules[i].dstmask = 0x00000000; /* dst mask */
            }
            else
            {
                rules[i].dstmask = 0xffffffff << (32 - atoi(strtok_r(NULL, delim, &last))); /* get CIDR submask */
            }

            rules[i].dstip = ntohl(addr.s_addr) & rules[i].dstmask;
            strcpy(rules[i].dstport, strtok_r(NULL, delim, &last)); /* dst port */
            printf("RULE: %s %x %s %x %s\n", rules[i].mode, rules[i].srcip, rules[i].srcport, rules[i].dstip, rules[i].dstport);
            i++;

            if (i >= MAX_RULE)
            {
                break;
            }
        }
        return i;
    }
}

int startSerever()
{
    int sockfd;
    struct sockaddr_in serv_addr, cli_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
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
        else if (fork_pid > 0) /* Parents */
        {
            close(clifd);
            if (waitpid(fork_pid, NULL, 0) != fork_pid)
            {
                perror(strerror(errno));
                exit(1);
            }
        }
        else /* Child */
        {
            int fork_pid2 = fork();
            if (fork_pid2 < 0)
            {
                perror(strerror(errno));
                exit(1);
            }
            else if (fork_pid2 > 0) /* Parents */
            {
                exit(0);
            }
            else /* Child */
            {
                close(sockfd);
                sprintf(cnt.srcip, inet_ntoa(cli_addr.sin_addr));
                sprintf(cnt.srcport, "%u", ntohs(cli_addr.sin_port));
                return clifd;
            }
        }
    }
}

Socks4Packet handleSocksRequest(int clifd)
{
    Socks4Packet pkt; /* struct to store socks column */
    unsigned char socks4_request[MAX_SOCKS_LEN];
    memset((char *)socks4_request, '\0', MAX_SOCKS_LEN);

    int len;
    if ((len = read(clifd, socks4_request, MAX_SOCKS_LEN)) > 0)
    {
        if (socks4_request[0] != 4) /* not socks version 4 packet */
        {
            printf("Not Socks4 Request\n");
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

    if (!(pkt.dstip[0] || pkt.dstip[1] || pkt.dstip[2]) && pkt.dstip[3]) /* dst ip 0.0.0.x */
    {
        pkt.hostname = pkt.userid + strlen(pkt.userid) + 1; /* get hostname after userid */

        struct hostent *he = NULL;
        if ((he = gethostbyname((const char *)pkt.hostname)) == NULL)
        {
            fprintf(stderr, "gethostbyname error:");
            perror(strerror(errno));
        }
        else
        {
            char *dstip = inet_ntoa(*((struct in_addr *)he->h_addr)); /* get IP */
            int i;
            char *iptoken;
            for (i = 0, iptoken = strtok(dstip, ".\r\n"); i <= 3 && iptoken; i++, iptoken = strtok(NULL, ".\r\n"))
            {
                pkt.dstip[i] = atoi(iptoken);
            }
        }
    }

    cnt.mode = pkt.cd == 1 ? "c" : "b";
    sprintf(cnt.dstip, "%u.%u.%u.%u", pkt.dstip[0], pkt.dstip[1], pkt.dstip[2], pkt.dstip[3]);
    sprintf(cnt.dstport, "%d", pkt.dstport[0] * 256 + pkt.dstport[1]);

    return pkt;
}

int firewallAccessCheck(int sum)
{
    struct in_addr addr;
    int permit = 0;

    inet_aton(cnt.srcip, &addr);             /* convert srcip from string format with dot to binary */
    unsigned int srcip = ntohl(addr.s_addr); /* network byte order to hosts byte order */
    inet_aton(cnt.dstip, &addr);             /* convert dstip from string format with dot to binary */
    unsigned int dstip = ntohl(addr.s_addr); /* network byte order to hosts byte order */

    int i = 0;
    while (i < sum && !permit) /* still have rule, and connection not accept yet */
    {
        if (strcmp(cnt.mode, rules[i].mode) != 0)
        {
            i++;
            continue;
        }

        /*printf("SRC IP:%x , %x\n", srcip & rules[i].srcmask, rules[i].srcip);*/
        if ((srcip & rules[i].srcmask) != rules[i].srcip)
        {
            i++;
            continue;
        }

        if (strcmp(rules[i].srcport, "-") != 0 && strcmp(cnt.srcport, rules[i].srcport) != 0)
        {
            i++;
            continue;
        }

        /*printf("DST IP:%x , %x\n", dstip & rules[i].dstmask, rules[i].dstip);*/
        if ((dstip & rules[i].dstmask) != rules[i].dstip)
        {
            i++;
            continue;
        }

        if (strcmp(rules[i].dstport, "-") != 0 && strcmp(cnt.dstport, rules[i].dstport) != 0)
        {
            i++;
            continue;
        }

        permit = 1; /* permit if match all field */
    }

    return permit;
}

int socksConnectReply(Socks4Packet request, int status, int fd)
{
    unsigned char socks4_reply[8];
    socks4_reply[0] = 0;
    socks4_reply[1] = status;
    socks4_reply[2] = request.dstport[0];
    socks4_reply[3] = request.dstport[1];
    socks4_reply[4] = request.dstip[0];
    socks4_reply[5] = request.dstip[1];
    socks4_reply[6] = request.dstip[2];
    socks4_reply[7] = request.dstip[3];
    write(fd, socks4_reply, 8);
}

int socksBindReply(int port, int status, int fd)
{
    unsigned char socks4_reply[8];
    socks4_reply[0] = 0;
    socks4_reply[1] = status;
    socks4_reply[2] = port / 256;
    socks4_reply[3] = port % 256;
    socks4_reply[4] = 0;
    socks4_reply[5] = 0;
    socks4_reply[6] = 0;
    socks4_reply[7] = 0;
    write(fd, socks4_reply, 8);
}

int connectTCP(char *dstip, char *dstport)
{
    struct sockaddr_in serv_addr;
    int serfd = socket(AF_INET, SOCK_STREAM, 0);
    memset((char *)&serv_addr, '\0', sizeof(serv_addr)); /* set serv_addr all bytes to zero*/
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(dstip);
    serv_addr.sin_port = htons(atoi(dstport));

    /*int flags = fcntl(serfd, F_GETFL, 0);*/
    /*fcntl(serfd, F_SETFL, flags | O_NONBLOCK);*/

    /*connect(serfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));*/
    /*return serfd;*/
    return connect(serfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0 ? -1 : serfd;
}

int passiveTCP()
{
    struct sockaddr_in bind_addr;
    int bindfd = socket(AF_INET, SOCK_STREAM, 0);
    memset((char *)&bind_addr, '\0', sizeof(bind_addr)); /* set bind_addr all bytes to zero*/
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = htons(0);

    int yes = 1;
    /* lose the pesky "Address already in use" error message */
    if (setsockopt(bindfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
        fprintf(stderr, "getsocketopt error\n");
        perror(strerror(errno));
        exit(1);
    }

    /* -1 if bind fail, 0 if success */
    if (bind(bindfd, (struct sockaddr *)&bind_addr, sizeof(struct sockaddr)) < 0)
    {
        fprintf(stderr, "bind error:");
        perror(strerror(errno));
        return -1;
    }

    if (listen(bindfd, MAXCONN) < 0) /* -1 if listen fail, 0 if success */
    {
        fprintf(stderr, "listen error:");
        perror(strerror(errno));
        return -1;
    }

    return bindfd;
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
            /*fprintf(stderr, "==========>CLI READ:%d\n", len);*/
            if (len < 0)
            {
                fprintf(stderr, "read error:");
                perror(strerror(errno));
            }
            else if (len == 0)
            {
                close(clifd);
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
            /*fprintf(stderr, "==========>SER READ:%d\n", len);*/
            if (len < 0)
            {
                fprintf(stderr, "read error:");
                perror(strerror(errno));
            }
            else if (len == 0)
            {
                close(clifd);
                close(serfd);
                break;
            }
            else
            {
                write(clifd, buffer, len);
            }
        }
    }
}
