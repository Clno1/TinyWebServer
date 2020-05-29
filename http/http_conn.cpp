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


//定义http响应的一些状态信息
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file form this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the request file.\n";

//网站根目录，文件夹内存放请求的资源和跳转的html文件
const char* doc_root = "/home/clno1/projects/TinyWebServer/root";

//将表中的用户名和密码放入map
map<string, string> users;


int http_conn::m_epollfd=-1;
int http_conn::m_user_count=0;

//客户端断开连接
void http_conn::close_conn(bool real_close) {
	if (real_close && (m_sockfd != -1)) {
		removefd(m_epollfd, m_sockfd);
		m_sockfd = -1;
		m_user_count--;
	}
}
//客户端连接初始化，记录客户数据
void http_conn::init(int sockfd, const sockaddr_in& addr) {
	m_sockfd = sockfd;	m_address = addr;
	m_user_count++;
	addfd(m_epollfd, sockfd, true);		//把客户端fd添加到监听树上
	init();		//其他属性值的初始化
}
//初始化这个连接的各个数据，当响应完一次请求可以init然后重新开始新的请求
void http_conn::init() {
	mysql = NULL;
	m_check_state = CHECK_STATE_REQUESTLINE;		//主状态机初始状态：解析请求行
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

//循环读取客户数据，直到无数据可读或对方关闭连接
bool http_conn::read_once() {
	if (m_read_idx >= READ_BUFFER_SIZE) return false;		//缓冲区存不下了
	int bytes_read = 0;		//每一次recv读到多少数据
	while (true)
	{
		bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
		if (bytes_read == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) break;	//没数据读了
			return false;		//产生错误
		}
		else if (bytes_read==0) {
			//return true;
			return false;		//产生错误
		}
		m_read_idx += bytes_read;
	}
	return true;
}

//从状态机负责读取buffer中的数据，将每行数据末尾的\r\n置为\0\0，
//并更新从状态机在buffer中读取的位置m_checked_idx，以此来驱动主状态机解析。
http_conn::LINE_STATUS http_conn::parse_line() {
	char tmp;
	//从读入缓冲区中取出数据进行解析
	for (; m_checked_idx < m_read_idx; ++m_checked_idx) {
		tmp = m_read_buf[m_checked_idx];
		//如果当前是\r字符，则有可能会读取到完整行
		if (tmp == '\r') {
			if ((m_checked_idx + 1) == m_read_idx)  //当前为\r并且后面没了，继续读
				return LINE_OPEN;	
			else if (m_read_buf[m_checked_idx + 1] == '\n') {		//下一个字符是\n，读到完整行，将\r\n改为\0\0
				m_read_buf[m_checked_idx++] = '\0';
				m_read_buf[m_checked_idx++] = '\0';
				return LINE_OK;
			}
			//如果都不符合，则返回语法错误
			return LINE_BAD;
		}
		//如果当前字符是\n，也有可能读取到完整行
		else if (tmp == '\n') {
			//判断前一个字符是不是\r，是的话也是接受完整了
			if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r') {
				m_read_buf[m_checked_idx - 1] = '\0';
				m_read_buf[m_checked_idx++] = '\0';
				return LINE_OK;
			}
			//否则，语法错误
			return LINE_BAD;
		}
	}
	//看完缓冲区的数据都没有找到\r\n，继续接受数据
	return LINE_OPEN;
}


//线程抢到任务之后干什么
void http_conn::process() {
	HTTP_CODE read_ret = process_read();
	if (read_ret == NO_REQUEST) {		//请求不完整，继续读监听
		modfd(m_epollfd, m_sockfd, EPOLLIN);
		return;
	}

	bool write_ret = process_write(read_ret);	//搓响应报文
	if (!write_ret) close_conn();
	modfd(m_epollfd, m_sockfd, EPOLLOUT);
}



