#include <getopt.h>
#define _WIN32_WINNT 0x0600
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

static int read_line_sock(SOCKET fd, char *buf, size_t cap){
    size_t n = 0;
    while(n + 1 < cap){
        char c; int r = recv(fd, &c, 1, 0);
        if(r == 0) return 0; if(r < 0){ if(WSAGetLastError()==WSAEINTR) continue; return -1; }
        if(c == '\n'){ buf[n]=0; return (int)n; }
        buf[n++]=c;
    }
    buf[n]=0; return (int)n;
}

int main(int argc, char **argv){
    int opt; const char *addr=NULL, *port=NULL;
    while((opt=getopt(argc,argv,"a:p:h"))!=-1){
        if(opt=='a') addr=optarg;
        else if(opt=='p') port=optarg;
        else { fprintf(stderr,"Usage: %s -a ADDR -p PORT\n", argv[0]); return 2; }
    }
    if(!addr || !port){ fprintf(stderr,"Usage: %s -a ADDR -p PORT\n", argv[0]); return 2; }

    WSADATA wsa;
    if(WSAStartup(MAKEWORD(2,2), &wsa) != 0){
        fprintf(stderr,"WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    struct addrinfo hints, *res=NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family=AF_INET; hints.ai_socktype=SOCK_STREAM;

    int gi = getaddrinfo(addr, port, &hints, &res);
    if(gi != 0){ fprintf(stderr,"getaddrinfo: %s\n", gai_strerrorA(gi)); WSACleanup(); return 1; }

    SOCKET fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(fd == INVALID_SOCKET){ fprintf(stderr,"socket: %d\n", WSAGetLastError()); freeaddrinfo(res); WSACleanup(); return 1; }
    if(connect(fd, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR){
        fprintf(stderr,"connect: %d\n", WSAGetLastError()); freeaddrinfo(res); closesocket(fd); WSACleanup(); return 1;
    }
    freeaddrinfo(res);

    char buf[128];
    int r = read_line_sock(fd, buf, sizeof(buf));
    if(r>0) printf("%s\n", buf);

    while(1){
        printf("your guess> "); fflush(stdout);
        if(!fgets(buf, sizeof(buf), stdin)) break;
        size_t len=strlen(buf); if(len && buf[len-1]=='\n') buf[len-1]=0;
        if(strcmp(buf,"quit")==0 || strcmp(buf,"exit")==0) break;

        char out[128]; snprintf(out, sizeof(out), "%s\n", buf);
        if(send(fd, out, (int)strlen(out), 0) < 0){ fprintf(stderr,"send: %d\n", WSAGetLastError()); break; }

        int rr = read_line_sock(fd, buf, sizeof(buf));
        if(rr<=0){ printf("disconnected\n"); break; }
        printf("server: %s\n", buf);

        if(strncmp(buf,"EQUAL",5)==0){
            int rr2 = read_line_sock(fd, buf, sizeof(buf));
            if(rr2>0) printf("server: %s\n", buf);
        }
    }
    closesocket(fd);
    WSACleanup();
    return 0;
}
