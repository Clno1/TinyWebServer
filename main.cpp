#include <stdio.h>
#include <unistd.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<sys/unistd.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include <assert.h>
#include <cstring>
#include<string>
#include<sys/epoll.h>
#include <errno.h>
#include <fcntl.h>
#include<signal.h>

#include "epoll/epoll.h"
#include "http/http_conn.h"
#include"timer/lst_timer.h"
#include"CGImysql/sql_connection_pool.h"
#include"threadpool/threadpool.h"


#define MAX_FD 65536        //�����ļ�����������
#define MAX_EVENT_NUMBER 10000      //����¼�����
#define TIMESLOT 10          //ʱ����

int epollfd,pipefd[2];
sort_timer_lst timer_lst;       //��ʱ������
int listen_init(int port);
int timer_init(int epollfd,int *pipefd);
void cb_func(client_data* user_data);
void timer_handler();

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

    //ͨ������ģʽ������ݿ����ӳأ����֮���ʼ��
    connection_pool* connPool = connection_pool::GetInstance();
    connPool->init("localhost", "root", "root", "yourdb", 3306, 8);
    //�����̳߳�,�̳߳ص�T��http_conn����
    threadpool<http_conn>* pool = new threadpool<http_conn>(connPool);

    timer_init(epollfd,pipefd);     //��ʱ����ʼ����

    addfd(epollfd,listenfd,false);      //��listenfd��ӵ������������

    client_data* user_timer = new client_data[MAX_FD];      //�û���Ϣ���飬��ʱ�����Ҫ�ã����ڻص��رո��û����ӣ�
    http_conn* users = new http_conn[MAX_FD];       //�û���������
    assert(users);

    bool stop_server = false;
    bool timeout = false;       //��ʱʱ���Ƿ���
    alarm(TIMESLOT);        //���µ�һ������
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

            //�������������¼�
            if (sockfd == listenfd) {
                struct sockaddr_in client_address;
                socklen_t client_address_len = sizeof(client_address);
                
                //LT
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_address_len);
                if (connfd < 0) {
                    //accept����
                    continue;
                }
                if (http_conn::m_user_count >= MAX_FD) {
                    //�û��������������������
                    continue;
                }

                in_addr client_ip;
                memcpy(&client_ip, &client_address.sin_addr.s_addr, 4);
                printf("ip:%s connect\n", inet_ntoa(client_ip) );
                
                users[connfd].init(connfd, client_address);     // ��ʼ���¿ͻ�����epoll����

                //����timer��client_data
                user_timer[connfd].address = client_address;
                user_timer[connfd].sockfd = connfd;
                
                util_timer* timer = new util_timer;
                timer->user_data = &user_timer[connfd];
                timer->cb_func = cb_func;
                timer->expire = time(NULL) + 6 * TIMESLOT;

                user_timer[connfd].timer = timer;
                //���洴�����timer�����뵽������
                timer_lst.add_timer(timer);
            }
            //�Զ˹ر������¼�
            else if (whatopt & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                //�Զ˹ر��ˣ��������Ҳ�ر�Ȼ��ȡ����ʱ��
                util_timer* timer = user_timer[sockfd].timer;
                timer->cb_func(&user_timer[sockfd]);
                if (timer) timer_lst.del_timer(timer);
            }
            //��Ϊͳһ���¼�Դ���źŴ����ɶ��¼�������
            //��ôͳһ�������źŻص��������ﲻ�����������д����pipe��д��
            else if ((sockfd==pipefd[0]) && (whatopt & EPOLLIN)) {
                int sig;
                char signals[1024];
                int ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1) continue;
                if (ret == 0) continue;
                //�����ﴦ���ź�
                for (int i = 0; i < ret; i++) {
                    switch (signals[i])
                    {
                    case SIGALRM:
                        timeout = true;
                        break;
                    case SIGTERM:
                        stop_server = true;
                    default:
                        break;
                    }
                }
            }
            /*�����¼������벽���ǣ�
            process->porcess_read(����parse_line->parse_status_line/parse_headers/parse_content
            ->do_request)->process_write(add_line/heads/content...)
            ->�ѱ��Ĵ굽���������
           */
            else if (whatopt & EPOLLIN) {
                //��ʼ����������������
                util_timer* timer = user_timer[sockfd].timer;
                if (users[sockfd].read_once()) {        //1�����������ݶ�����
                    pool->append(users + sockfd);   //2������֮������̳߳�������з���һ������
                
                    //��Ϊ�������������԰�����ͻ��Ĳ���Ծ�¼��Ӻ�
                    //�Ӻ�ʱ��֮������λ�õ���
                    if (timer) {
                        timer->expire = time(NULL) + 6 * TIMESLOT;
                        timer_lst.adjust_timer(timer);
                    }
                }
                //read_once()ʧ�ܣ��ر����Ӱ�
                else {
                    timer->cb_func(&user_timer[sockfd]);
                    if (timer) timer_lst.del_timer(timer);
                }
            }
            //����¼�
            else if (whatopt & EPOLLOUT) {
                util_timer* timer = user_timer[sockfd].timer;
                //��������¼��Ѿ������Ӧ���ľ͵�����write��������������͸������
                if (users[sockfd].write()) {
                    //�����¼�һ�����Ӻ�����ͻ��Ĳ���Ծ�¼�
                    if (timer) {
                        timer->expire = time(NULL) + 6 * TIMESLOT;
                        timer_lst.adjust_timer(timer);
                    }
                }
                else {      //����Ļ�����write���͸������ʧ�ܣ��ر�����
                    timer->cb_func(&user_timer[sockfd]);
                    if (timer) timer_lst.del_timer(timer);
                }
            }
        }

        if (timeout) {
            timer_handler();
            printf("Now %d clients connect\n", http_conn::m_user_count);
            timeout = false;
        }
    }

    close(listenfd);
    close(epollfd);
    close(pipefd[0]);
    close(pipefd[1]);
    delete[] users;
    delete[] user_timer;
    delete pool;
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
//��ʱ���Ļص�������ִ�к�����
//�������þ��ǰ�����ͻ��رգ���ʱ���޷�Ӧ��
void cb_func(client_data* user_data) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}
//ע��sig�źŵĻص�����Ϊhandler
void addsig(int sig, void (handler)(int), bool restart = true) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}
//�źŴ����ص�������
void sig_handler(int sig) {
    //Ϊ��֤�����Ŀ������ԣ�����ԭ����errno
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}
//���ö�ʱ��ǰ��С����
int timer_init(int epollfd,int* pipefd) {
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnoblocking(pipefd[1]);       //���ùܵ�д��Ϊ����������ֹ����������
    addfd(epollfd, pipefd[0], false);   //���ùܵ�����ΪET������

    addsig(SIGALRM, sig_handler, false);
    addsig(SIGTERM, sig_handler, false);
    return ret;
}
//��ʱ�����������¶�ʱ�Բ��ϴ���SIGALRM�ź�
void timer_handler() {
    timer_lst.tick();       //ʱ�䵽������ʼ����
    alarm(TIMESLOT);        //������һ��ʱ��
}