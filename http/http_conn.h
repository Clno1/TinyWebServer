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
    //���ö�ȡ�ļ�������m_real_file��С
    static const int FILENAME_LEN = 200;
    //���ö�������m_read_buf��С
    static const int READ_BUFFER_SIZE = 2048;
    //����д������m_write_buf��С
    static const int WRITE_BUFFER_SIZE = 1024;

    //���ĵ����󷽷�������Ŀֻ�õ�GET��POST
    enum METHOD { GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATH };

    //��״̬����״̬������������  ��������ͷ  ������Ϣ��(�����ڽ���POST����)
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };
    //���Ľ����Ľ��������������Ҫ������ȡ����������  �����������HTTP����  HTTP���������﷨����  �������ڲ�����,�ý������״̬���߼�switch��default��,һ�㲻�ᴥ��
    enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };
    //��״̬����״̬��������ȡһ��  �����﷨����  ��ȡ���в�����
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };

public:
    static int m_epollfd;           //eollfd
    static int m_user_count;    //�û�����
    MYSQL* mysql;

public:
    http_conn() {};
    ~http_conn() {};
    //��ʼ���׽��ֵ�ַ�������ڲ������˽�з���init
    void init(int  sockfd, const sockaddr_in& addr);
    void close_conn(bool real_close = true);
    bool read_once();
    char* get_line() { return m_read_buf + m_start_line; }      //
    LINE_STATUS parse_line();       //��״̬��������һ�б��ģ������Ƿ�����һ��/��ʽ����/δ����һ��
    bool write();

public:
    void process();
    //��ȡhttp������غ���
    HTTP_CODE process_read();
    HTTP_CODE parse_request_line(char* text);
    HTTP_CODE parse_headers(char* text);
    HTTP_CODE parse_content(char* text);
    HTTP_CODE do_request();
    //��Ӧhttp������غ���
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
    //�ڲ�˽��init
    void init();
    void unmap();

private:     //private
    int m_sockfd;
    sockaddr_in m_address;
    CHECK_STATE m_check_state;      //��״̬����״̬
    METHOD m_method;

    char m_read_buf[READ_BUFFER_SIZE];      //��������
    int m_read_idx;
    int m_checked_idx;
    int m_start_line;

    char m_write_buf[WRITE_BUFFER_SIZE];    //д������
    int m_write_idx;

    //ͨ������http�����ĵõ�����Ϣ
    char m_real_file[FILENAME_LEN];
    char* m_url;
    char* m_version;
    char* m_host;
    int m_content_length;
    char* m_string;         //�û�������
    bool m_linger;          //http�Ƿ�����
    
    /*
    ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
    ssize_t writev(int fd, const struct iovec *iov, int iovcnt);
    fd��Ҫ�����Ͻ��ж�����д���ļ���������iov�Ƕ���д���õ�I/O������iovcnt��Ҫʹ�õ�����Ԫ�ظ�����
    */
    struct iovec m_iv[2];   int m_iv_count;     //����������readv��wirtev������صĽṹ�塣
    struct stat m_file_stat;    //���ڴ�ŷ�����Դ��stat����
    char* m_file_address;     //���ڴ��mmap�ĵ�ַ

    int bytes_to_send;          //
    int bytes_have_send;      //
};

#endif