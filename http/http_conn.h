#ifndef HTTP_CONN_H
#define HTTP_CONN_H


#include<unistd.h>
#include<sys/unistd.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<map>
#include<sys/stat.h>

#include"../CGImysql/sql_connection_pool.h"

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

    //主状态机的状态：解析请求行  解析请求头  解析消息体(仅用于解析POST请求)
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };
    //报文解析的结果：请求不完整需要继续读取请求报文数据  获得了完整的HTTP请求  HTTP请求报文有语法错误  服务器内部错误,该结果在主状态机逻辑switch的default下,一般不会触发
    enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };
    //从状态机的状态：完整读取一行  报文语法有误  读取的行不完整
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };

public:
    static int m_epollfd;           //eollfd
    static int m_user_count;    //用户数量
    MYSQL* mysql;

public:
    http_conn() {};
    ~http_conn() {};
    //初始化套接字地址，函数内部会调用私有方法init
    void init(int  sockfd, const sockaddr_in& addr);
    void close_conn(bool real_close = true);
    bool read_once();
    char* get_line() { return m_read_buf + m_start_line; }      //
    LINE_STATUS parse_line();       //从状态机，解析一行报文，看看是否：完整一行/格式错误/未完整一行
    bool write();

public:
    void process();
    //读取http报文相关函数
    HTTP_CODE process_read();
    HTTP_CODE parse_request_line(char* text);
    HTTP_CODE parse_headers(char* text);
    HTTP_CODE parse_content(char* text);
    HTTP_CODE do_request();
    //相应http报文相关函数
    bool process_write(HTTP_CODE ret);
    bool add_response(const char* format, ...);
    bool add_content(const char* content);
    bool add_status_line(int status, const char* title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

private:
    //内部私有init
    void init();
    void unmap();

private:     //private
    int m_sockfd;
    sockaddr_in m_address;
    CHECK_STATE m_check_state;      //主状态机的状态
    METHOD m_method;

    char m_read_buf[READ_BUFFER_SIZE];      //读缓冲区
    int m_read_idx;
    int m_checked_idx;
    int m_start_line;

    char m_write_buf[WRITE_BUFFER_SIZE];    //写缓冲区
    int m_write_idx;

    //通过解析http请求报文得到的信息
    char m_real_file[FILENAME_LEN];
    char* m_url;
    char* m_version;
    char* m_host;
    int m_content_length;
    char* m_string;         //用户名密码
    bool m_linger;          //http是否长连接
    
    /*
    ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
    ssize_t writev(int fd, const struct iovec *iov, int iovcnt);
    fd是要在其上进行读或是写的文件描述符；iov是读或写所用的I/O向量；iovcnt是要使用的向量元素个数。
    */
    struct iovec m_iv[2];   int m_iv_count;     //这两个是与readv和wirtev操作相关的结构体。
    struct stat m_file_stat;    //用于存放访问资源的stat属性
    char* m_file_address;     //用于存放mmap的地址

    int bytes_to_send;          //
    int bytes_have_send;      //
};

#endif