#include<unistd.h>
#include<stdio.h>
#include<sys/unistd.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include <fcntl.h>
#include<sys/stat.h>
#include<sys/mman.h>
#include<sys/uio.h>

#include"../epoll/epoll.h"
#include"http_conn.h"


//����http��Ӧ��һЩ״̬��Ϣ
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file form this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the request file.\n";

//��վ��Ŀ¼���ļ����ڴ���������Դ����ת��html�ļ�
const char* doc_root = "/home/clno1/projects/TinyWebServer/root";

//�����е��û������������map
map<string, string> users;


int http_conn::m_epollfd=-1;
int http_conn::m_user_count=0;

//�ͻ��˶Ͽ�����
void http_conn::close_conn(bool real_close) {
	if (real_close && (m_sockfd != -1)) {
		removefd(m_epollfd, m_sockfd);
		m_sockfd = -1;
		m_user_count--;
	}
}
//�ͻ������ӳ�ʼ������¼�ͻ�����
void http_conn::init(int sockfd, const sockaddr_in& addr) {
	m_sockfd = sockfd;	m_address = addr;
	m_user_count++;
	addfd(m_epollfd, sockfd, true);		//�ѿͻ���fd��ӵ���������
	init();		//��������ֵ�ĳ�ʼ��
}
//��ʼ��������ӵĸ������ݣ�����Ӧ��һ���������initȻ�����¿�ʼ�µ�����
void http_conn::init() {
	mysql = NULL;
	m_check_state = CHECK_STATE_REQUESTLINE;		//��״̬����ʼ״̬������������
	bytes_to_send = 0;
	bytes_have_send = 0;
	m_linger = false;
	m_method = GET;
	m_url = 0;
	m_version = 0;
	m_content_length = 0;
	m_host = 0;
	m_start_line = 0;
	m_checked_idx = 0;
	m_read_idx = 0;
	m_write_idx = 0;
	//cgi=0;
	memset(m_read_buf, '\0', READ_BUFFER_SIZE);
	memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
	memset(m_real_file, '\0', FILENAME_LEN);
}

//ѭ����ȡ�ͻ����ݣ�ֱ�������ݿɶ���Է��ر�����
bool http_conn::read_once() {
	if (m_read_idx >= READ_BUFFER_SIZE) return false;		//�������治����
	int bytes_read = 0;		//ÿһ��recv������������
	while (true)
	{
		bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
		if (bytes_read == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) break;	//û���ݶ���
			return false;		//��������
		}
		else if (bytes_read==0) {
			//return true;
			return false;		//��������
		}
		m_read_idx += bytes_read;
	}
	return true;
}

//��״̬�������ȡbuffer�е����ݣ���ÿ������ĩβ��\r\n��Ϊ\0\0��
//�����´�״̬����buffer�ж�ȡ��λ��m_checked_idx���Դ���������״̬��������
http_conn::LINE_STATUS http_conn::parse_line() {
	char tmp;
	//�Ӷ��뻺������ȡ�����ݽ��н���
	for (; m_checked_idx < m_read_idx; ++m_checked_idx) {
		tmp = m_read_buf[m_checked_idx];
		//�����ǰ��\r�ַ������п��ܻ��ȡ��������
		if (tmp == '\r') {
			if ((m_checked_idx + 1) == m_read_idx)  //��ǰΪ\r���Һ���û�ˣ�������
				return LINE_OPEN;	
			else if (m_read_buf[m_checked_idx + 1] == '\n') {		//��һ���ַ���\n�����������У���\r\n��Ϊ\0\0
				m_read_buf[m_checked_idx++] = '\0';
				m_read_buf[m_checked_idx++] = '\0';
				return LINE_OK;
			}
			//����������ϣ��򷵻��﷨����
			return LINE_BAD;
		}
		//�����ǰ�ַ���\n��Ҳ�п��ܶ�ȡ��������
		else if (tmp == '\n') {
			//�ж�ǰһ���ַ��ǲ���\r���ǵĻ�Ҳ�ǽ���������
			if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r') {
				m_read_buf[m_checked_idx - 1] = '\0';
				m_read_buf[m_checked_idx++] = '\0';
				return LINE_OK;
			}
			//�����﷨����
			return LINE_BAD;
		}
	}
	//���껺���������ݶ�û���ҵ�\r\n��������������
	return LINE_OPEN;
}


