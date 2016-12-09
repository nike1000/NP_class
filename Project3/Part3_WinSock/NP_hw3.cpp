#include <windows.h>
#include <list>
using namespace std;

#include "resource.h"

#include <io.h>
#include <errno.h>                                                                   
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <map>

#define SERVER_PORT 7799
#define WM_SOCKET_NOTIFY (WM_USER + 1)
#define MAXCONN 10
#define MAX_CLIENTS 5
#define MAX_HEADER_LEN 8192
#define CGI_SOCKET_NOTIFY 8000
#define BUFFER_LEN 4096
#define COLOR_WELCOME_MSG 1

typedef struct Header
{
    char method[8];
    char path[256];
    char query_string[512];
    char protocol[16];
} Header;

typedef struct ClientInfo
{
    int id;
    SOCKET clifd;
    char host_ip[64];
    char host_port[6];
    char batch_file[32];
    int is_active;
    FILE *batch_fp;
} ClientInfo;

typedef struct WebConn
{
    SOCKET websocket;
    ClientInfo clients[MAX_CLIENTS];
}WebConn;

BOOL CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);
int EditPrintf (HWND, TCHAR *, ...);
//=================================================================
//  Global Variables
//=================================================================
list<SOCKET> Socks;
WebConn web[MAXCONN];
map<SOCKET, int> web_clifd_map;
map<SOCKET, int>::iterator iter;

int init()
{
    for (int i = 0; i < MAXCONN; i++)
    {
        web[i].websocket = NULL;
        for (int j = 0; j < MAX_CLIENTS; j++)
        {
            web[i].clients[j] = { -1, NULL, NULL, NULL, NULL, 0, NULL };
        }
    }

    web_clifd_map.erase(web_clifd_map.begin(), web_clifd_map.end());
    return 0;
}

Header parseRequest(SOCKET websocket)
{
    Header request;
    char buffer[MAX_HEADER_LEN];
    memset((char *)buffer, '\0', MAX_HEADER_LEN);

    int len;
    if ((len = recv(websocket, buffer, MAX_HEADER_LEN, 0)) > 0)
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


int parseQueryString(int index, char *query_string)
{
    /* QUERY_STRING: h1=npbsd3.cs.nctu.edu.tw&p1=6085&f1=t1.txt&h2=&p2=&f2=&h3=&p3=&f3=&h4=&p4=&f4=&h5=&p5=&f5= */
    
    if (query_string != NULL)
    {
        int i = 0;
        for (i = 0; i < MAX_CLIENTS; i++)
        {
            web[index].clients[i].id = i;
            /* +3 to remove h1= p1= f1= ... */
            strcpy(web[index].clients[i].host_ip, strtok((i == 0 ? query_string : NULL), "&") + 3);
            strcpy(web[index].clients[i].host_port, strtok(NULL, "&") + 3);
            strcpy(web[index].clients[i].batch_file, strtok(NULL, "&") + 3);

            /* active only if host, port, file all complete */
            if (strlen(web[index].clients[i].host_ip) && strlen(web[index].clients[i].host_port) && strlen(web[index].clients[i].batch_file))
            {
                web[index].clients[i].is_active = 1;
            }
            else
            {
                web[index].clients[i].is_active = 0;
            }
        }
    }
    return 0;
}

int responseHTML(int index, SOCKET websocket)
{
    send(websocket, "Content-Type: text/html\n\n", strlen("Content-Type: text/html\n\n") , 0);
    send(websocket, "<html>\n", strlen("<html>\n"), 0);
    send(websocket, "<head>\n", strlen("<head>\n"), 0);
    send(websocket, "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=big5\" />\n", strlen("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=big5\" />\n"), 0);
    send(websocket, "<title>Network Programming Homework 3</title>\n", strlen("<title>Network Programming Homework 3</title>\n"), 0);
    send(websocket, "</head>\n", strlen("</head>\n"), 0);
    send(websocket, "<body bgcolor=\"#336699\">\n", strlen("<body bgcolor=\"#336699\">\n"), 0);
    send(websocket, "<font face=\"Courier New\" size=2 color=#FFFF99>", strlen("<font face=\"Courier New\" size=2 color=#FFFF99>"), 0);
    send(websocket, "<table width=\"800\" border=\"1\">\n", strlen("<table width=\"800\" border=\"1\">\n"), 0);
    send(websocket, "<tr>\n", strlen("<tr>\n"), 0);

    int i;
    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (web[index].clients[i].is_active)
        {
            char buffer[64];
            sprintf(buffer, "<th>%s</th>\n", web[index].clients[i].host_ip);
            send(websocket, buffer, strlen(buffer), 0);
        }
    }
    send(websocket, "</tr>\n", strlen("</tr>\n"), 0);

    send(websocket, "<tr>\n", strlen("<tr>\n"), 0);

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (web[index].clients[i].is_active)
        {
            char buffer[64];
            sprintf(buffer, "<td valign=\"top\" id=\"m%d\"></td>\n", i);
            send(websocket, buffer, strlen(buffer), 0);
        }
    }
    send(websocket, "</tr>\n", strlen("</tr>\n"), 0);
    send(websocket, "</table>\n", strlen("</table>\n"), 0);
    send(websocket, "</body>\n", strlen("</body>\n"), 0);
    send(websocket, "</html>\n", strlen("</html>\n"), 0);
    return 0;
}

