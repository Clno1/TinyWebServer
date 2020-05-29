#ifndef EPOLL_H
#define EPOLL_H

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

extern int epoll_myinit();

//把fd添加到监听红黑树epollfd上
extern void addfd(int epollfd, int fd, bool oneshot);
extern void removefd(int epollfd, int fd);
extern void modfd(int epollfd, int fd, int ev);
extern int setnoblocking(int fd);

#endif