#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <errno.h>
#include "transport.h"

TRANS::TRANS(bool listener, uint16 port, uint16 time_interval, const char* ip):
                listener(listener), port(port), time_interval(time_interval)
{
    memset(&this->srcaddr, 0, sizeof(this->srcaddr));
    memset(&this->dstaddr, 0, sizeof(this->dstaddr));
    struct sockaddr_in* addr = &this->srcaddr;
    if(!listener){
        addr = &this->dstaddr;
        addr->sin_addr.s_addr = inet_addr(ip);
    }else{
        addr->sin_addr.s_addr = INADDR_ANY;
        this->ip = ANY_IP;
    }
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
}

int TRANS::init(){
    this->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(this->fd < 0){
        return TRANS_SOCKET_ERROR;
    }
    if(listener){
        int ret = bind(this->fd, (struct sockaddr*)&this->srcaddr, sizeof(this->srcaddr));
        if(ret < 0){
            return TRANS_BIND_ERROR;
        }
    }

    this->epollfd = epoll_create(this->fd + 1);
    if(this->epollfd < 0){
        return TRANS_EPOLL_ERROR;
    }

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = this->fd;

    int iret = epoll_ctl(this->epollfd, EPOLL_CTL_ADD, this->fd, &event);
    if(iret < 0){
        return TRANS_EPOLL_ADD_ERROR;
    }

    return TRANS_NOERROR;
}

int TRANS::fini(){
    close(this->epollfd);
    close(this->fd);
    return TRANS_NOERROR;
}

int TRANS::send(const void* buf, uint32 size){
    int iRet = sendto(this->fd, buf, size, 0, (struct sockaddr*)&this->dstaddr, sizeof(this->dstaddr));
    if(iRet < 0){
        printf("send error\n");
    }
    return iRet;
}

void TRANS::recv(void* buf, uint32& size){
    struct epoll_event event;

    int nfd = epoll_wait(this->epollfd, &event, 1, 1);
    if(nfd){
        int len;
        struct sockaddr_in tmpaddr;
        uint32 sin_size = sizeof(tmpaddr);
        len = recvfrom(this->fd, buf, size, MSG_DONTWAIT, (struct sockaddr*)&tmpaddr, &sin_size);
        if(this->dstaddr.sin_addr.s_addr == 0){
            memcpy(&this->dstaddr, &tmpaddr, sin_size);
        }else{
            if(this->dstaddr.sin_addr.s_addr != tmpaddr.sin_addr.s_addr){
                int i = *(int*)0x0;
                printf("error source package!\n");
            }
        }
        if(len > 0){
            size = len;
        }else{
            printf("error recv\n");
            size = 0;
        }
    }else{
        size = 0;
    }
}