//�߳���������֮���ʲô
void http_conn::process() {
	HTTP_CODE read_ret = process_read();
	if (read_ret == NO_REQUEST) {		//��������������������
		modfd(m_epollfd, m_sockfd, EPOLLIN);
		return;
	}

	bool write_ret = process_write(read_ret);	//����Ӧ����
	if (!write_ret) close_conn();
	modfd(m_epollfd, m_sockfd, EPOLLOUT);
}



//������
//ע�������while���������˳���Ҫ�����ǻ�����Ҫ�����ǻ����������HTTP����Ȼ�����do_request
http_conn::HTTP_CODE http_conn::process_read() {
	LINE_STATUS lines_status = LINE_OK;
	HTTP_CODE ret = NO_REQUEST;
	char* text = 0;

	//��Ȼ��Ҫ������������һ�в��ܽ��� ��/ͷ/����  �Ľ���
	//���ﲻ����parse_line��ȡһ�У�����һ��
	while ((m_check_state == CHECK_STATE_CONTENT && lines_status == LINE_OK)
		|| ((lines_status = parse_line()) == LINE_OK)) {
		text = get_line();
		m_start_line = m_checked_idx;
		
		//������״̬��״̬���ú���
		switch (m_check_state)
		{
		case CHECK_STATE_REQUESTLINE:		//������
			ret = parse_request_line(text);
			if (ret == BAD_REQUEST)
				return BAD_REQUEST;
			break;
		case CHECK_STATE_HEADER:					//����ͷ
			ret = parse_headers(text);
			if (ret == BAD_REQUEST)
				return BAD_REQUEST;
			else if (ret == GET_REQUEST)
				return do_request();
			break;
		case CHECK_STATE_CONTENT:				//��������
			ret = parse_content(text);
			if (ret == GET_REQUEST)		//�����������HTTP����
				return do_request();
			lines_status = LINE_OPEN;
			break;
		default:
			return INTERNAL_ERROR;
			break;
		}
	}
	return NO_REQUEST;		//�����while���ѻ����������˶�û���
}

//����http�����У�������󷽷���Ŀ��url��http�汾��
http_conn::HTTP_CODE http_conn::parse_request_line(char* text) {
	//�����������Ⱥ��пո��\t��һ�ַ���λ�ò�����
	m_url = strpbrk(text, " \t");	//strpbrk����Դ�ַ�����s1�����ҳ����Ⱥ��������ַ�����s2������һ�ַ���λ�ò����أ����Ҳ����򷵻ؿ�ָ�롣
	if (!m_url) return BAD_REQUEST;
	*m_url++ = '\0';
	//ȡ�����ݣ���ͨ����GET��POST�Ƚϣ���ȷ������ʽ
	char* method = text;
	if (strcasecmp(method, "GET") == 0)		//strcasecmp�ú��Դ�Сд�Ƚ��ַ���
		m_method = GET;
	else if (strcasecmp(method, "POST") == 0) {
		m_method = POST;
		//cgi = 1;
	}
	else
		return BAD_REQUEST;
	//��ȡurl
	m_url += strspn(m_url, " \t");		//�����ַ����е�һ������ָ���ַ����г��ֵ��ַ��±�
	//��ȡversion
	m_version = strpbrk(m_url, " \t");
	if (!m_version) return BAD_REQUEST;
	*m_version++ = '\0';
	m_version += strspn(m_version, " \t");

	//��֧��HTTP/1.1
	if (strcasecmp(m_version, "HTTP/1.1") != 0)
		return BAD_REQUEST;
	
	//������Ҫ����Щ���ĵ�������Դ�л����http://��������Ҫ������������е�������
	if (strncasecmp(m_url, "http://", 7) == 0) {		//��������Դǰ7���ַ������ж�
		m_url += 7;
		m_url = strchr(m_url, '/');	//char *strchr(const char *str, int c)���ڲ���str��ָ����ַ�����������һ�γ����ַ�c��һ���޷����ַ�����λ�á�
	}

	//ͬ������https���
	if (strncasecmp(m_url, "https://", 8) == 0) {
		m_url += 8;
		m_url = strchr(m_url, '/');
	}

	//����urlȥ����http��֮�󿴿�����ĸ�ǲ���/�����ǳ���
	if (!m_url || m_url[0] != '/')
		return BAD_REQUEST;

	//���������url���ǵ�����'/'
	if (strlen(m_url) == 1)
		strcat(m_url, "judge.html");

	//����������״̬�оʹ��������
	m_check_state = CHECK_STATE_HEADER;
	return NO_REQUEST;
}

