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

#include "epoll/epoll.h"
#include "http/http_conn.h"

#define MAX_FD 65536        //�����ļ�����������
#define MAX_EVENT_NUMBER 10000      //����¼�����
#define TIMESLOT 5          //ʱ����

int listen_init(int port);

int main(int argc,char *argv[])
{
    if (argc < 2) {
        printf("usage: %s ip_address port_number\n", argv[0]);
        return 1;
    }
    int listenfd=listen_init(atoi(argv[1]));      //�����ͻ���ǰ�ĳ�ʼ������
    
    int epollfd=epoll_myinit();       //epollǰ�ĳ�ʼ������
    http_conn::m_epollfd = epollfd;
    epoll_event events[MAX_EVENT_NUMBER];

    addfd(epollfd,listenfd,false);      //��listenfd��ӵ������������

    http_conn* users = new http_conn[MAX_FD];
    assert(users);

    bool stop_server = false;
    while (!stop_server)
    {
        int total = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
        if (total < 0 && errno != EINTR) {
                //�Ѵ����¼����־
            break;
        }

        for (int i = 0; i < total; i++) {
            int sockfd = events[i].data.fd;
            int whatopt = events[i].events;

            //������������
            if (sockfd == listenfd) {
                struct sockaddr_in client_address;
                socklen_t client_address_len = sizeof(client_address);
                
                //LT
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_address_len);
                if (connfd < 0) {
                    //
                    continue;
                }
                /*if (http_conn::m_user_conunt >= MAX_FD) {

                }*/

                users[connfd].init(connfd, client_address);     // ��ʼ���¿ͻ�����epoll����

            }
            //
            else if (whatopt & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {

            }
            //�����¼�
            else if (whatopt & EPOLLIN) {
                
                //Test
                read(users[sockfd].m_sockfd, users[sockfd].m_read_buf, http_conn::READ_BUFFER_SIZE);
                printf("%s\n", users[sockfd].m_read_buf);
            }
            //����¼�
            else if (whatopt & EPOLLOUT) {

            }
        }
    }

    close(listenfd);
    close(epollfd);

    pause();
    return 0;
}


int listen_init(int port) {
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    int flag = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    
    assert(bind(listenfd, (struct sockaddr*) & address, sizeof(address) )>= 0);

    assert(listen(listenfd, 20) >= 0);
    
    return listenfd;
}