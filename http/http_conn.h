#ifndef HTTP_CONN_H
#define HTTP_CONN_H


#include<unistd.h>
#include<sys/unistd.h>
#include<arpa/inet.h>
#include<netinet/in.h>

class http_conn
{
public:
    //���ö�ȡ�ļ�������m_real_file��С
    static const int FILENAME_LEN = 200;
    //���ö�������m_read_buf��С
    static const int READ_BUFFER_SIZE = 2048;
    //����д������m_write_buf��С
    static const int WRITE_BUFFER_SIZE = 1024;

    //���ĵ����󷽷�������Ŀֻ�õ�GET��POST
    enum METHOD { GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATH };

    //��״̬����״̬
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };
    //���Ľ����Ľ��
    enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };
    //��״̬����״̬
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };

public:
    static int m_epollfd;           //eollfd
    static int m_user_count;    //�û�����
    //MYSQL* mysql;

public:
    http_conn();
    ~http_conn();

public:
    //��ʼ���׽��ֵ�ַ�������ڲ������˽�з���init
    void init(int  sockfd, const sockaddr_in& addr);

private:
    //�ڲ�˽��init
    void init();

public:     //private
    int m_sockfd;
    sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE];
    char m_write_buf[WRITE_BUFFER_SIZE];
};

#endif