//����http������ͷ
http_conn::HTTP_CODE http_conn::parse_headers(char* text) {
	//
	if (text[0] == '\0') {
		//�ж���GET����POST����
		if (m_content_length != 0) {
			m_check_state = CHECK_STATE_CONTENT;
			return NO_REQUEST;
		}
		return GET_REQUEST;
	}
	//��������ͷ�������ֶ�
	else if (strncasecmp(text, "Connection:", 11) == 0) {
		text += 11;
		//�����ո��\t�ַ�
		text += strspn(text, " \t");
		if (strcasecmp(text, "keep-live") == 0)
			m_linger = true;
	}
	//��������ͷ�����ݳ����ֶ�
	else if (strncasecmp(text, "Content-length:", 15) == 0) {
		text += 15;
		text += strspn(text, " \t");
		m_content_length = atol(text);		//���ַ������ֱ�ɳ���������
	}
	//��������ͷ��HOST�ֶ�
	else if (strncasecmp(text, "Host:", 5) == 0) {
		text += 5;
		text += strspn(text, " \t");
		m_host = text;
	}
	else {
		//printf("oop!unkown header: %s\n", text);
	}
	return NO_REQUEST;
}

//����http��������
http_conn::HTTP_CODE http_conn::parse_content(char* text) {
	//�ж�buffer���Ƿ��ȡ����Ϣ��
	if (m_read_idx >= (m_content_length + m_checked_idx)) {
		text[m_content_length] = '\0';
		//POST���������Ϊ������û���������
		m_string = text;
		return GET_REQUEST;	//OK!�����������������ڻ��������http������
	}
	return NO_REQUEST;
}

//���������process_read�õ����������ĺ���ã�
//�����������Դ��״̬(�Ƿ����ѽȨ��ѽ)����д���ļ�·��m_real_file
http_conn::HTTP_CODE http_conn::do_request() {
	//m_real_file����������ļ�·��������ʼ����m_real_file��ֵΪ��վ��Ŀ¼
	strcpy(m_real_file, doc_root);
	int len = strlen(doc_root);

	const char* p = strrchr(m_url, '/');		//�����ַ���ָ���ַ����д����濪ʼ�ĵ�һ�γ��ֵ�λ�ã��ɹ����ظ��ַ��Լ��������ַ���ʧ�ܷ��� NULL��

	//if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3')) {
		//���ݱ�־�ж��ǵ�¼��⻹��ע����
	//}

	//������ԴΪ/0����ʾ����ע�ᣬд��m_real_file
	if (*(p + 1) == '0') {
		char* m_url_real = (char*)malloc(sizeof(char) * 200);
		strcpy(m_url_real, "/register.html");
		strncpy(m_real_file + len, m_url_real, strlen(m_url_real));	//��ʾ��src��ָ����ַ�������src��ַ��ʼ��ǰn���ֽڸ��Ƶ�dest��ָ�������У������ر����ƺ��dest��
		free(m_url_real);
	}
	//������ԴΪ/1����ʾ�����¼��д��m_real_file
	else if (*(p + 1) == '1') {
		char* m_url_real = (char*)malloc(sizeof(char) * 200);
		strcpy(m_url_real, "/log.html");
		strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
		free(m_url_real);
	}
	else if (*(p + 1) == '5')
	{
		char* m_url_real = (char*)malloc(sizeof(char) * 200);
		strcpy(m_url_real, "/picture.html");
		strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

		free(m_url_real);
	}
	else if (*(p + 1) == '6')
	{
		char* m_url_real = (char*)malloc(sizeof(char) * 200);
		strcpy(m_url_real, "/video.html");
		strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

		free(m_url_real);
	}
	else if (*(p + 1) == '7')
	{
		char* m_url_real = (char*)malloc(sizeof(char) * 200);
		strcpy(m_url_real, "/fans.html");
		strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

		free(m_url_real);
	}
	//�����ﲻ�ǵ�¼ע�ᣬ��ͨ������Դ��ֱ��ƴ�Ӽ���
	else
		strncpy(m_real_file+len,m_url,FILENAME_LEN-len-1);		//��������˼�����ƺ�m_real_life����Ϊ200


	//ͨ��stat��ȡ������Դ�ļ���Ϣ���ɹ�����Ϣ���µ�m_file_stat�ṹ��
	//ʧ�ܷ���NO_RESOURCE״̬����ʾ��Դ������
	if (stat(m_real_file, &m_file_stat) < 0)
		return NO_RESOURCE;

	//�ж��ļ���Ȩ�ޣ��Ƿ�ɶ������ɶ��򷵻�FORBIDDEN_REQUEST״̬
	if (!(m_file_stat.st_mode & S_IROTH))
		return FORBIDDEN_REQUEST;

	//�ж��ļ����ͣ������Ŀ¼���򷵻�BAD_REQUEST����ʾ����������
	if (S_ISDIR(m_file_stat.st_mode))
		return BAD_REQUEST;

	//��ֻ����ʽ��ȡ�ļ���������ͨ��mmap�����ļ�ӳ�䵽�ڴ���
	int fd = open(m_real_file, O_RDONLY);
	m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

	//������ͳɹ���
	close(fd);
	return FILE_REQUEST;		//��ʾ�����ļ����ڣ��ҿ��Է���
}