int openBatchFile(int index)
{
    int i;
    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (web[index].clients[i].is_active)
        {
            /* inactive client if batch file can't read */
            if ((web[index].clients[i].batch_fp = fopen(web[index].clients[i].batch_file, "r")) == NULL) {
                fprintf(stderr, "fopen error:");
                perror(strerror(errno));
                web[index].clients[i].is_active = 0;
            }
        }
    }
    return 0;
}


int connectServer(int index, SOCKET websocket)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (web[index].clients[i].is_active)
        {
            struct hostent *hostname;
            struct sockaddr_in cli_addr;

            if ((hostname = gethostbyname(web[index].clients[i].host_ip)) == NULL)
            {
                fprintf(stderr, "gethostbyname error:");
                perror(strerror(errno));
            }
            else
            {
                web[index].clients[i].clifd = socket(AF_INET, SOCK_STREAM, 0);
                memset((char *)&cli_addr, '\0', sizeof(cli_addr)); /* set serv_addr all bytes to zero*/
                cli_addr.sin_family = AF_INET;
                cli_addr.sin_addr = *((struct in_addr *)hostname->h_addr);
                cli_addr.sin_port = htons(atoi(web[index].clients[i].host_port));

                u_long mode = 1;
                ioctlsocket(web[index].clients[i].clifd, FIONBIO, &mode);
            }

            connect(web[index].clients[i].clifd, (struct sockaddr *)&cli_addr, sizeof(cli_addr));
            web_clifd_map[web[index].clients[i].clifd] = index;
            
        }
    }
    return 0;
}


