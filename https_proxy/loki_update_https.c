#define _GNU_SOURCE
#include <arpa/inet.h>
#include <dlfcn.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>

static int (*real_connect)(int, const struct sockaddr *, socklen_t);
static struct in_addr *loki_update;

void _init(void) {
    char *hostname = "updates.lokigames.com";
    *(void **) (&real_connect) = dlsym(RTLD_NEXT, "connect");
    struct hostent *lh = gethostbyname(hostname);
    if(!lh) {
        fprintf(stderr, "Can't resolv %s ... exiting\n", hostname);
        exit(1);
    }
    loki_update = (struct in_addr*)lh->h_addr;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    if(addr->sa_family == AF_INET) {
        struct sockaddr_in *sin = (struct sockaddr_in *) addr;
        if(ntohs(sin->sin_port) == 80 &&
           sin->sin_addr.s_addr == loki_update->s_addr)
        {
            sin->sin_port = htons(8888);
            sin->sin_addr.s_addr = inet_addr("127.0.0.1");
        }
    }
    return real_connect(sockfd, addr, addrlen);
}
