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


#define MAX_FD 65536        //最大的文件描述符数量
#define MAX_EVENT_NUMBER 10000      //最大事件数量
#define TIMESLOT 10          //时间间隔

int epollfd,pipefd[2];
sort_timer_lst timer_lst;       //定时器链表
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
    int listenfd=listen_init(atoi(argv[1]));      //监听客户端前的初始化工作
    
    int epollfd=epoll_myinit();       //epoll前的初始化工作
    http_conn::m_epollfd = epollfd;
    epoll_event events[MAX_EVENT_NUMBER];

    //通过单例模式获得数据库连接池，获得之后初始化
    connection_pool* connPool = connection_pool::GetInstance();
    connPool->init("localhost", "root", "root", "yourdb", 3306, 8);
    //创建线程池,线程池的T是http_conn类型
    threadpool<http_conn>* pool = new threadpool<http_conn>(connPool);

    timer_init(epollfd,pipefd);     //定时器初始工作

    addfd(epollfd,listenfd,false);      //把listenfd添加到监听红黑树上

    client_data* user_timer = new client_data[MAX_FD];      //用户信息数组，定时器结点要用（用于回调关闭该用户连接）
    http_conn* users = new http_conn[MAX_FD];       //用户连接数组
    assert(users);

    bool stop_server = false;
    bool timeout = false;       //定时时间是否到了
    alarm(TIMESLOT);        //定下第一个闹钟
    while (!stop_server)
    {
        int total = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
        if (total < 0 && errno != EINTR) {
                //把错误记录到日志
            break;
        }

        for (int i = 0; i < total; i++) {
            int sockfd = events[i].data.fd;
            int whatopt = events[i].events;

            //有新连接请求事件
            if (sockfd == listenfd) {
                struct sockaddr_in client_address;
                socklen_t client_address_len = sizeof(client_address);
                
                //LT
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_address_len);
                if (connfd < 0) {
                    //accept错误
                    continue;
                }
                if (http_conn::m_user_count >= MAX_FD) {
                    //用户数量超过最大描述符了
                    continue;
                }

                in_addr client_ip;
                memcpy(&client_ip, &client_address.sin_addr.s_addr, 4);
                printf("ip:%s connect\n", inet_ntoa(client_ip) );
                
                users[connfd].init(connfd, client_address);     // 初始化新客户，并epoll监听

                //创造timer和client_data
                user_timer[connfd].address = client_address;
                user_timer[connfd].sockfd = connfd;
                
                util_timer* timer = new util_timer;
                timer->user_data = &user_timer[connfd];
                timer->cb_func = cb_func;
                timer->expire = time(NULL) + 6 * TIMESLOT;

                user_timer[connfd].timer = timer;
                //上面创造好了timer，加入到链表中
                timer_lst.add_timer(timer);
            }
            //对端关闭连接事件
            else if (whatopt & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                //对端关闭了，我们这边也关闭然后取消定时器
                util_timer* timer = user_timer[sockfd].timer;
                timer->cb_func(&user_timer[sockfd]);
                if (timer) timer_lst.del_timer(timer);
            }
            //因为统一了事件源，信号处理当成读事件来处理
            //怎么统一？就是信号回调函数哪里不立即处理而是写到：pipe的写端
            else if ((sockfd==pipefd[0]) && (whatopt & EPOLLIN)) {
                int sig;
                char signals[1024];
                int ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1) continue;
                if (ret == 0) continue;
                //在这里处理信号
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
            /*输入事件，理想步骤是：
            process->porcess_read(不断parse_line->parse_status_line/parse_headers/parse_content
            ->do_request)->process_write(add_line/heads/content...)
            ->把报文搓到输出缓冲区
           */
            else if (whatopt & EPOLLIN) {
                //开始处理这个浏览器请求
                util_timer* timer = user_timer[sockfd].timer;
                if (users[sockfd].read_once()) {        //1，把所有数据读进来
                    pool->append(users + sockfd);   //2，读完之后把往线程池任务队列放入一个任务
                
                    //因为有了新请求，所以把这个客户的不活跃事件延后
                    //延后时间之后做出位置调整
                    if (timer) {
                        timer->expire = time(NULL) + 6 * TIMESLOT;
                        timer_lst.adjust_timer(timer);
                    }
                }
                //read_once()失败，关闭连接吧
                else {
                    timer->cb_func(&user_timer[sockfd]);
                    if (timer) timer_lst.del_timer(timer);
                }
            }
            //输出事件
            else if (whatopt & EPOLLOUT) {
                util_timer* timer = user_timer[sockfd].timer;
                //在上面读事件已经搓好响应报文就等这里write把输出缓冲区发送给浏览器
                if (users[sockfd].write()) {
                    //跟读事件一样，延后这个客户的不活跃事件
                    if (timer) {
                        timer->expire = time(NULL) + 6 * TIMESLOT;
                        timer_lst.adjust_timer(timer);
                    }
                }
                else {      //这里的话就是write发送给浏览器失败，关闭连接
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
//定时器的回调函数（执行函数）
//函数作用就是把这个客户关闭（长时间无反应）
void cb_func(client_data* user_data) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}
//注册sig信号的回调函数为handler
void addsig(int sig, void (handler)(int), bool restart = true) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}
//信号处理（回调）函数
void sig_handler(int sig) {
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}
//设置定时器前的小工作
int timer_init(int epollfd,int* pipefd) {
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnoblocking(pipefd[1]);       //设置管道写端为非阻塞，防止缓冲区满了
    addfd(epollfd, pipefd[0], false);   //设置管道读端为ET非阻塞

    addsig(SIGALRM, sig_handler, false);
    addsig(SIGTERM, sig_handler, false);
    return ret;
}
//定时处理任务，重新定时以不断触发SIGALRM信号
void timer_handler() {
    timer_lst.tick();       //时间到咯，开始清理
    alarm(TIMESLOT);        //定下下一个时间
}