#include <stdio.h>
#include <unistd.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<sys/unistd.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include <assert.h>
#include <strings.h>
#include<sys/epoll.h>
#include <errno.h>

#include"epoll.h"

int epoll_myinit() {
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    return epollfd;
}

//��fd��ӵ����������epollfd��
void addfd(int epollfd, int fd, bool oneshot) {
    epoll_event event;
    event.data.fd = fd;

    event.events = EPOLLIN | EPOLLRDHUP;        //���¼�

    if (oneshot)
        event.events |= EPOLLONESHOT;

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
}