//请求报文
//注意这里的while怎样才能退出：要不就是坏请求，要不就是获得了完整的HTTP请求然后调用do_request
http_conn::HTTP_CODE http_conn::process_read() {
	LINE_STATUS lines_status = LINE_OK;
	HTTP_CODE ret = NO_REQUEST;
	char* text = 0;

	//当然需要解析出完整的一行才能进行 行/头/主体  的解析
	//这里不断用parse_line读取一行，解析一行
	while ((m_check_state == CHECK_STATE_CONTENT && lines_status == LINE_OK)
		|| ((lines_status = parse_line()) == LINE_OK)) {
		text = get_line();
		m_start_line = m_checked_idx;
		
		//根据主状态机状态调用函数
		switch (m_check_state)
		{
		case CHECK_STATE_REQUESTLINE:		//解析行
			ret = parse_request_line(text);
			if (ret == BAD_REQUEST)
				return BAD_REQUEST;
			break;
		case CHECK_STATE_HEADER:					//解析头
			ret = parse_headers(text);
			if (ret == BAD_REQUEST)
				return BAD_REQUEST;
			else if (ret == GET_REQUEST)
				return do_request();
			break;
		case CHECK_STATE_CONTENT:				//解析主体
			ret = parse_content(text);
			if (ret == GET_REQUEST)		//获得了完整的HTTP请求
				return do_request();
			lines_status = LINE_OPEN;
			break;
		default:
			return INTERNAL_ERROR;
			break;
		}
	}
	return NO_REQUEST;		//上面的while都把缓冲区读完了都没结果
}

//解析http请求行，获得请求方法，目标url及http版本号
http_conn::HTTP_CODE http_conn::parse_request_line(char* text) {
	//请求行中最先含有空格和\t任一字符的位置并返回
	m_url = strpbrk(text, " \t");	//strpbrk是在源字符串（s1）中找出最先含有搜索字符串（s2）中任一字符的位置并返回，若找不到则返回空指针。
	if (!m_url) return BAD_REQUEST;
	*m_url++ = '\0';
	//取出数据，并通过与GET和POST比较，以确定请求方式
	char* method = text;
	if (strcasecmp(method, "GET") == 0)		//strcasecmp用忽略大小写比较字符串
		m_method = GET;
	else if (strcasecmp(method, "POST") == 0) {
		m_method = POST;
		//cgi = 1;
	}
	else
		return BAD_REQUEST;
	//获取url
	m_url += strspn(m_url, " \t");		//返回字符串中第一个不在指定字符串中出现的字符下标
	//获取version
	m_version = strpbrk(m_url, " \t");
	if (!m_version) return BAD_REQUEST;
	*m_version++ = '\0';
	m_version += strspn(m_version, " \t");

	//仅支持HTTP/1.1
	if (strcasecmp(m_version, "HTTP/1.1") != 0)
		return BAD_REQUEST;
	
	//这里主要是有些报文的请求资源中会带有http://，这里需要对这种情况进行单独处理
	if (strncasecmp(m_url, "http://", 7) == 0) {		//对请求资源前7个字符进行判断
		m_url += 7;
		m_url = strchr(m_url, '/');	//char *strchr(const char *str, int c)，在参数str所指向的字符串中搜索第一次出现字符c（一个无符号字符）的位置。
	}

	//同样增加https情况
	if (strncasecmp(m_url, "https://", 8) == 0) {
		m_url += 8;
		m_url = strchr(m_url, '/');
	}

	//上面url去掉了http，之后看看首字母是不是/，不是出错
	if (!m_url || m_url[0] != '/')
		return BAD_REQUEST;

	//这种情况，url就是单单的'/'
	if (strlen(m_url) == 1)
		strcat(m_url, "judge.html");

	//到这里请求状态行就处理完成了
	m_check_state = CHECK_STATE_HEADER;
	return NO_REQUEST;
}

