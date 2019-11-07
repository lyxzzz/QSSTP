#ifndef _TRANSPORT_H
#define _TRANSPORT_H
#include "type.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define MAX_BUF_SIZE 1024
#define ANY_IP "0.0.0.0"

enum TRANS_ERROR{
    TRANS_NOERROR=0,
    TRANS_SOCKET_ERROR=-1,
    TRANS_BIND_ERROR=-2,
    TRANS_EPOLL_ERROR=-3,
    TRANS_EPOLL_ADD_ERROR=-4
};

struct TRANS{
    bool listener;
    int fd;
    int epollfd;

    uint16 port;
    const char* ip;
    
    struct sockaddr_in srcaddr;
    struct sockaddr_in dstaddr;

    uint16 time_interval;

    TRANS(bool listener, uint16 port, uint16 time_interval, const char* ip=ANY_IP);
    int init();
    int fini();
    void recv(void* buf, uint32& size);
    int send(const void* buf, uint32 size);
};

#endif