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
#include <fcntl.h>

int epoll_myinit() {
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    return epollfd;
}

//把文件描述符fd设为非阻塞
int setnoblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//把fd添加到监听红黑树epollfd上
void addfd(int epollfd, int fd, bool oneshot) {
    epoll_event event;
    event.data.fd = fd;

    event.events = EPOLLIN | EPOLLRDHUP;        //读事件

    if (oneshot)
        event.events |= EPOLLONESHOT;

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnoblocking(fd);              //为什么？？？？？？？？？？？？？？
}
//把fd从监听红黑树epollfd上摘下来
void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd,0);
    close(fd);
}
//将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev) {
    epoll_event event;
    event.data.fd = fd;
    //LT
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}