//解析http的请求头
http_conn::HTTP_CODE http_conn::parse_headers(char* text) {
	//
	if (text[0] == '\0') {
		//判断是GET还是POST请求
		if (m_content_length != 0) {
			m_check_state = CHECK_STATE_CONTENT;
			return NO_REQUEST;
		}
		return GET_REQUEST;
	}
	//解析请求头部连接字段
	else if (strncasecmp(text, "Connection:", 11) == 0) {
		text += 11;
		//跳过空格和\t字符
		text += strspn(text, " \t");
		if (strcasecmp(text, "keep-live") == 0)
			m_linger = true;
	}
	//解析请求头部内容长度字段
	else if (strncasecmp(text, "Content-length:", 15) == 0) {
		text += 15;
		text += strspn(text, " \t");
		m_content_length = atol(text);		//把字符串数字变成长整型数字
	}
	//解析请求头部HOST字段
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

//解析http请求主体
http_conn::HTTP_CODE http_conn::parse_content(char* text) {
	//判断buffer中是否读取了消息体
	if (m_read_idx >= (m_content_length + m_checked_idx)) {
		text[m_content_length] = '\0';
		//POST请求中最后为输入的用户名和密码
		m_string = text;
		return GET_REQUEST;	//OK!解析到这里我们终于获得完整的http请求报文
	}
	return NO_REQUEST;
}

//这个函数由process_read得到完整请求报文后调用，
//看看请求的资源的状态(是否存在呀权限呀)，并写好文件路径m_real_file
http_conn::HTTP_CODE http_conn::do_request() {
	//m_real_file就是请求的文件路径，将初始化的m_real_file赋值为网站根目录
	strcpy(m_real_file, doc_root);
	int len = strlen(doc_root);

	const char* p = strrchr(m_url, '/');		//查找字符在指定字符串中从右面开始的第一次出现的位置，成功返回该字符以及其后面的字符，失败返回 NULL。

	//if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3')) {
		//根据标志判断是登录检测还是注册检测
	//}

	//请求资源为/0，表示请求注册，写好m_real_file
	if (*(p + 1) == '0') {
		char* m_url_real = (char*)malloc(sizeof(char) * 200);
		strcpy(m_url_real, "/register.html");
		strncpy(m_real_file + len, m_url_real, strlen(m_url_real));	//表示把src所指向的字符串中以src地址开始的前n个字节复制到dest所指的数组中，并返回被复制后的dest。
		free(m_url_real);
	}
	//请求资源为/1，表示请求登录，写好m_real_file
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
	//到这里不是登录注册，普通请求资源，直接拼接即可
	else
		strncpy(m_real_file+len,m_url,FILENAME_LEN-len-1);		//这里有意思，复制后m_real_life长度为200


	//通过stat获取请求资源文件信息，成功则将信息更新到m_file_stat结构体
	//失败返回NO_RESOURCE状态，表示资源不存在
	if (stat(m_real_file, &m_file_stat) < 0)
		return NO_RESOURCE;

	//判断文件的权限，是否可读，不可读则返回FORBIDDEN_REQUEST状态
	if (!(m_file_stat.st_mode & S_IROTH))
		return FORBIDDEN_REQUEST;

	//判断文件类型，如果是目录，则返回BAD_REQUEST，表示请求报文有误
	if (S_ISDIR(m_file_stat.st_mode))
		return BAD_REQUEST;

	//以只读方式获取文件描述符，通过mmap将该文件映射到内存中
	int fd = open(m_real_file, O_RDONLY);
	m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

	//到这里就成功了
	close(fd);
	return FILE_REQUEST;		//表示请求文件存在，且可以访问
}





//一步步按格式搓响应报文
bool http_conn::process_write(HTTP_CODE ret) {
	switch (ret)
	{
	case http_conn::NO_REQUEST:
		break;
	case http_conn::GET_REQUEST:
		break;
	//语法错误，404
	case http_conn::BAD_REQUEST:
		add_status_line(404, error_404_title);
		add_headers(strlen(error_404_form));
		if (!add_content(error_404_form))
			return false;
		break;
	case http_conn::NO_RESOURCE:
		break;
	//没有访问权限，403
	case http_conn::FORBIDDEN_REQUEST:
		add_status_line(403, error_403_title);
		add_headers(strlen(error_400_form));
		if (!add_content(error_403_form))
			return false;
		break;
	//文件存在可访问，200
	case http_conn::FILE_REQUEST: {
			add_status_line(200, ok_200_title);
			//如果请求的资源存在
			if (m_file_stat.st_size != 0) {
				add_headers(m_file_stat.st_size);
				//第一个iovec指针指向响应报文缓冲区，长度指向m_write_idx
				m_iv[0].iov_base = m_write_buf;
				m_iv[0].iov_len = m_write_idx;
				//第二个iovec指针指向mmap返回的文件指针，长度指向文件大小
				m_iv[1].iov_base = m_file_address;
				m_iv[1].iov_len = m_file_stat.st_size;

				m_iv_count = 2;

				//发送的全部数据为响应报文头部信息和文件大小
				bytes_to_send = m_write_idx + m_file_stat.st_size;
				return true;
			}
			//否则请求的资源大小为0，则返回空白html文件
			else {
				const char* ok_string = "<html><body></body></html>";
				add_headers(strlen(ok_string));
				if (!add_content(ok_string))
					return false;
			}
		}
		break;
	//内部错误，500
	case http_conn::INTERNAL_ERROR:
		add_status_line(500, error_500_title);		//状态行
		add_headers(strlen(error_500_form));	//状态头
		if (!add_content(error_500_form))			//消息主体
			return false;
		break;
	case http_conn::CLOSED_CONNECTION:
		break;
	default:
		return false;
	}
	//除FILE_REQUEST状态外，其余状态只申请一个iovec，指向响应报文缓冲区
	m_iv[0].iov_base = m_write_buf;
	m_iv[0].iov_len = m_write_idx;
	m_iv_count = 1;
	return true;
}

//此函数又各种add_line,add_headers等调用
//作用是：把报文写到输出缓冲区（搓响应报文到输出缓冲区）
bool http_conn::add_response(const char* format, ...) {
	//如果写入内容超出m_write_buf大小则报错
	if (m_write_idx >= WRITE_BUFFER_SIZE)
		return false;

	//va_list（ VA_LIST 是在C语言中解决变参问题的一组宏）：va_list表示可变参数列表类型，实际上就是一个char指针fmt。
	//具体用法请百度，挺有意思的
	va_list arg_list;

	//将变量arg_list初始化为传入参数
	va_start(arg_list, format);

	//将数据format从可变参数列表写入缓冲区写，返回写入数据的长度
	int len=vsnprintf(m_write_buf+m_write_idx,WRITE_BUFFER_SIZE-1-m_write_idx,format,arg_list);

	//如果将要写入的数据长度超过缓冲区剩余空间，则报错
	if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)) {
		va_end(arg_list);
		return false;
	}

	//写入输出缓冲区成功，更新m_write_idx位置和清空可变参数列表
	m_write_idx += len;
	va_end(arg_list);
	return true;
}

