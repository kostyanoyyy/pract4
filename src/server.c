#include <getopt.h>
#define _WIN32_WINNT 0x0600
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

static void die_wsa(const char* msg){
    fprintf(stderr, "%s: WSA err=%d\n", msg, WSAGetLastError());
    exit(EXIT_FAILURE);
}

static int read_line(SOCKET fd, char *buf, size_t cap){
    size_t n = 0;
    while(n + 1 < cap){
        char c; int r = recv(fd, &c, 1, 0);
        if(r == 0) return 0;                 // peer closed
        if(r < 0){ if(WSAGetLastError()==WSAEINTR) continue; return -1; }
        if(c == '\n'){ buf[n] = 0; return (int)n; }
        buf[n++] = c;
    }
    buf[n] = 0; return (int)n;
}

int main(int argc, char **argv){
    // разбор -p PORT
    int opt; const char *port = NULL;
    while((opt = getopt(argc, argv, "p:h")) != -1){
        if(opt=='p') port = optarg;
        else { fprintf(stderr,"Usage: %s -p PORT\n", argv[0]); return 2; }
    }
    if(!port){ fprintf(stderr,"Usage: %s -p PORT\n", argv[0]); return 2; }

    WSADATA wsa;
    if(WSAStartup(MAKEWORD(2,2), &wsa) != 0) die_wsa("WSAStartup");

    struct addrinfo hints, *res=NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE; // 0.0.0.0

    int gi = getaddrinfo(NULL, port, &hints, &res);
    if(gi != 0){ fprintf(stderr,"getaddrinfo: %s\n", gai_strerrorA(gi)); WSACleanup(); return 1; }

    SOCKET srv = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(srv == INVALID_SOCKET) die_wsa("socket");

    const char yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    if(bind(srv, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) die_wsa("bind");
    if(listen(srv, 16) == SOCKET_ERROR) die_wsa("listen");
    freeaddrinfo(res);

    printf("[server] listening on port %s\n", port); fflush(stdout);
    srand((unsigned)time(NULL));

    for(;;){
        struct sockaddr_in cli; int clilen=(int)sizeof(cli);
        SOCKET fd = accept(srv, (struct sockaddr*)&cli, &clilen);
        if(fd == INVALID_SOCKET){
            if(WSAGetLastError()==WSAEINTR) continue;
            die_wsa("accept");
        }

        char ip[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));
        unsigned short cport = ntohs(cli.sin_port);

        int target = 1 + rand()%100;
        int attempts = 0;

        printf("%s:%u: CONNECT\n", ip, cport); fflush(stdout);
        const char *hello = "GUESS 1..100\n";
        send(fd, hello, (int)strlen(hello), 0);

        char line[128];
        while(1){
            int r = read_line(fd, line, sizeof(line));
            if(r == 0){ printf("%s:%u: DISCONNECT\n", ip, cport); break; }
            if(r < 0){ fprintf(stderr,"recv error: %d\n", WSAGetLastError()); break; }

            attempts++;
            printf("%s:%u:%s\n", ip, cport, line); fflush(stdout);

            char *end=NULL; long guess = strtol(line, &end, 10);
            if(end==line){ const char *err="ERR not a number\n"; send(fd, err, (int)strlen(err), 0); continue; }

            if(guess < target){ const char *ans="LESS\n";    send(fd, ans, (int)strlen(ans), 0); }
            else if(guess > target){ const char *ans="GREATER\n"; send(fd, ans, (int)strlen(ans), 0); }
            else {
                char ok[64]; snprintf(ok, sizeof(ok), "EQUAL %d\n", attempts);
                send(fd, ok, (int)strlen(ok), 0);
                target = 1 + rand()%100; attempts = 0;
                const char *again="NEW 1..100\n";
                send(fd, again, (int)strlen(again), 0);
            }
        }
        closesocket(fd);
    }

    // недостижимо в этом бесконечном цикле, но формально:
    // closesocket(srv);
    // WSACleanup();
    // return 0;
}