//һ��������ʽ����Ӧ����
bool http_conn::process_write(HTTP_CODE ret) {
	switch (ret)
	{
	case http_conn::NO_REQUEST:
		break;
	case http_conn::GET_REQUEST:
		break;
	//�﷨����404
	case http_conn::BAD_REQUEST:
		add_status_line(404, error_404_title);
		add_headers(strlen(error_404_form));
		if (!add_content(error_404_form))
			return false;
		break;
	case http_conn::NO_RESOURCE:
		break;
	//û�з���Ȩ�ޣ�403
	case http_conn::FORBIDDEN_REQUEST:
		add_status_line(403, error_403_title);
		add_headers(strlen(error_400_form));
		if (!add_content(error_403_form))
			return false;
		break;
	//�ļ����ڿɷ��ʣ�200
	case http_conn::FILE_REQUEST: {
			add_status_line(200, ok_200_title);
			//����������Դ����
			if (m_file_stat.st_size != 0) {
				add_headers(m_file_stat.st_size);
				//��һ��iovecָ��ָ����Ӧ���Ļ�����������ָ��m_write_idx
				m_iv[0].iov_base = m_write_buf;
				m_iv[0].iov_len = m_write_idx;
				//�ڶ���iovecָ��ָ��mmap���ص��ļ�ָ�룬����ָ���ļ���С
				m_iv[1].iov_base = m_file_address;
				m_iv[1].iov_len = m_file_stat.st_size;

				m_iv_count = 2;

				//���͵�ȫ������Ϊ��Ӧ����ͷ����Ϣ���ļ���С
				bytes_to_send = m_write_idx + m_file_stat.st_size;
				return true;
			}
			//�����������Դ��СΪ0���򷵻ؿհ�html�ļ�
			else {
				const char* ok_string = "<html><body></body></html>";
				add_headers(strlen(ok_string));
				if (!add_content(ok_string))
					return false;
			}
		}
		break;
	//�ڲ�����500
	case http_conn::INTERNAL_ERROR:
		add_status_line(500, error_500_title);		//״̬��
		add_headers(strlen(error_500_form));	//״̬ͷ
		if (!add_content(error_500_form))			//��Ϣ����
			return false;
		break;
	case http_conn::CLOSED_CONNECTION:
		break;
	default:
		return false;
	}
	//��FILE_REQUEST״̬�⣬����״ֻ̬����һ��iovec��ָ����Ӧ���Ļ�����
	m_iv[0].iov_base = m_write_buf;
	m_iv[0].iov_len = m_write_idx;
	m_iv_count = 1;
	return true;
}