//以下add_函数都是由process_write调用，函数调用add_response把响应报文写到输出缓冲区
//添加状态行
bool http_conn::add_status_line(int status, const char* title) {
	return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
//将数据format从可变参数列表写入缓冲区写，返回写入数据的长度
bool http_conn::add_headers(int content_len) {
	add_content_length(content_len);
	add_linger();
	add_blank_line();
}
//添加Content-Length，表示响应报文的长度
bool http_conn::add_content_length(int content_len) {
	return add_response("Content-Length:%d\r\n", content_len);
}
//添加文本类型，这里是html
bool http_conn::add_content_type() {
	return add_response("Content-Type:%s\r\n", "text/html");
}
//添加连接状态，通知浏览器端是保持连接还是关闭
bool http_conn::add_linger() {
	return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}
//添加空行
bool http_conn::add_blank_line() {
	return add_response("%s", "\r\n");
}
//添加文本content
bool http_conn::add_content(const char* content) {
	 return add_response("%s", content);
}

//process调用process_write函数把响应报文写到输出缓冲区
//然后注册写时间，写时间就绪之后调用write函数把缓冲区的响应报文发送给浏览器
bool http_conn::write() {
	int tmp = 0;
	int newadd = 0;

	//要发送的数据长度为0,响应报文为空
	if (bytes_to_send == 0) {
		modfd(m_epollfd, m_sockfd, EPOLLIN);
		init();
		return true;
	}

	while (true)
	{
		//将响应报文的状态行、消息头、空行和响应正文发送给浏览器端
		tmp = writev(m_sockfd, m_iv, m_iv_count);
		//正常发送，temp为发送的字节数
		if (tmp > 0) {
			bytes_have_send += tmp;			//更新已发送字节
			newadd = bytes_have_send - m_write_idx;	//偏移文件iovec的指针
		}
		//没有正常发送，仔细判断原因
		else if (tmp < 0) {
			if (errno == EAGAIN) {
				//第一个iovec头部信息的数据已发送完，发送第二个iovec数据
				if (bytes_have_send >= m_iv[0].iov_len) {
					m_iv[0].iov_len = 0;
					m_iv[1].iov_base = m_file_address + newadd;
					m_iv[1].iov_len = bytes_to_send;
				}
				//否则就是第一个iovec没发送完，继续发第一个
				else {
					m_iv[0].iov_base = m_write_buf + bytes_to_send;
					m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
				}
				//重新注册写事件
				modfd(m_epollfd, m_sockfd, EPOLLOUT);
				return true;
			}
			//到这里说明没有正常发送并且不是EAGAIN，那就是产生错误
			unmap();
			return false;
		}

		//更新已发送字节数
		bytes_to_send -= tmp;
		//能进入这里，说明数据已全部发送完
		if (bytes_to_send <= 0) {
			unmap();
			//在epoll树上重置EPOLLONESHOT事件
			modfd(m_epollfd, m_sockfd, EPOLLIN);

			//判断这个连接是否是长连接
			if (m_linger) {
				init();
				return true;
			}
			else
				return false;
		}
	}
}

//取消映射
void http_conn::unmap() {
	if (m_file_address) {
		munmap(m_file_address, m_file_stat.st_size);
		m_file_address = 0;
	}
}