int script_content(char *buffer, int id, int bold, SOCKET websocket)
{
    char tmp[BUFFER_LEN];
    sprintf(tmp, "<script>document.getElementById(\"m%d\").innerHTML +=\"%s", id, bold ? "<b>" : "");
    send(websocket, tmp ,strlen(tmp) , 0);
    //OutputDebugString(buffer);

    int i;
    for (i = 0; buffer[i] != '\0'; i++)
    {
        switch (buffer[i])
        {
        case '<':
            send(websocket, "&lt", strlen("&lt"), 0);
            break;
        case '>':
            send(websocket, "&gt", strlen("&gt"), 0);
            break;
        case ' ':
            send(websocket, "&nbsp;", strlen("&nbsp;"), 0);
            break;
        case '\r':
            break;
        case '\n':
            send(websocket, "<br>", strlen("<br>"), 0);
            break;
        case '\\':
            send(websocket, "&#039", strlen("&#039"), 0);
            break;
        case '\"':
            send(websocket, "&quot;", strlen("&quot;"), 0);
            printf("&quot;");
            break;
        case '[':
            if (!COLOR_WELCOME_MSG)
            {
                send(websocket, buffer+i, 1 , 0);
                break;
            }

            if (strncmp(buffer + i, "[31m", 3) == 0)
            {
                send(websocket, "<font color=\\\"red\\\">", strlen("<font color=\\\"red\\\">"), 0);
            }
            else if (strncmp(buffer + i, "[32m", 3) == 0)
            {
                send(websocket, "<font color=\\\"green\\\">", strlen("<font color=\\\"green\\\">"), 0);
            }
            else if (strncmp(buffer + i, "[33m", 3) == 0)
            {
                send(websocket, "<font color=\\\"yellow\\\">", strlen("<font color=\\\"yellow\\\">"), 0);
            }
            else if (strncmp(buffer + i, "[34m", 3) == 0)
            {
                send(websocket, "<font color=\\\"bule\\\">", strlen("<font color=\\\"blue\\\">"), 0);
            }
            else if (strncmp(buffer + i, "[35m", 3) == 0)
            {
                send(websocket, "<font color=\\\"magenta\\\">", strlen("<font color=\\\"magenta\\\">"), 0);
            }
            else if (strncmp(buffer + i, "[36m", 3) == 0)
            {
                send(websocket, "<font color=\\\"cyan\\\">", strlen("<font color=\\\"cyan\\\">"), 0);
            }
            else if (strncmp(buffer + i, "[0m", 2) == 0)
            {
                send(websocket, "</font>", strlen("</font>"), 0);
                i += 2;
                break;
            }
            else
            {
                send(websocket, buffer + i, 1, 0);
                break;
            }
            i += 3;
            break;
        default:
            send(websocket, buffer + i, 1, 0);
        }
    }

    sprintf(tmp, "%s\";</script>", bold ? "</b>" : "");
    send(websocket, tmp, strlen(tmp), 0);
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

int check_end(int index)
{
    int running = 0;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        running = running | web[index].clients[i].is_active;
    }

    if (!running)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow)
{
    init();
    return DialogBox(hInstance, MAKEINTRESOURCE(ID_MAIN), NULL, MainDlgProc);
}