//�˺����ָ���add_line,add_headers�ȵ���
//�����ǣ��ѱ���д�����������������Ӧ���ĵ������������
bool http_conn::add_response(const char* format, ...) {
	//���д�����ݳ���m_write_buf��С�򱨴�
	if (m_write_idx >= WRITE_BUFFER_SIZE)
		return false;

	//va_list�� VA_LIST ����C�����н����������һ��꣩��va_list��ʾ�ɱ�����б����ͣ�ʵ���Ͼ���һ��charָ��fmt��
	//�����÷���ٶȣ�ͦ����˼��
	va_list arg_list;

	//������arg_list��ʼ��Ϊ�������
	va_start(arg_list, format);

	//������format�ӿɱ�����б�д�뻺����д������д�����ݵĳ���
	int len=vsnprintf(m_write_buf+m_write_idx,WRITE_BUFFER_SIZE-1-m_write_idx,format,arg_list);

	//�����Ҫд������ݳ��ȳ���������ʣ��ռ䣬�򱨴�
	if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)) {
		va_end(arg_list);
		return false;
	}

	//д������������ɹ�������m_write_idxλ�ú���տɱ�����б�
	m_write_idx += len;
	va_end(arg_list);
	return true;
}

//����add_����������process_write���ã���������add_response����Ӧ����д�����������
//���״̬��
bool http_conn::add_status_line(int status, const char* title) {
	return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
//������format�ӿɱ�����б�д�뻺����д������д�����ݵĳ���
bool http_conn::add_headers(int content_len) {
	add_content_length(content_len);
	add_linger();
	add_blank_line();
}
//���Content-Length����ʾ��Ӧ���ĵĳ���
bool http_conn::add_content_length(int content_len) {
	return add_response("Content-Length:%d\r\n", content_len);
}
//����ı����ͣ�������html
bool http_conn::add_content_type() {
	return add_response("Content-Type:%s\r\n", "text/html");
}
//�������״̬��֪ͨ��������Ǳ������ӻ��ǹر�
bool http_conn::add_linger() {
	return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}
//��ӿ���
bool http_conn::add_blank_line() {
	return add_response("%s", "\r\n");
}
//����ı�content
bool http_conn::add_content(const char* content) {
	 return add_response("%s", content);
}

//process����process_write��������Ӧ����д�����������
//Ȼ��ע��дʱ�䣬дʱ�����֮�����write�����ѻ���������Ӧ���ķ��͸������
bool http_conn::write() {
	int tmp = 0;
	int newadd = 0;

	//Ҫ���͵����ݳ���Ϊ0,��Ӧ����Ϊ��
	if (bytes_to_send == 0) {
		modfd(m_epollfd, m_sockfd, EPOLLIN);
		init();
		return true;
	}

	while (true)
	{
		//����Ӧ���ĵ�״̬�С���Ϣͷ�����к���Ӧ���ķ��͸��������
		tmp = writev(m_sockfd, m_iv, m_iv_count);
		//�������ͣ�tempΪ���͵��ֽ���
		if (tmp > 0) {
			bytes_have_send += tmp;			//�����ѷ����ֽ�
			newadd = bytes_have_send - m_write_idx;	//ƫ���ļ�iovec��ָ��
		}
		//û���������ͣ���ϸ�ж�ԭ��
		else if (tmp < 0) {
			if (errno == EAGAIN) {
				//��һ��iovecͷ����Ϣ�������ѷ����꣬���͵ڶ���iovec����
				if (bytes_have_send >= m_iv[0].iov_len) {
					m_iv[0].iov_len = 0;
					m_iv[1].iov_base = m_file_address + newadd;
					m_iv[1].iov_len = bytes_to_send;
				}
				//������ǵ�һ��iovecû�����꣬��������һ��
				else {
					m_iv[0].iov_base = m_write_buf + bytes_to_send;
					m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
				}
				//����ע��д�¼�
				modfd(m_epollfd, m_sockfd, EPOLLOUT);
				return true;
			}
			//������˵��û���������Ͳ��Ҳ���EAGAIN���Ǿ��ǲ�������
			unmap();
			return false;
		}

		//�����ѷ����ֽ���
		bytes_to_send -= tmp;
		//�ܽ������˵��������ȫ��������
		if (bytes_to_send <= 0) {
			unmap();
			//��epoll��������EPOLLONESHOT�¼�
			modfd(m_epollfd, m_sockfd, EPOLLIN);

			//�ж���������Ƿ��ǳ�����
			if (m_linger) {
				init();
				return true;
			}
			else
				return false;
		}
	}
}

//ȡ��ӳ��
void http_conn::unmap() {
	if (m_file_address) {
		munmap(m_file_address, m_file_stat.st_size);
		m_file_address = 0;
	}
}