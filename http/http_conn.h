#ifndef HTTP_CONN_H
#define HTTP_CONN_H


#include<unistd.h>
#include<sys/unistd.h>
#include<arpa/inet.h>
#include<netinet/in.h>

class http_conn
{
public:
    //设置读取文件的名称m_real_file大小
    static const int FILENAME_LEN = 200;
    //设置读缓冲区m_read_buf大小
    static const int READ_BUFFER_SIZE = 2048;
    //设置写缓冲区m_write_buf大小
    static const int WRITE_BUFFER_SIZE = 1024;

    //报文的请求方法，本项目只用到GET和POST
    enum METHOD { GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATH };

    //主状态机的状态
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };
    //报文解析的结果
    enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };
    //从状态机的状态
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };

public:
    static int m_epollfd;           //eollfd
    static int m_user_count;    //用户数量
    //MYSQL* mysql;

public:
    http_conn();
    ~http_conn();

public:
    //初始化套接字地址，函数内部会调用私有方法init
    void init(int  sockfd, const sockaddr_in& addr);

private:
    //内部私有init
    void init();

public:     //private
    int m_sockfd;
    sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE];
    char m_write_buf[WRITE_BUFFER_SIZE];
};

#endif