BOOL CALLBACK MainDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    WSADATA wsaData;

    static HWND hwndEdit;
    static SOCKET msock, ssock;
    static struct sockaddr_in sa;

    int err;

    int index = -1;
    int first_null = MAXCONN;

    switch(Message) 
    {
        case WM_INITDIALOG:
            hwndEdit = GetDlgItem(hwnd, IDC_RESULT);
            break;
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case ID_LISTEN:

                    WSAStartup(MAKEWORD(2, 0), &wsaData);

                    //create master socket
                    msock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                    
                    if( msock == INVALID_SOCKET ) {
                        EditPrintf(hwndEdit, TEXT("=== Error: create socket error ===\r\n"));
                        WSACleanup();
                        return TRUE;
                    }

                    err = WSAAsyncSelect(msock, hwnd, WM_SOCKET_NOTIFY, FD_ACCEPT | FD_CLOSE | FD_READ | FD_WRITE);

                    if ( err == SOCKET_ERROR ) {
                        EditPrintf(hwndEdit, TEXT("=== Error: select error ===\r\n"));
                        closesocket(msock);
                        WSACleanup();
                        return TRUE;
                    }

                    //fill the address info about server
                    sa.sin_family       = AF_INET;
                    sa.sin_port         = htons(SERVER_PORT);
                    sa.sin_addr.s_addr  = INADDR_ANY;

                    //bind socket
                    err = bind(msock, (LPSOCKADDR)&sa, sizeof(struct sockaddr));

                    if( err == SOCKET_ERROR ) {
                        EditPrintf(hwndEdit, TEXT("=== Error: binding error ===\r\n"));
                        WSACleanup();
                        return FALSE;
                    }

                    err = listen(msock, 2);
        
                    if( err == SOCKET_ERROR ) {
                        EditPrintf(hwndEdit, TEXT("=== Error: listen error ===\r\n"));
                        WSACleanup();
                        return FALSE;
                    }
                    else {
                        EditPrintf(hwndEdit, TEXT("=== Server START ===\r\n"));
                    }

                    break;
                case ID_EXIT:
                    EndDialog(hwnd, 0);
                    break;
            };
            break;

        case WM_CLOSE:
            EndDialog(hwnd, 0);
            break;

        case WM_SOCKET_NOTIFY:
            switch( WSAGETSELECTEVENT(lParam) )
            {
                case FD_ACCEPT:
                    ssock = accept(msock, NULL, NULL);
                    Socks.push_back(ssock);
                    EditPrintf(hwndEdit, TEXT("=== Accept one new client(%d), List size:%d ===\r\n"), ssock, Socks.size());
                    break;
                case FD_READ:
                //Write your code for read event here.
                    /* broweser will make multi connection, keep the first socket we want */
                    index = -1;
                    first_null = MAXCONN;

                    for (int i = 0; i < MAXCONN; i++)
                    {
                        if (web[i].websocket == wParam)  /* wParam is already recorded, find the position in record */
                        {
                            index = i;
                            break;
                        }

                        if (i < first_null && web[i].websocket == NULL) /* find the first position not in use */
                        {
                            first_null = i;
                        }

                        if (i == MAXCONN-1 && index < 0 && first_null >= 0)  /* wParam not in record , and still have position can allocate */
                        {
                            web[first_null].websocket = wParam;
                            index = first_null;
                        }
                        else if(i == MAXCONN - 1 && index < 0 && first_null < 0)
                        {
                            char buffer[MAX_HEADER_LEN];
                            sprintf(buffer, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
                            sprintf(buffer, "%s FULL", buffer);
                            send(wParam, buffer, strlen(buffer), 0);
                            closesocket(wParam);
                        }
                    }

                    if (index > -1 && index < MAXCONN)
                    {
                        Header request = parseRequest(web[index].websocket);
                        char buffer[MAX_HEADER_LEN];

                        if (strcmp(request.path, "form_get.htm") == 0)
                        {
                            sprintf(buffer, "%s 200 OK\r\nContent-Type: text/html\r\n\r\n", request.protocol);
                            send(web[index].websocket, buffer, strlen(buffer), 0);

                            char read_buffer[BUFFER_LEN];
                            int read_len;
                            int readfd = open(request.path, O_RDONLY);
                            while ((read_len = read(readfd, read_buffer, BUFFER_LEN)) > 0)
                            {
                                send(web[index].websocket, read_buffer, read_len, 0);
                            }

                            close(readfd);
                            closesocket(web[index].websocket);
                        }
                        else if (strcmp(request.path, "hw3.cgi") == 0)
                        {
                            sprintf(buffer, "%s 200 OK\r\n", request.protocol);
                            send(web[index].websocket, buffer, strlen(buffer), 0);

                            parseQueryString(index, request.query_string);
                            responseHTML(index, web[index].websocket);
                            openBatchFile(index);
                            connectServer(index, web[index].websocket);

                            for (int i = 0; i < MAX_CLIENTS; i++)
                            {
                                if (web[index].clients[i].is_active)
                                {
                                    //OutputDebugString("New member to Select\n");
                                    err = WSAAsyncSelect(web[index].clients[i].clifd, hwnd, CGI_SOCKET_NOTIFY, FD_READ | FD_WRITE | FD_CLOSE);
                                    if (err == SOCKET_ERROR)
                                    {
                                        EditPrintf(hwndEdit, TEXT("=== Error: select error for client [%d]===\r\n"), i);
                                        closesocket(web[index].clients[i].clifd);
                                    }
                                }
                            }
                        }
                        else
                        {
                            sprintf(buffer, "%s 200 OK\r\nContent-Type: text/html\r\n\r\n", request.protocol);
                            sprintf(buffer, "%s Only provides remote batch service", buffer);
                            send(web[index].websocket, buffer, strlen(buffer), 0);
                            closesocket(web[index].websocket);
                            web[index].websocket = NULL;
                        }
                    }
                    
                    break;
                case FD_WRITE:
                //Write your code for write event here

                    break;
                case FD_CLOSE:
                    for (int i = 0; i < MAXCONN; i++)
                    {
                        if (web[i].websocket == wParam)
                        {
                            closesocket(web[i].websocket);
                            web[i].websocket = NULL;
                            for (int j = 0; j < MAX_CLIENTS; j++)
                            {
                                send(web[i].clients[j].clifd, "exit\n", strlen("exit\n"), 0);
                                web[i].clients[j] = { -1, NULL, NULL, NULL, NULL, 0, NULL };
                            }
                        }
                    }
                    break;
            };
            break;
        case CGI_SOCKET_NOTIFY:
            switch (WSAGETSELECTEVENT(lParam))
            {
                case FD_READ:
                    index = -1;
                    try
                    {
                        index = web_clifd_map.at(wParam);
                    }
                    catch (const char* message)
                    {
                        break;
                    }

                    for (int i = 0; i < MAX_CLIENTS; i++)
                    {
                        if (web[index].clients[i].is_active && web[index].clients[i].clifd == wParam)
                        {
                            char buffer[BUFFER_LEN];
                            char cmdline[16384];

                            int socketlen = recv(web[index].clients[i].clifd, buffer, BUFFER_LEN, 0);
                            
                            if (socketlen > 0)
                            {
                                buffer[socketlen] = '\0';
                                script_content(buffer, i, 0, web[index].websocket);

                                if (prompt_check(buffer))
                                {
                                    fgets(cmdline, strlen(cmdline), web[index].clients[i].batch_fp);
                                    send(web[index].clients[i].clifd, cmdline, strlen(cmdline), 0);
                                    script_content(cmdline, i, 1, web[index].websocket);
                                }
                            }
                        }
                    }

                    
                case FD_WRITE:
                    break;
                case FD_CLOSE:
                    index = -1;
                    try
                    {
                        index = web_clifd_map.at(wParam);
                    }
                    catch (const char* message)
                    {
                        break;
                    }

                    for (int i = 0; i < MAX_CLIENTS; i++)
                    {
                        if (web[index].clients[i].is_active && web[index].clients[i].clifd == wParam)
                        {
                            closesocket(web[index].clients[i].clifd);
                            web[index].clients[i].is_active = 0;
                            web[index].clients[i] = {-1 ,NULL, NULL, NULL, NULL, 0, NULL};
                            web_clifd_map.erase(web[index].clients[i].clifd);
                        }
                    }

                    if (check_end(index))
                    {
                        closesocket(web[index].websocket);
                    }
                    break;
                default:
                    break;
            }
            break;

        default:
            return FALSE;
    };

    return TRUE;
}

int EditPrintf (HWND hwndEdit, TCHAR * szFormat, ...)
{
     TCHAR   szBuffer [BUFFER_LEN] ;
     va_list pArgList ;

     va_start (pArgList, szFormat) ;
     wvsprintf (szBuffer, szFormat, pArgList) ;
     va_end (pArgList) ;

     SendMessage (hwndEdit, EM_SETSEL, (WPARAM) -1, (LPARAM) -1) ;
     SendMessage (hwndEdit, EM_REPLACESEL, FALSE, (LPARAM) szBuffer) ;
     SendMessage (hwndEdit, EM_SCROLLCARET, 0, 0) ;
     return SendMessage(hwndEdit, EM_GETLINECOUNT, 0, 0); 
}
