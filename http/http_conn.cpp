#include<unistd.h>
#include<sys/unistd.h>
#include<arpa/inet.h>
#include<netinet/in.h>

#include"../epoll/epoll.h"
#include"http_conn.h"



int http_conn::m_epollfd;
int http_conn::m_user_count;

http_conn::http_conn() {

}
http_conn::~http_conn() {

}

void http_conn::init(int sockfd, const sockaddr_in& addr) {
	m_sockfd = sockfd;	m_address = addr;
	m_user_count++;
	addfd(m_epollfd, sockfd, true);		//�ѿͻ���fd��ӵ���������
	init();		//��������ֵ�ĳ�ʼ��
}
void http_conn::init() {

}